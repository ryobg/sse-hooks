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
#include <fstream>

#include <windows.h>

#include <MinHook.h>
#include <nlohmann/json.hpp>

//--------------------------------------------------------------------------------------------------

using namespace std::string_literals;

/// Supports SSEH specific errors in a manner of #GetLastError() and #FormatMessage()
static thread_local std::string sseh_error;

/// The JSON configuration database
static nlohmann::json sseh_json;

/// Lock the access to the global storage (hook* vars)
static std::shared_timed_mutex json_mutex;

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

/// Validate, insert and update missing indices before assigning to #sseh_json

static void
assign_config (nlohmann::json&& json)
{
    std::lock_guard<std::shared_timed_mutex> lock (json_mutex);
    sseh_json.swap (json);
}

//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------

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

SSEH_API int SSEH_CCONV
sseh_load (const char* filepath)
{
    sseh_error.clear ();
    try
    {
        std::ifstream fi (filepath);
        nlohmann::json j;
        fi >> j;
        assign_config (std::move (j));
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_identify (const char* pointer, size_t* size, char* json)
{
    std::string v;
    sseh_error.clear ();
    try
    {
        decltype (""_json_pointer) p (pointer);
        std::shared_lock<std::shared_timed_mutex> lock (json_mutex);
        auto const& j = sseh_json.at (p);
        if (size)
            v = j.dump ();
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }
    if (size)
        copy_string (v, size, json);
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_merge_patch (const char* json)
{
    sseh_error.clear ();
    try
    {
        assign_config (sseh_json.patch (nlohmann::json (json)));
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_detour (const char* module, const char* hook, const char* patch, uintptr_t detour)
{
    sseh_error.clear ();
    try
    {
        auto h = "/hooks"s + hook;
        if (sseh_json.find (h) == sseh_json.end ())
        {
        }
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_original (const char* hook, const char* patch, uintptr_t* original)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_apply ()
{
    return false;
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
    sseh_api api     = {};
	api.version      = sseh_version;
	api.last_error   = sseh_last_error;
	api.init         = sseh_init;
	api.uninit       = sseh_uninit;
	api.find_address = sseh_find_address;
	api.load         = sseh_load;
	api.identify     = sseh_identify;
	api.merge_patch  = sseh_merge_patch;
	api.apply        = sseh_apply;
	api.execute      = sseh_execute;
    return api;
}

//--------------------------------------------------------------------------------------------------

