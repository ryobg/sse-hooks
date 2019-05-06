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
#include <fstream>

#include <windows.h>

#include <MinHook.h>
#include <nlohmann/json.hpp>

//--------------------------------------------------------------------------------------------------

using namespace std::string_literals;

/// Shorts code below
typedef nlohmann::json::json_pointer json_pointer;

/// Supports SSEH specific errors in a manner of #GetLastError() and #FormatMessage()
static std::string sseh_error;

/// The JSON configuration database
static nlohmann::json sseh_json;

/// Remember all used Minhook profiles (for proper init, uninit sequences).
std::map<std::string, int> sseh_profiles;

/// Our hook into Minhook to allow multi-state
extern switch_globals (std::size_t);

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
    out.resize (sz, 0);
    ::MultiByteToWideChar (CP_UTF8, 0, bytes, bytes_size, &out[0], sz);
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
    out.resize (sz, 0);
    ::WideCharToMultiByte (CP_UTF8, 0, wide, wide_size, &out[0], sz, NULL, NULL);
    return true;
}

//--------------------------------------------------------------------------------------------------

/// Helper function to upload to API callers a managed range of bytes

static void
copy_string (std::string const& src, std::size_t* n, char* dst)
{
    if (!n)
        return;
    if (dst)
    {
        if (*n > 0)
            *std::copy_n (src.cbegin (), std::min (*n-1, src.size ()), dst) = '\0';
        else *dst = 0;
    }
    *n = src.size () + 1;
}

//--------------------------------------------------------------------------------------------------

/// Converts any scalar to a 0xabcde string (made for fun)

template<class T> static
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

template<class T> static inline
std::string hex_string (T* v)
{
    return hex_string (std::uintptr_t (v));
}

//--------------------------------------------------------------------------------------------------

/// SSEH uses string representation of function address

