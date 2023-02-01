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

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/platform.h>
#include <multipass/settings/basic_setting_spec.h>
#include <multipass/settings/bool_setting_spec.h>
#include <multipass/settings/custom_setting_spec.h>
#include <multipass/settings/persistent_settings_handler.h>
#include <multipass/settings/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <fmt/ostream.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const auto client_root = QStringLiteral("client");
const auto autostart_default = QStringLiteral("true");

QString default_hotkey()
{
    return QKeySequence{mp::hotkey_default}.toString(QKeySequence::NativeText); // outcome depends on platform
}

QString petenv_interpreter(QString val)
{
    if (!val.isEmpty() && !mp::utils::valid_hostname(val.toStdString()))
        throw mp::InvalidSettingException{mp::petenv_key, val, "Invalid hostname"};

    return val;
}

mp::ReturnCode return_code_for(const grpc::StatusCode& code)
{
    return code == grpc::StatusCode::UNAVAILABLE ? mp::ReturnCode::DaemonFail : mp::ReturnCode::CommandFail;
}

std::string message_box(const std::string& message)
{
    std::string::size_type divider_length = 50;
    {
        std::istringstream m(message);
        std::string s;
        while (getline(m, s, '\n'))
        {
            divider_length = std::max(divider_length, s.length());
        }
    }

    const auto divider = std::string(divider_length, '#');

    return '\n' + divider + '\n' + message + '\n' + divider + '\n';
}

grpc::SslCredentialsOptions get_ssl_credentials_opts_from(const QString& cert_dir_path)
{
    mp::SSLCertProvider cert_provider{cert_dir_path};
    auto opts = grpc::SslCredentialsOptions();

    opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
    opts.pem_cert_chain = cert_provider.PEM_certificate();
    opts.pem_private_key = cert_provider.PEM_signing_key();

    return opts;
}

std::shared_ptr<grpc::Channel> create_channel_and_validate(const std::string& server_address,
                                                           const grpc::SslCredentialsOptions& opts)
{
    auto rpc_channel{grpc::CreateChannel(server_address, grpc::SslCredentials(opts))};
    mp::Rpc::Stub stub{rpc_channel};

    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(100); // should be enough...
    context.set_deadline(deadline);

    mp::PingRequest request;
    mp::PingReply reply;
    auto status = stub.ping(&context, request, &reply);

    if (status.ok())
    {
        return rpc_channel;
    }
    else if (status.error_code() == grpc::StatusCode::UNAUTHENTICATED)
    {
        return nullptr;
    }
    // Throw for other error status as we don't want the client to process any further.  It will show up
    // as an "unhandled exception" in the client, but this is fine in this case since this is just covering
    // errors when the client is trying to determine the valid cert when upgrading from 1.8.
    // This whole function will be deprecated in the future.
    else
    {
        throw std::runtime_error(
            fmt::format("Error connecting to the Multipass daemon: {}\nPlease try again in a few moments.",
                        status.error_message()));
    }
}

bool client_certs_exist(const QString& cert_dir_path)
{
    QDir cert_dir{cert_dir_path};

    return cert_dir.exists(mp::client_cert_file) && cert_dir.exists(mp::client_key_file);
}

void copy_client_certs_to_common_dir(const QString& cert_dir_path, const QString& common_cert_dir_path)
{
    MP_UTILS.make_dir(common_cert_dir_path);
    QDir common_dir{common_cert_dir_path}, cert_dir{cert_dir_path};

    QFile::copy(cert_dir.filePath(mp::client_cert_file), common_dir.filePath(mp::client_cert_file));
    QFile::copy(cert_dir.filePath(mp::client_key_file), common_dir.filePath(mp::client_key_file));
}
} // namespace

mp::ReturnCode mp::cmd::standard_failure_handler_for(const std::string& command, std::ostream& cerr,
                                                     const grpc::Status& status, const std::string& error_details)
{
    fmt::print(cerr, "{} failed: {}\n{}", command, status.error_message(),
               !error_details.empty() ? fmt::format("{}\n", error_details) : "");

    return return_code_for(status.error_code());
}

bool mp::cmd::update_available(const mp::UpdateInfo& update_info)
{
    return update_info.version() != "";
}

std::string mp::cmd::update_notice(const mp::UpdateInfo& update_info)
{
    return ::message_box(fmt::format("{}\n{}\n\nGo here for more information: {}", update_info.title(),
                                     update_info.description(), update_info.url()));
}

/*
 * We make up our own file name to
 *   a) avoid unknown org/domain in path;
 *   b) keep settings-file locations consistent among daemon and client
 * Example: ${HOME}/.config/multipass/multipass.conf
 */
