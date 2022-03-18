/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "instance_settings_handler.h"

#include <multipass/constants.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/format.h>

#include <QRegularExpression>
#include <QStringList>

namespace mp = multipass;

namespace
{
constexpr auto cpus_suffix = "cpus";
constexpr auto mem_suffix = "memory";
constexpr auto disk_suffix = "disk";

enum class Operation
{
    Obtain,
    Modify
};

std::string operation_msg(Operation op)
{
    return op == Operation::Obtain ? "Cannot obtain instance settings" : "Cannot update instance settings";
}

QRegularExpression make_key_regex()
{
    auto instance_pattern = QStringLiteral("(?<instance>.+)");

    const auto prop_template = QStringLiteral("(?<property>%1)");
    auto either_prop = QStringList{cpus_suffix, mem_suffix, disk_suffix}.join("|");
    auto prop_pattern = prop_template.arg(std::move(either_prop));

    const auto key_template = QStringLiteral(R"(%1\.%2\.%3)");
    auto key_pattern = key_template.arg(mp::daemon_settings_root, std::move(instance_pattern), std::move(prop_pattern));

    return QRegularExpression{QRegularExpression::anchoredPattern(std::move(key_pattern))};
}

std::pair<std::string, std::string> parse_key(const QString& key)
{
    static const auto key_regex = make_key_regex();

    auto match = key_regex.match(key);
    if (match.hasMatch())
    {
        auto instance = match.captured("instance");
        auto property = match.captured("property");

        assert(!instance.isEmpty() && !property.isEmpty());
        return {instance.toStdString(), property.toStdString()};
    }

    throw mp::UnrecognizedSettingException{key};
}

template <typename InstanceMap>
typename InstanceMap::mapped_type&
pick_instance(InstanceMap& instances, const std::string& instance_name, Operation operation,
              const std::unordered_map<std::string, mp::VirtualMachine::ShPtr>& deleted = {})
{
    try
    {
        return instances.at(instance_name);
    }
    catch (std::out_of_range&)
    {
        const auto is_deleted = deleted.find(instance_name) != deleted.end();
        const auto reason = is_deleted ? "Instance is deleted" : "No such instance";

        throw mp::InstanceSettingsException{operation_msg(operation), instance_name, reason};
    }
}

void check_state_for_update(mp::VirtualMachine& instance)
{
    auto st = instance.current_state();
    if (st != mp::VirtualMachine::State::stopped && st != mp::VirtualMachine::State::off)
        throw mp::InstanceSettingsException{operation_msg(Operation::Modify), instance.vm_name,
                                            "Instance must be stopped for modification"};
}

mp::MemorySize get_memory_size(const QString& key, const QString& val)
{
    try
    {
        return mp::MemorySize{val.toStdString()};
    }
    catch (const mp::InvalidMemorySizeException& e)
    {
        throw mp::InvalidSettingException{key, val, e.what()};
    }
}

void update_cpus(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec)
{
    bool converted_ok = false;
    if (auto cpus = val.toInt(&converted_ok); !converted_ok || cpus < 1)
        throw mp::InvalidSettingException{key, val, "Need a positive decimal integer"};
    else if (cpus < spec.num_cores)
        throw mp::InvalidSettingException{key, val, "The number of cores can only be increased"};
    else if (cpus > spec.num_cores) // NOOP if equal
    {
        instance.update_cpus(cpus);
        spec.num_cores = cpus;
    }
}

void update_mem(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec,
                const mp::MemorySize& size)
{
    if (size < spec.mem_size)
        throw mp::InvalidSettingException{key, val, "Memory can only be expanded"};
    else if (size > spec.mem_size) // NOOP if equal
    {
        instance.resize_memory(size);
        spec.mem_size = size;
    }
}

void update_disk(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec,
                 const mp::MemorySize& size)
{
    if (size < spec.disk_space)
        throw mp::InvalidSettingException{key, val, "Disk can only be expanded"};
    else if (size > spec.disk_space) // NOOP if equal
    {
        instance.resize_disk(size);
        spec.disk_space = size;
    }
}

} // namespace

mp::InstanceSettingsException::InstanceSettingsException(const std::string& reason, const std::string& instance,
                                                         const std::string& detail)
    : SettingsException{fmt::format("{}; instance: {}; reason: {}", reason, instance, detail)}
{
}

mp::InstanceSettingsHandler::InstanceSettingsHandler(
    std::unordered_map<std::string, VMSpecs>& vm_instance_specs,
    std::unordered_map<std::string, VirtualMachine::ShPtr>& vm_instances,
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
    const std::unordered_set<std::string>& preparing_instances, std::function<void()> instance_persister)
    : vm_instance_specs{vm_instance_specs},
      vm_instances{vm_instances},
      deleted_instances{deleted_instances},
      preparing_instances{preparing_instances},
      instance_persister{std::move(instance_persister)}
{
}

std::set<QString> mp::InstanceSettingsHandler::keys() const
{
    static constexpr auto instance_placeholder = "<instance-name>"; // actual instances would bloat help text
    static const auto ret = [] {
        std::set<QString> ret;
        const auto key_template = QStringLiteral("%1.%2.%3").arg(daemon_settings_root);
        for (const auto& suffix : {cpus_suffix, mem_suffix, disk_suffix})
            ret.insert(key_template.arg(instance_placeholder).arg(suffix));

        return ret;
    }();

    return ret;
}

QString mp::InstanceSettingsHandler::get(const QString& key) const
{
    auto [instance_name, property] = parse_key(key);

    const auto& spec = find_spec(instance_name);
    if (property == cpus_suffix)
        return QString::number(spec.num_cores);
    if (property == mem_suffix)
        return QString::number(spec.mem_size.in_bytes()) + " bytes"; // TODO@ricab choose best unit

    assert(property == disk_suffix);
    return QString::number(spec.disk_space.in_bytes()) + " bytes"; // TODO@ricab choose best unit
}

void mp::InstanceSettingsHandler::set(const QString& key, const QString& val)
{
    auto [instance_name, property] = parse_key(key);

    if (preparing_instances.find(instance_name) != preparing_instances.end())
        throw InstanceSettingsException{operation_msg(Operation::Modify), instance_name, "Instance is being prepared"};

    auto& instance = modify_instance(instance_name);
    auto& spec = modify_spec(instance_name);
    check_state_for_update(instance);

    if (property == cpus_suffix)
        update_cpus(key, val, instance, spec);
    else
    {
        auto size = get_memory_size(key, val);
        if (property == mem_suffix)
            update_mem(key, val, instance, spec, size);
        else
        {
            assert(property == disk_suffix);
            update_disk(key, val, instance, spec, size);
        }
    }

    instance_persister();
}

auto mp::InstanceSettingsHandler::modify_instance(const std::string& instance_name) -> VirtualMachine&
{
    auto ret = pick_instance(vm_instances, instance_name, Operation::Modify, deleted_instances);

    assert(ret && "can't have null instance");
    return *ret;
}

auto mp::InstanceSettingsHandler::modify_spec(const std::string& instance_name) -> VMSpecs&
{
    return pick_instance(vm_instance_specs, instance_name, Operation::Modify);
}

auto mp::InstanceSettingsHandler::find_spec(const std::string& instance_name) const -> const VMSpecs&
{
    return pick_instance(vm_instance_specs, instance_name, Operation::Obtain);
}
