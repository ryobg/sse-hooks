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

/// Converts any fundemantal integer to a 0xabcde (made for fun)

template<class T>
std::string hex_string (T v)
{
    std::array<char, sizeof (T)*2+2+1> dst;
    auto x = int (dst.size () - 2);
    constexpr char lut[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    for (auto i = int (dst.size ()-1); i--; v >>= 4)
    {
        dst[i] = lut[v & 0xF];
        if (v & 0xf) x = i;
    }
    dst.back () = '\0';
    dst[x-1] = 'x';
    dst[x-2] = '0';
    return dst.data () + x - 2;
}

//--------------------------------------------------------------------------------------------------

/// As SSEH supports both string and numerical representation of function address

static bool
is_pointer (nlohmann::json const& json, std::uintptr_t* request = nullptr)
{
    try
    {
        std::uintptr_t v;
        if (json.is_string ())
        {
            v = std::stoull (json.get<std::string> (), nullptr, 0);
        }
        else if (json.is_number ())
        {
            v = json.get<uintptr_t> ();
        }
        else
        {
            return false;
        }
        if (request) *request = v;
        return true;
    }
    catch (std::exception const&)
    {
        return false;
    }
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
    std::vector<std::string> invalid_patches;
    for (auto& hook: json["hooks"])
    {
        std::string module;
        if (hook.find ("module") == hook.end () || !hook["module"].is_string ())
            hook["module"] = module;
        else module = hook["module"].get<std::string> ();

        std::string target;
        if (hook.find ("target") == hook.end () || !is_pointer (hook["target"]))
            hook["target"] = target;
        else target = hook["target"].get<std::string> ();

        if (target.empty ())
        {
            uintptr_t address;
            if (!sseh_find_address (module.c_str (), target.c_str (), &address));
            {
                std::size_t n;
                sseh_last_error (&n, nullptr);
                std::string err (n, '\0');
                sseh_last_error (&n, &err[0]);
                throw std::runtime_error (err);
            }
            hook["target"] = hex_string (address);
        }

        if (hook.find ("applied") == hook.end () || !hook["applied"].is_boolean ())
            hook["applied"] = false;

        if (hook.find ("status") == hook.end () || !hook["status"].is_string ())
            hook["status"] = "new";

        if (hook.find ("patches") == hook.end () || !hook["patches"].is_object ())
            hook["patches"] = std::map<std::string, std::string> ();

        invalid_patches.clear ();
        auto& patches = hook["patches"];
        for (auto const& patch: patches.items ())
        {
            auto const& value = patch.value ();
            if (value.find ("detour") == value.end () || !is_pointer (value["detour"]))
                invalid_patches.push_back (patch.key ());
        }
        for (auto const& name: invalid_patches)
            patches.erase (name);
    }
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
        decltype (""_json_pointer) p ("/hooks/"s + hook);
        std::lock_guard<std::shared_timed_mutex> lock (json_mutex);
        if (sseh_json.find (p) == sseh_json.end ())
        {
            sseh_json[p] = {
                { "module", module },
                { "target", "" },
                { "patches", {
                    { patch, {{ "detour", hex_string (detour) }}}
                }}
            };
        }
        else
        {
            decltype (""_json_pointer) g ("/hooks/"s + hook + "/patches/"s + patch + "/detour"s);
            sseh_json[g] = hex_string (detour);
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
    sseh_error.clear ();
    try
    {
        std::string address ("/hooks/"s + hook + "/patches/"s + patch + "/original"s);
        decltype (""_json_pointer) p (address);
        std::shared_lock<std::shared_timed_mutex> lock (json_mutex);
        if (is_pointer (sseh_json.at (p), original))
        {
            return true;
        }
        else sseh_error = __func__ + address + " is not a number or string address";
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
    }
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

