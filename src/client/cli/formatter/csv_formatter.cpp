/*
 * Copyright (C) Canonical, Ltd.
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

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/format_utils.h>
#include <multipass/format.h>

namespace mp = multipass;

namespace
{
std::string format_images(const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info,
                          std::string type)
{
    fmt::memory_buffer buf;

    for (const auto& image : images_info)
    {
        auto aliases = image.aliases_info();

        mp::format::filter_aliases(aliases);

        auto image_id = aliases[0].remote_name().empty()
                            ? aliases[0].alias()
                            : fmt::format("{}:{}", aliases[0].remote_name(), aliases[0].alias());

        fmt::format_to(std::back_inserter(buf), "{},{},{},{},{},{},{}\n", image_id, aliases[0].remote_name(),
                       fmt::join(aliases.cbegin() + 1, aliases.cend(), ";"), image.os(), image.release(),
                       image.version(), type);
    }

    return fmt::to_string(buf);
}
} // namespace

std::string mp::CSVFormatter::format(const InfoReply& reply) const
{
    fmt::memory_buffer buf;
    fmt::format_to(
        std::back_inserter(buf),
        "Name,State,Ipv4,Ipv6,Release,Image hash,Image release,Load,Disk usage,Disk total,Memory usage,Memory "
        "total,Mounts,AllIPv4,CPU(s)\n");

    for (const auto& info : format::sorted(reply.info()))
    {
        fmt::format_to(std::back_inserter(buf), "{},{},{},{},{},{},{},{},{},{},{},{},", info.name(),
                       mp::format::status_string_for(info.instance_status()), info.ipv4_size() ? info.ipv4(0) : "",
                       info.ipv6_size() ? info.ipv6(0) : "", info.current_release(), info.id(), info.image_release(),
                       info.load(), info.disk_usage(), info.disk_total(), info.memory_usage(), info.memory_total());

        auto mount_paths = info.mount_info().mount_paths();
        for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
        {
            fmt::format_to(std::back_inserter(buf), "{} => {};", mount->source_path(), mount->target_path());
        }

        fmt::format_to(std::back_inserter(buf), ",\"{}\";,{}\n", fmt::join(info.ipv4(), ","), info.cpu_count());
    }
    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const ListReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Name,State,IPv4,IPv6,Release,AllIPv4\n");

    for (const auto& instance : format::sorted(reply.instances()))
    {
        fmt::format_to(std::back_inserter(buf), "{},{},{},{},{},\"{}\"\n", instance.name(),
                       mp::format::status_string_for(instance.instance_status()),
                       instance.ipv4_size() ? instance.ipv4(0) : "", instance.ipv6_size() ? instance.ipv6(0) : "",
                       instance.current_release().empty() ? "Not Available"
                                                          : fmt::format("Ubuntu {}", instance.current_release()),
                       fmt::join(instance.ipv4(), ","));
    }

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const NetworksReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Name,Type,Description\n");

    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        // Quote the description because it can contain commas.
        fmt::format_to(std::back_inserter(buf), "{},{},\"{}\"\n", interface.name(), interface.type(),
                       interface.description());
    }

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const FindReply& reply) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Image,Remote,Aliases,OS,Release,Version,Type\n");
    fmt::format_to(std::back_inserter(buf), format_images(reply.images_info(), "Cloud Image"));
    fmt::format_to(std::back_inserter(buf), format_images(reply.blueprints_info(), "Blueprint"));

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const VersionReply& reply, const std::string& client_version) const
{
    fmt::memory_buffer buf;

    fmt::format_to(std::back_inserter(buf), "Multipass,Multipassd,Title,Description,URL\n");

    fmt::format_to(std::back_inserter(buf), "{},{},{},{},{}\n", client_version, reply.version(),
                   reply.update_info().title(), reply.update_info().description(), reply.update_info().url());

    return fmt::to_string(buf);
}

std::string mp::CSVFormatter::format(const mp::AliasDict& aliases) const
{
    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "Alias,Instance,Command,Working directory\n");

    for (const auto& elt : sort_dict(aliases))
    {
        const auto& name = elt.first;
        const auto& def = elt.second;

        fmt::format_to(std::back_inserter(buf), "{},{},{},{}\n", name, def.instance, def.command,
                       def.working_directory);
    }

    return fmt::to_string(buf);
}
