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
#include <windows.h>

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

}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_init ()
{
    return 1;
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
    return 1;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_find_address (const char* module, const char* name, uintptr_t* address)
{
    HMODULE h;
    if (!::GetModuleHandleEx (0, module, &h))
    {
        return 0;
    }

    FARPROC p = ::GetProcAddress (h, name);
    ::FreeLibrary (h);

    if (p)
    {
        static_assert (sizeof (uintptr_t) == sizeof (p), "FARPROC unconvertible to uintptr_t");
        *address = reinterpret_cast<uintptr_t> (p);
        return 1;
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_hook_name (uintptr_t address, size_t* size, char* name)
{

}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_hook_address (const char* name, uintptr_t* address)
{

}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_hook_status (const char* name, int* enabled)
{

}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, uintptr_t address, uintptr_t* original)
{
    return 1;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_enable_hooks ()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------

SSEH_API int SSEH_CCONV
sseh_disable_hooks ()
{
    return 1;
}

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_execute (const char* command, void* arg)
{

}

//--------------------------------------------------------------------------------------------------

SSEH_API sseh_api SSEH_CCONV
sseh_make_api ()
{
    sseh_api api;
    return api;
}

//--------------------------------------------------------------------------------------------------