QString mp::client::persistent_settings_filename()
{
    static const auto file_pattern = QStringLiteral("%2%1").arg(mp::settings_extension); // note the order
    static const auto user_config_path = QDir{MP_STDPATHS.writableLocation(mp::StandardPaths::GenericConfigLocation)};
    static const auto dir_path = QDir{user_config_path.absoluteFilePath(mp::client_name)};
    static const auto path = dir_path.absoluteFilePath(file_pattern.arg(mp::client_name));

    return path;
}

void mp::client::register_global_settings_handlers()
{
    auto settings = MP_PLATFORM.extra_client_settings(); // platform settings override inserts with the same key below
    settings.insert(std::make_unique<BoolSettingSpec>(autostart_key, autostart_default));
    settings.insert(std::make_unique<CustomSettingSpec>(mp::petenv_key, petenv_default, petenv_interpreter));
    settings.insert(std::make_unique<CustomSettingSpec>(mp::hotkey_key, default_hotkey(), [](QString val) {
        return mp::platform::interpret_setting(mp::hotkey_key, val);
    }));

    MP_SETTINGS.register_handler(
        std::make_unique<PersistentSettingsHandler>(persistent_settings_filename(), std::move(settings)));
}

std::shared_ptr<grpc::Channel> mp::client::make_channel(const std::string& server_address,
                                                        mp::CertProvider* cert_provider)
{
    // No common client certificates exist yet.
    // TODO: Remove the following logic when we are comfortable all installed clients are using the common cert
    if (!cert_provider)
    {
        auto data_location{MP_STDPATHS.writableLocation(StandardPaths::GenericDataLocation)};
        auto common_client_cert_dir_path{data_location + common_client_cert_dir};

        // The following logic is for determing which certificate to use when the client starts up.
        // 1. Check if the multipass-gui certificate exists and determine if it's authenticated
        //    with the daemon already.  If it is, copy it to the common client certificate directory and use it.
        // 2. If that fails, then try the certificate from the cli client in the same manner.
        // 3. Delete any per-client certificate dirs.
        // 4. Lastly, no known certificate for the user exists, so create a new common certificate and use that.

        const std::vector<QString> cert_dirs{data_location + gui_client_cert_dir, data_location + cli_client_cert_dir};
        for (const auto& cert_dir : cert_dirs)
        {
            if (client_certs_exist(cert_dir))
            {
                if (auto rpc_channel{
                        create_channel_and_validate(server_address, get_ssl_credentials_opts_from(cert_dir))})
                {
                    copy_client_certs_to_common_dir(cert_dir, common_client_cert_dir_path);
                    mp::utils::remove_directories(cert_dirs);

                    return rpc_channel;
                }
            }
        }

        mp::utils::remove_directories(cert_dirs);
        MP_UTILS.make_dir(common_client_cert_dir_path);

        return grpc::CreateChannel(server_address,
                                   grpc::SslCredentials(get_ssl_credentials_opts_from(common_client_cert_dir_path)));
    }

    auto opts = grpc::SslCredentialsOptions();
    opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
    opts.pem_cert_chain = cert_provider->PEM_certificate();
    opts.pem_private_key = cert_provider->PEM_signing_key();

    return grpc::CreateChannel(server_address, grpc::SslCredentials(opts));
}

std::string mp::client::get_server_address()
{
    const auto address = qgetenv("MULTIPASS_SERVER_ADDRESS").toStdString();
    if (!address.empty())
    {
        mp::utils::validate_server_address(address);
        return address;
    }

    return mp::platform::default_server_address();
}

std::unique_ptr<mp::SSLCertProvider> mp::client::get_cert_provider()
{
    auto data_location{MP_STDPATHS.writableLocation(StandardPaths::GenericDataLocation)};
    auto common_client_cert_dir_path{data_location + common_client_cert_dir};

    if (client_certs_exist(common_client_cert_dir_path))
    {
        return std::make_unique<mp::SSLCertProvider>(common_client_cert_dir_path);
    }

    return nullptr;
}

void mp::client::set_logger()
{
    set_logger(mpl::Level::info);
}

void mp::client::set_logger(mpl::Level verbosity)
{
    mpl::set_logger(std::make_shared<mpl::StandardLogger>(verbosity));
}

void mp::client::pre_setup()
{
    try
    {
        platform::setup_gui_autostart_prerequisites();
    }
    catch (AutostartSetupException& e)
    {
        mpl::log(mpl::Level::error, "client", fmt::format("Failed to set up autostart prerequisites: {}", e.what()));
        mpl::log(mpl::Level::debug, "client", e.get_detail());
    }
}

void mp::client::post_setup()
{
    platform::sync_winterm_profiles();
}
