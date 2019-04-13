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
#include <array>
#include <locale>
#include <codecvt>
#include <algorithm>
#include <windows.h>

//--------------------------------------------------------------------------------------------------

/// Supports SSEH specific errors in a manner of #GetLastError() and #FormatMessage()
static thread_local std::string sseh_error;

//--------------------------------------------------------------------------------------------------

typedef std::wstring_convert<std::codecvt_utf8_utf16<TCHAR>, TCHAR>
	convert_utf8_tchar;

static_assert (std::is_same<std::wstring::value_type, TCHAR>::value, "Not a _UNICODE build.");

/// Safe convert from UTF-8 (Skyrim) encoding to UTF-16 (Windows).

static bool
utf8_to_utf16 (char const* bytes, std::wstring& out)
{
    sseh_error.clear ();
    if (bytes) try {
        out = convert_utf8_tchar ().from_bytes (bytes);
    }
    catch (std::exception const& ex) {
        sseh_error = ex.what ();
        return false;
    }
    return true;
}

/// Safe convert from UTF-16 (Windows) encoding to UTF-8 (Skyrim).

static bool
utf16_to_utf8 (wchar_t const* wide, std::string& out)
{
    if (wide) try {
        out = convert_utf8_tchar ().to_bytes (wide);
    }
    catch (std::exception const& ex) {
        sseh_error = ex.what ();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (api) *api = ver[0];
    if (maj) *maj = ver[1];
    if (imp) *imp = ver[2];
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_last_error (size_t* size, char* message)
{
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

    if (message)
    {
        if (!*size) *message = 0;
        else *std::copy_n (m.cbegin (), std::min (*size-1, m.size ()), message) = '\0';
    }
    *size = m.size ();
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_init ()
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_uninit ()
{

}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_map_hook (const char* name, uintptr_t address)
{
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

    if (p)
    {
        static_assert (sizeof (uintptr_t) == sizeof (p), "FARPROC unconvertible to uintptr_t");
        *address = reinterpret_cast<uintptr_t> (p);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_name (uintptr_t address, size_t* size, char* name)
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_address (const char* name, uintptr_t* address)
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_hook_status (const char* name, int* enabled)
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, uintptr_t address, uintptr_t* original)
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enable_hooks ()
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_disable_hooks ()
{
    return true;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_execute (const char* command, void* arg)
{
    return true;
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
	api.hook_name     = sseh_hook_name;
	api.hook_address  = sseh_hook_address;
	api.hook_status   = sseh_hook_status;
	api.detour        = sseh_detour;
	api.enable_hooks  = sseh_enable_hooks;
	api.disable_hooks = sseh_disable_hooks;
	api.execute       = sseh_execute;
    return api;
}

//--------------------------------------------------------------------------------------------------