static bool
is_pointer (nlohmann::json const& json, std::uintptr_t* request = nullptr)
{
    if (json.is_string ()) try
    {
        auto v = std::stoull (json.get<std::string> (), nullptr, 0);
        if (request) *request = v;
        return true;
    }
    catch (std::exception const&)
    {
    }
    return false;
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

/// Common try/catch block used in API methods

template<class Function>
static bool
try_call (Function&& func)
{
    sseh_error.clear ();
    try
    {
        func ();
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

/// Validate the passed in configuration

static void
validate (nlohmann::json const& json)
{
    for (auto const& it: json["map"].items ())
    {
        auto const& map = it.value ();
        if (!map.contains ("target"))
            continue;

        if (!is_pointer (map["target"]))
            throw std::runtime_error ("/map/"s + it.key () + "/target is not string address");

        if (!map.contains ("detours"))
            continue;

        for (auto const& di: map["detours"].items ())
        {
            try
            {
                std::stoull (di.key (), nullptr, 0);
            }
            catch (std::exception const&)
            {
                throw std::runtime_error ("/map/"s + it.key () + "/detours/" + di.key ()
                        + " is not string address");
            }

            auto const& detour = di.value ();
            if (!detour.contains ("original") || !is_pointer (detour["original"]))
            {
                throw std::runtime_error ("/map/"s + it.key () + "/detours/" + di.key ()
                        + "/original does not exist or is not a string address");
            }
        }
    }
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
    static_assert (ver[0] == SSEH_API_VERSION, "API in files VERSION and sse-hooks.h must match");
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
    return sseh_profile ("");
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_uninit ()
{
    for (auto const& p: sseh_profiles)
    {
        switch_globals (p.second);
        if (!call_minhook (MH_Uninitialize))
        {
            sseh_error = __func__ + " MH_Uninitialize "s + sseh_error;
        }
    }
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_profile (const char* profile)
{
    auto it = sseh_profiles.find (profile);
    if (it != sseh_profiles.end ())
    {
        switch_globals (it->second);
        return true;
    }

    switch_globals (sseh_profiles.size ());
    if (!call_minhook (MH_Initialize))
    {
        sseh_error = __func__ + " MH_Initialize "s + sseh_error;
        return false;
    }
    sseh_json["profiles"][profile] = sseh_profiles.size ();
    sseh_profiles.emplace (profile, sseh_profiles.size ());
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_find_address (const char* module, const char* name, void** address)
{
    std::wstring wm;
    if (!utf8_to_utf16 (module, wm))
        return false;

    auto h = ::GetModuleHandle (wm.empty () ? nullptr : wm.c_str ());
	if (!h)
        return false;

    auto p = ::GetProcAddress (h, name);
    if (!p)
        return false;

    *address = reinterpret_cast<void*> (p);
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_load (const char* filepath)
{
    return try_call ([&]
    {
        std::ifstream fi (filepath);
        if (!fi.is_open ())
            throw std::runtime_error ("unable to read file");
        nlohmann::json j;
        fi >> j;
        validate (j);
        sseh_json.swap (j);
    });
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_map_name (const char* name, uintptr_t address)
{
    std::uintptr_t target;
    if (sseh_find_target (name, &target))
    {
        if (target == address)
            return true;
        sseh_error = __func__ + " target already different"s;
        return false;
    }

    return try_call ([&]
    {
        sseh_json["map"][name]["target"] = hex_string (address);
    });
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_find_target (const char* name, uintptr_t* target)
{
    sseh_error.clear ();
    try
    {
        auto const& json = sseh_json.at (json_pointer ("/map/"s + name + "/target"s));
        if (!is_pointer (json, target))
            throw std::runtime_error ("target not a pointer");
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
sseh_find_name (uintptr_t target, size_t* size, char* name)
{
    sseh_error.clear ();
    try
    {
        for (auto const& map: sseh_json["map"].items ())
        {
            std::uintptr_t address;
            auto const& value = map.value ();
            if (value.contains ("target")
                    && is_pointer (value["target"], &address)
                    && address == target)
            {
                copy_string (map.key (), size, name);
                return true;
            }
        }
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
    }
    return false;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, void* detour, void** original)
{
    void *target = nullptr,
         *trampoline = nullptr;

    if (const char* module = std::strchr (name, '@'))
    {
        std::string map (name, module);
        if (!sseh_find_address (++module, map.c_str (), &target))
            return false;
    }
    else
    {
        if (!sseh_find_target (name, reinterpret_cast<uintptr_t*> (&target)))
            return false;
    }

    if (!call_minhook (MH_CreateHook, target, detour, &trampoline))
    {
        sseh_error = __func__ + " MH_CreateHook "s + sseh_error;
        return false;
    }

    if (!call_minhook (MH_QueueEnableHook, target))
    {
        sseh_error = __func__ + " MH_QueueEnableHook "s + sseh_error;
        return false;
    }

    sseh_error.clear ();
    try
    {
        auto& json = sseh_json["map"][name];
        json["target"] = hex_string (target);
        json["detours"][hex_string (detour)] = {
            { "original", hex_string (trampoline) }
        };
    }
    catch (std::exception const& ex)
    {
        sseh_error = __func__ + " "s + ex.what ();
        return false;
    }

    if (original)
        *original = trampoline;
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enable (const char* name)
{
    std::uintptr_t target;
    if (!sseh_find_target (name, &target))
        return false;
    if (!call_minhook (MH_QueueEnableHook, (void*) target))
    {
        sseh_error = __func__ + " MH_QueueEnableHook "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_disable (const char* name)
{
    std::uintptr_t target;
    if (!sseh_find_target (name, &target))
        return false;
    if (!call_minhook (MH_QueueDisableHook, (void*) target))
    {
        sseh_error = __func__ + " MH_QueueDisableHook "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enable_all ()
{
    if (!call_minhook (MH_QueueEnableHook, nullptr))
    {
        sseh_error = __func__ + " MH_QueueEnableHook "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_disable_all ()
{
    if (!call_minhook (MH_QueueDisableHook, nullptr))
    {
        sseh_error = __func__ + " MH_QueueDisableHook "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_apply ()
{
    if (!call_minhook (MH_ApplyQueued))
    {
        sseh_error = __func__ + " MH_ApplyQueued "s + sseh_error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_identify (const char* pointer, size_t* size, char* json)
{
    return try_call ([&]
    {
        auto const& j = sseh_json.at (json_pointer (pointer == "/"s ? "" : pointer));
        if (size)
            copy_string (j.dump (4), size, json);
    });
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_merge_patch (const char* json)
{
    return try_call ([&]
    {
        auto j = sseh_json.patch (nlohmann::json (json));
        validate (j);
        sseh_json.swap (j);
    });
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
	api.profile      = sseh_profile;
	api.find_address = sseh_find_address;
	api.load         = sseh_load;
	api.map_name     = sseh_map_name;
	api.find_target  = sseh_find_target;
	api.find_name    = sseh_find_name;
	api.detour       = sseh_detour;
	api.enable       = sseh_enable;
	api.disable      = sseh_disable;
	api.enable_all   = sseh_enable_all;
	api.disable_all  = sseh_disable_all;
	api.apply        = sseh_apply;
	api.identify     = sseh_identify;
	api.merge_patch  = sseh_merge_patch;
	api.execute      = sseh_execute;
    return api;
}

//--------------------------------------------------------------------------------------------------

