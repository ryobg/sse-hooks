/**
 * @file skse.cpp
 * @brief Implementing SSEH as plugin for SKSE
 * @internal
 *
 * This file is part of SSE Hooks project (aka SSEH).
 *
 *   SSEH is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   SSEH is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with SSEH. If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Public API
 *
 * @details
 * This file only depends on one header file provided by SKSE. As it looks, that plugin interface
 * does not change much, if at all. Hence, this means there is stable interface which can make SSEH
 * version independent of SKSE. This file by itself is standalone enough so it can be a separate DLL
 * project - binding SSEH to SKSE.
 */

#include <sse-hooks/sse-hooks.h>

#include <cstdint>
typedef std::uint32_t UInt32;
typedef std::uint64_t UInt64;
#include <skse/PluginAPI.h>

#include <vector>
#include <chrono>
#include <fstream>
#include <iomanip>

//--------------------------------------------------------------------------------------------------

/// Given by SKSE to uniquely identify this DLL
PluginHandle plugin = 0;

/// To send events to the other plugins.
SKSEMessagingInterface* messages = nullptr;

/// Log file in pre-defined location
static std::ofstream logfile;

//--------------------------------------------------------------------------------------------------

/// TODO: Better location

static void open_log ()
{
    logfile.open ("sseh.log");
}

//--------------------------------------------------------------------------------------------------

static decltype(logfile)&
log ()
{
    // MinGW 4.9.1 have no std::put_time()
	using std::chrono::system_clock;
	auto now_c = system_clock::to_time_t (system_clock::now ());
	auto loc_c = std::localtime (&now_c);
    logfile << '['
            << 1900 + loc_c->tm_year
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mon
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mday
            << ' ' << std::setw (2) << std::setfill ('0') << loc_c->tm_hour
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_min
        << "] ";
    return logfile;
}

//--------------------------------------------------------------------------------------------------

/// Frequent scenario to get the last error and log it

static void
log_error ()
{
    size_t n = 0;
    sseh_last_error (&n, nullptr);
    if (n)
    {
        std::string s (n+1, '\0');
        sseh_last_error (&n, &s[0]);
        log () << s << std::endl;
    }
}

//--------------------------------------------------------------------------------------------------

void handle_skse_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SKSEMessagingInterface::kMessage_PostPostLoad)
        return;

    log () << "All mods reported as loaded and listening." << std::endl;

    int api;
    sseh_version (&api, nullptr, nullptr, nullptr);
    auto data = sseh_make_api ();
    messages->Dispatch (plugin, UInt32 (api), &data, sizeof (data), nullptr);

    log () << "SSEH interface broadcasted." << std::endl;

    if (!sseh_apply ())
    {
        log_error ();
        return;
    }
    log () << "Applied." << std::endl;

    std::size_t n;
    if (sseh_identify ("/", &n, nullptr))
    {
        std::string s (n, '\0');
        sseh_identify ("/", &n, &s[0]);
        log () << s << std::endl;
    }

    messages->Dispatch (plugin, 0, nullptr, 0, nullptr);
    log () << "All done." << std::endl;
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" SSEH_API bool SSEH_CCONV
SKSEPlugin_Query (SKSEInterface const* skse, PluginInfo* info)
{
    int api;
    sseh_version (&api, nullptr, nullptr, nullptr);

    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "SSEH";
    info->version = api;

    plugin = skse->GetPluginHandle ();

    if (skse->isEditor)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" SSEH_API bool SSEH_CCONV
SKSEPlugin_Load (SKSEInterface const* skse)
{
    open_log ();

    messages = (SKSEMessagingInterface*) skse->QueryInterface (kInterface_Messaging);
    if (!messages)
        return false;

    messages->RegisterListener (plugin, "SKSE", handle_skse_message);

    int a, m, p;
    const char* b;
    sseh_version (&a, &m, &p, &b);
    log () << "SSEH "<< a <<'.'<< m <<'.'<< p <<" ("<< b <<')' << std::endl;

    int r = sseh_init ();

    if (r) log () << "Initialized." << std::endl;
    else log_error ();

    return r;
}

//--------------------------------------------------------------------------------------------------

