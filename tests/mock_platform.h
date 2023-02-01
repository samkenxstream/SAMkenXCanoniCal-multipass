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

#ifndef MULTIPASS_MOCK_PLATFORM_H
#define MULTIPASS_MOCK_PLATFORM_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/platform.h>

namespace multipass::test
{
class MockPlatform : public platform::Platform
{
public:
    MockPlatform(const PrivatePass& pass) : platform::Platform(pass)
    {
        EXPECT_CALL(*this, set_server_socket_restrictions)
            .Times(testing::AnyNumber())
            .WillRepeatedly(testing::Return());
    };

    MOCK_CONST_METHOD0(get_network_interfaces_info, std::map<std::string, NetworkInterfaceInfo>());
    MOCK_CONST_METHOD0(get_blueprints_url_override, QString());
    MOCK_CONST_METHOD1(is_remote_supported, bool(const std::string&));
    MOCK_CONST_METHOD1(is_backend_supported, bool(const QString&));
    MOCK_CONST_METHOD2(is_alias_supported, bool(const std::string&, const std::string&));
    MOCK_CONST_METHOD2(chmod, int(const char*, unsigned int));
    MOCK_CONST_METHOD3(chown, int(const char*, unsigned int, unsigned int));
    MOCK_CONST_METHOD2(link, bool(const char*, const char*));
    MOCK_CONST_METHOD3(symlink, bool(const char*, const char*, bool));
    MOCK_CONST_METHOD3(utime, int(const char*, int, int));
    MOCK_CONST_METHOD2(create_alias_script, void(const std::string&, const AliasDefinition&));
    MOCK_CONST_METHOD1(remove_alias_script, void(const std::string&));
    MOCK_CONST_METHOD2(set_server_socket_restrictions, void(const std::string&, const bool));
    MOCK_CONST_METHOD0(multipass_storage_location, QString());
    MOCK_CONST_METHOD0(extra_daemon_settings, SettingSpec::Set());
    MOCK_CONST_METHOD0(extra_client_settings, SettingSpec::Set());
    MOCK_CONST_METHOD0(daemon_config_home, QString());
    MOCK_CONST_METHOD0(default_driver, QString());
    MOCK_CONST_METHOD0(default_privileged_mounts, QString());

    MP_MOCK_SINGLETON_BOILERPLATE(MockPlatform, Platform);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_PLATFORM_H
