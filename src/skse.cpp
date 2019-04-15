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

/// Small helper function to get the last error and log it (if stream is opened).

static void
log_hooks ()
{
    std::size_t n = 0;
    sseh_enum_hooks (&n, nullptr);

    if (!n)
        return;

    log () << "Dumping " << n << " hooks... " << std::endl;

    std::vector<std::uintptr_t> addresses (n);
    sseh_enum_hooks (&n, addresses.data ());

    std::vector<std::uintptr_t> detours, originals;
    std::string name, status;
    for (auto addr: addresses)
    {
        n = 0;
        if (!sseh_hook_name (addr, &n, nullptr))
        {
            log_error ();
            continue;
        }

        name.resize (++n, '\0');
        if (!sseh_hook_name (addr, &n, &name[0]))
        {
            log_error ();
            continue;
        }

        n = 0;
        if (!sseh_hook_status (name.c_str (), nullptr, &n, nullptr))
        {
            log_error ();
            continue;
        }

        int applied = 0;
        status.resize (++n, '\0');
        if (!sseh_hook_status (name.c_str (), &applied, &n, &status[0]))
        {
            log_error ();
            continue;
        }

        n = 0;
        if (!sseh_enum_detours (name.c_str (), &n, nullptr, nullptr))
        {
            log_error ();
            continue;
        }

        detours.resize (n);
        originals.resize (n);
        if (!sseh_enum_detours (name.c_str (), &n, detours.data (), originals.data ()))
        {
            log_error ();
            continue;
        }

        log () <<  "Name: " << name
            << " Addr: " << std::hex << addr << std::dec
            << " Appl: " << bool (applied)
            << " Stat: " << status
            << " Patches (" << n << "): ";
        for (size_t i = 0; i < n; ++i)
            log () << std::hex << "0x" << originals[i] << " -> 0x" << detours[i];
        log () << std::endl;
    }
}

//--------------------------------------------------------------------------------------------------

void handle_skse_message (SKSEMessagingInterface::Message* m)
{
    if (!m || m->type != SKSEMessagingInterface::kMessage_PostLoad)
        return;

    log () << "All mods reported as loaded." << std::endl;

    int api;
    sseh_version (&api, nullptr, nullptr, nullptr);
    auto data = sseh_make_api ();
    messages->Dispatch (plugin, UInt32 (api), &data, sizeof (data), nullptr);

    log () << "SSEH interface broadcasted." << std::endl;
    log_hooks ();

    if (!sseh_enable_hooks (true))
        log_error ();
    else
    {
        std::size_t n = 0;
        sseh_enum_hooks (&n, nullptr);
        log () << n << " hooks applied." << std::endl;
    }
    log_hooks ();
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

