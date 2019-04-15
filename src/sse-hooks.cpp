/**
 * @file sse-hooks.cpp
 * @copybrief sse-hooks.h
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
 * Implements the public API.
 */

#include <sse-hooks/sse-hooks.h>

#include <cstring>
#include <string>
#include <array>
#include <map>
#include <vector>
#include <locale>
#include <algorithm>
#include <shared_mutex>

#include <windows.h>

#include <MinHook.h>

//--------------------------------------------------------------------------------------------------

using namespace std::string_literals;

/// Supports SSEH specific errors in a manner of #GetLastError() and #FormatMessage()
static thread_local std::string sseh_error;

//--------------------------------------------------------------------------------------------------

/// Describes a patched function as used by SSEH
struct hook
{
    /// Are the patches applied?
    bool applied;
    /// A (error mostly) status in human-readable form
    std::string status;
    /// Name of the hook, case sensitive, unique.
    std::string name;
    /// The target address which should be or is already patched, unique.
    std::uintptr_t target;
    /// Describe one patch request for that #target
    struct patch {
        /// The address of the function to jump to, when the target is patched.
        std::uintptr_t detour;
        /// The address of trampoline function to use to call the original (or previous) function.
        std::uintptr_t original;
    };
    /// Patches as they have been requested from SSEH.
    std::vector<patch> patches;
};

/// All the hooks registered in SSEH.
static std::vector<hook> hooks;

/// Enable lookup of hook by its name.
static std::map<std::string, std::size_t> hook_names;

/// Enable lookup of hook by its address.
static std::map<std::uintptr_t, std::size_t> hook_addresses;

/// Lock the access to the global storage (hook* vars)
static std::shared_timed_mutex hooks_mutex;

//--------------------------------------------------------------------------------------------------

static_assert (std::is_same<std::wstring::value_type, TCHAR>::value, "Not an _UNICODE build.");

/// Safe convert from UTF-8 (Skyrim) encoding to UTF-16 (Windows).

static bool
utf8_to_utf16 (char const* bytes, std::wstring& out)
{
    sseh_error.clear ();
    if (!bytes) return true;
    int bytes_size = static_cast<int> (std::strlen (bytes));
    if (bytes_size < 1) return true;
    int sz = ::MultiByteToWideChar (CP_UTF8, 0, bytes, bytes_size, NULL, 0);
    if (sz < 1) return false;
    std::wstring ws (sz, 0);
    ::MultiByteToWideChar (CP_UTF8, 0, bytes, bytes_size, &ws[0], sz);
    return true;
}

/// Safe convert from UTF-16 (Windows) encoding to UTF-8 (Skyrim).

static bool
utf16_to_utf8 (wchar_t const* wide, std::string& out)
{
    sseh_error.clear ();
    if (!wide) return true;
    int wide_size = static_cast<int> (std::wcslen (wide));
    if (wide_size < 1) return true;
    int sz = ::WideCharToMultiByte (CP_UTF8, 0, wide, wide_size, NULL, 0, NULL, NULL);
    if (sz < 1) return false;
    std::string s (sz, 0);
    ::WideCharToMultiByte (CP_UTF8, 0, wide, wide_size, &s[0], sz, NULL, NULL);
    return true;
}

//--------------------------------------------------------------------------------------------------

/// Helper function to upload to API callers a managed range of bytes

static void
copy_string (std::string& src, std::size_t* n, char* dst)
{
    if (dst)
    {
        if (*n > 0)
            *std::copy_n (src.cbegin (), std::min (*n-1, src.size ()), dst) = '\0';
        else *dst = 0;
    }
    *n = src.size ();
}

//--------------------------------------------------------------------------------------------------

/// Cautious call to one of the MinHook library functions.

