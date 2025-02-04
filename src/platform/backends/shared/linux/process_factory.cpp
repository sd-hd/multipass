/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "process_factory.h"
#include "basic_process.h"
#include "simple_process_spec.h"
#include <multipass/logging/log.h>
#include <multipass/process_spec.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
class AppArmoredProcess : public mp::BasicProcess
{
public:
    AppArmoredProcess(const mp::AppArmor& aa, std::unique_ptr<mp::ProcessSpec>&& spec)
        : mp::BasicProcess{std::move(spec)}, apparmor{aa}
    {
        apparmor.load_policy(process_spec->apparmor_profile().toLatin1());
    }

    void setup_child_process() final
    {
        mp::BasicProcess::setup_child_process();

        apparmor.next_exec_under_policy(process_spec->apparmor_profile_name().toLatin1());
    }

    ~AppArmoredProcess()
    {
        try
        {
            apparmor.remove_policy(process_spec->apparmor_profile().toLatin1());
        }
        catch (const std::exception& e)
        {
            // It's not considered an error when an apparmor cannot be removed
            mpl::log(mpl::Level::info, "apparmor", e.what());
        }
    }

private:
    const mp::AppArmor& apparmor;
};

mp::optional<mp::AppArmor> create_apparmor()
{
    if (qEnvironmentVariableIsSet("DISABLE_APPARMOR"))
    {
        mpl::log(mpl::Level::warning, "apparmor", "AppArmor disabled by environment variable");
        return mp::nullopt;
    }
    else if (mp::utils::get_driver_str() == "libvirt")
    {
        mpl::log(mpl::Level::info, "apparmor", "libvirt backend disables Multipass' AppArmor support");
        return mp::nullopt;
    }
    else
    {
        mpl::log(mpl::Level::info, "apparmor", "Using AppArmor support");
        return mp::AppArmor{};
    }
}
} // namespace

mp::ProcessFactory::ProcessFactory(const Singleton<ProcessFactory>::PrivatePass& pass)
    : Singleton<ProcessFactory>::Singleton{pass}, apparmor{create_apparmor()}
{
}

// This is the default ProcessFactory that creates a Process with no security mechanisms enabled
std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(std::unique_ptr<mp::ProcessSpec>&& process_spec) const
{
    if (apparmor && !process_spec->apparmor_profile().isNull())
    {
        return std::make_unique<AppArmoredProcess>(apparmor.value(), std::move(process_spec));
    }
    else
    {
        return std::make_unique<BasicProcess>(std::move(process_spec));
    }
}

std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(const QString& command,
                                                                const QStringList& arguments) const
{
    return create_process(simple_process_spec(command, arguments));
}
