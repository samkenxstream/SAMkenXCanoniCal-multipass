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

#include "journald_logger.h"

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

namespace mpl = multipass::logging;

mpl::JournaldLogger::JournaldLogger(mpl::Level level) : LinuxLogger{level}
{
}

void mpl::JournaldLogger::log(mpl::Level level, CString category, CString message) const
{
    if (level <= logging_level)
    {
        sd_journal_send("MESSAGE=%s", message.c_str(), "PRIORITY=%i", to_syslog_priority(level), "CATEGORY=%s",
                        category.c_str(), nullptr);
    }
}