template<class Function, class... Args>
static bool
call_minhook (Function&& func, Args&&... args)
{
    sseh_error.clear ();
    MH_STATUS status;

    try
    {
        status = func (std::forward<Args> (args)...);
    }
    catch (std::exception const& ex)
    {
        sseh_error = ex.what ();
        return false;
    }

    if (status != MH_OK)
    {
        sseh_error = MH_StatusToString (status);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp, const char** build)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (api) *api = ver[0];
    if (maj) *maj = ver[1];
    if (imp) *imp = ver[2];
    if (build) *build = SSEH_TIMESTAMP; //"2019-04-15T08:37:11.419416+00:00"
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_last_error (size_t* size, char* message)
{
    if (sseh_error.size ())
    {
        copy_string (sseh_error, size, message);
        return;
    }

	auto err = ::GetLastError ();
	if (!err)
    {
        *size = 0;
        if (message) *message = 0;
        return;
    }

    LPTSTR buff = nullptr;
    FormatMessage (
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &buff, 0, nullptr);

    std::string m;
    if (!utf16_to_utf8 (buff, m))
    {
        ::LocalFree (buff);
        return;
    }
    ::LocalFree (buff);

    copy_string (m, size, message);
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_init ()
{
    if (!call_minhook (MH_Initialize))
    {
        sseh_error = __func__ + " MH_Initialize "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_uninit ()
{
    if (!call_minhook (MH_Uninitialize))
    {
        sseh_error = __func__ + " MH_Uninitialize "s + sseh_error;
    }
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_map_hook (const char* name, uintptr_t address)
{
    sseh_error.clear ();
    std::lock_guard<std::shared_timed_mutex> lock (hooks_mutex);

    if (hook_names.count (name))
    {
        sseh_error = __func__ + " name already exists"s;
        return false;
    }

    if (hook_addresses.count (address))
    {
        sseh_error = __func__ + " address already exists"s;
        return false;
    }

    hook h    = {};
    h.applied = false;
    h.status  = "no detour";
    h.name    = name;
    h.target  = address;

    hooks.push_back (h);
    hook_names[name] = hooks.size () - 1;
    hook_addresses[address] = hooks.size () - 1;

    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_find_address (const char* module, const char* name, uintptr_t* address)
{
    HMODULE h;
    std::wstring wm;

    if (!utf8_to_utf16 (module, wm))
        return false;

    if (!::GetModuleHandleEx (0, wm.empty () ? nullptr : wm.data (), &h))
        return false;

    auto p = ::GetProcAddress (h, name);

    ::FreeLibrary (h);

    if (!p)
    {
        sseh_error = __func__ + " procedure not found"s;
        return false;
    }

    static_assert (sizeof (uintptr_t) == sizeof (p), "FARPROC unconvertible to uintptr_t");
    *address = reinterpret_cast<uintptr_t> (p);
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_enum_hooks (size_t* size, uintptr_t* addresses)
{
    std::shared_lock<std::shared_timed_mutex> lock (hooks_mutex);

    if (addresses)
        for (size_t i = 0, n = std::min (*size, hooks.size ()); i < n; ++i)
        {
             *addresses++ = hooks[i].target;
        }

    *size = hooks.size ();
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_name (uintptr_t address, size_t* size, char* name)
{
    sseh_error.clear ();
    std::shared_lock<std::shared_timed_mutex> lock (hooks_mutex);

    auto it = hook_addresses.find (address);
    if (it == hook_addresses.end ())
    {
        sseh_error = __func__ + " hook address not found"s;
        return false;
    }

    copy_string (hooks.at (it->second).name, size, name);
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_address (const char* name, uintptr_t* address)
{
    sseh_error.clear ();
    std::shared_lock<std::shared_timed_mutex> lock (hooks_mutex);

    auto it = hook_names.find (name);
    if (it == hook_names.end ())
    {
        sseh_error = __func__ + " hook name not found"s;
        return false;
    }

    *address = hooks.at (it->second).target;
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_status (const char* name, int* applied, size_t* size, char* status)
{
    sseh_error.clear ();
    std::shared_lock<std::shared_timed_mutex> lock (hooks_mutex);

    auto it = hook_names.find (name);
    if (it == hook_names.end ())
    {
        sseh_error = __func__ + " hook name not found"s;
        return false;
    }

    hook& h = hooks.at (it->second);
    if (applied) *applied = h.applied;
    if (size) copy_string (h.status, size, status);
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, uintptr_t address, uintptr_t* original)
{
    sseh_error.clear ();
    std::lock_guard<std::shared_timed_mutex> lock (hooks_mutex);

    auto it = hook_names.find (name);
    if (it == hook_names.end ())
    {
        sseh_error = __func__ + " hook name not found"s;
        return false;
    }

    hook& h = hooks.at (it->second);
    for (auto const& p: h.patches)
    {
        if (p.detour == address)
        {
            sseh_error = __func__ + " detour already exists"s;
            return false;
        }
    }

    LPVOID trampoline;
    auto target = reinterpret_cast<LPVOID> (h.target);
    auto detour = reinterpret_cast<LPVOID> (address);

    if (!call_minhook (MH_CreateHook, target, detour, &trampoline))
    {
        sseh_error = __func__ + " MH_CreateHook "s + sseh_error;
        h.status = sseh_error;
        return false;
    }

    hook::patch p;
    p.detour = address;
    p.original = reinterpret_cast<uintptr_t> (trampoline);
    h.patches.push_back (p);
    h.status = "detoured";

    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enum_detours (const char* name, size_t* size, uintptr_t* detours, uintptr_t* originals)
{
    sseh_error.clear ();
    std::shared_lock<std::shared_timed_mutex> lock (hooks_mutex);

    auto it = hook_names.find (name);
    if (it == hook_names.end ())
    {
        sseh_error = __func__ + " hook name not found"s;
        return false;
    }

    auto const& h = hooks.at (it->second);
    if (detours || originals)
        for (size_t i = 0, n = std::min (*size, h.patches.size ()); i < n; ++i)
        {
            if (detours) *detours++ = h.patches[i].detour;
            if (originals) *originals++ = h.patches[i].original;
        }
    *size = h.patches.size ();

    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enable_hooks (int apply)
{
    std::lock_guard<std::shared_timed_mutex> lock (hooks_mutex);

    if (!call_minhook (MH_QueueHook, LPVOID (MH_ALL_HOOKS), BOOL (apply)))
    {
        sseh_error = __func__ + " MH_QueueEnableHook "s + sseh_error;
        return false;
    }

    auto logger = [&] (LPVOID target, MH_STATUS status)
    {
        auto it = hook_addresses.find (reinterpret_cast<std::uintptr_t> (target));
        if (it == hook_addresses.end ())
        {
            sseh_error = __func__ + " non-managed hook detected"s;
            return;
        }

        auto& h = hooks[it->second];

        if (status == MH_OK)
        {
            h.applied = true;
            h.status = "applied";
            return;
        }

        h.status = "MH_ApplyQueued "s + MH_StatusToString (status);
    };

    if (!call_minhook (MH_ApplyQueued, logger))
    {
        sseh_error = __func__ + " MH_QueueEnableHook "s + sseh_error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_execute (const char* command, void* arg)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

SSEH_API sseh_api SSEH_CCONV
sseh_make_api ()
{
    sseh_api api      = {};
    api.next          = 0;
	api.version       = sseh_version;
	api.last_error    = sseh_last_error;
	api.init          = sseh_init;
	api.uninit        = sseh_uninit;
	api.map_hook      = sseh_map_hook;
	api.find_address  = sseh_find_address;
	api.enum_hooks    = sseh_enum_hooks;
	api.hook_name     = sseh_hook_name;
	api.hook_address  = sseh_hook_address;
	api.hook_status   = sseh_hook_status;
	api.detour        = sseh_detour;
	api.enum_detours  = sseh_enum_detours;
	api.enable_hooks  = sseh_enable_hooks;
	api.execute       = sseh_execute;
    return api;
}

//--------------------------------------------------------------------------------------------------

