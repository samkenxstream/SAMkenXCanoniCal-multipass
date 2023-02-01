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

#include "delete.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Delete::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::DeleteReply& reply) {
        auto size = reply.purged_instances_size();
        for (auto i = 0; i < size; ++i)
        {
            const auto removed_aliases = aliases.remove_aliases_for_instance(reply.purged_instances(i));

            for (const auto& removed_alias : removed_aliases)
            {
                try
                {
                    MP_PLATFORM.remove_alias_script(removed_alias);
                }
                catch (const std::runtime_error& e)
                {
                    cerr << fmt::format("Warning: '{}' when removing alias script for {}\n", e.what(), removed_alias);
                }
            }
        }

        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::delet, request, on_success, on_failure);
}

std::string cmd::Delete::name() const
{
    return "delete";
}

QString cmd::Delete::short_help() const
{
    return QStringLiteral("Delete instances");
}

QString cmd::Delete::description() const
{
    return QStringLiteral("Delete instances, to be purged with the \"purge\" command,\n"
                          "or recovered with the \"recover\" command.");
}

mp::ParseCode cmd::Delete::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to delete", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Delete all instances");
    parser->addOption(all_option);

    QCommandLineOption purge_option({"p", "purge"}, "Purge instances immediately");
    parser->addOption(purge_option);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser));

    if (parser->isSet(purge_option))
    {
        request.set_purge(true);
    }
    return status;
}
