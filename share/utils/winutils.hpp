/**
 * @file winutils.hpp
 * @internal
 *
 * This file is part of General Utilities project (aka Utils).
 *
 *   Utils is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Utils is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Utils If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Utilities
 *
 * @details
 * Small help functions which seems to be reused across the projects. This file is
 * dedicated to ones applicable for Windows environments.
 */

#ifndef WINUTILS_HPP
#define WINUTILS_HPP

#include <cstring>
#include <string>
#include <array>
#include <algorithm>

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA // Default is NT, cross finger ppl dont use WinXP to play Skyrim
#endif

#include <windows.h>
#include <shlobj.h>
#include <initguid.h>
#include <knownfolders.h>

//--------------------------------------------------------------------------------------------------

static_assert (std::is_same<std::wstring::value_type, TCHAR>::value, "Not an _UNICODE build.");

/// Safe convert from UTF-8 encoding to UTF-16 (Windows).

template<class T>
bool
utf8_to_utf16 (char const* bytes, T& out)
{
    out.clear ();
    if (!bytes) return true;
    int bytes_size = static_cast<int> (std::strlen (bytes));
    if (bytes_size < 1) return true;
    int sz = ::MultiByteToWideChar (CP_UTF8, 0, bytes, bytes_size, NULL, 0);
    if (sz < 1) return false;
    out.resize (sz, 0);
    ::MultiByteToWideChar (CP_UTF8, 0, bytes, bytes_size, &out[0], sz);
    return true;
}

/// Safe convert from UTF-16 (Windows) encoding to UTF-8.

template<class T>
bool
utf16_to_utf8 (wchar_t const* wide, T& out)
{
    out.clear ();
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

template<class In, typename Out>
void
copy_string (In const& src, std::size_t* n, Out* dst)
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
std::string hex_string (T v, bool shrink = true)
{
    std::array<char, sizeof (T)*2+2+1> dst;
    auto x = shrink ? int (dst.size () - 2) : 2;
    constexpr char lut[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    for (auto i = int (dst.size ()-1); i--; v >>= 4)
    {
        dst[i] = lut[v & 0xF];
        if (shrink && (v & 0xf)) x = i;
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

template<class T>
bool
known_folder_path (REFKNOWNFOLDERID rfid, T& path)
{
    PWSTR buff = nullptr;
    if (S_OK != ::SHGetKnownFolderPath (rfid, 0, nullptr, &buff))
        return false;
    bool ret = utf16_to_utf8 (buff, path);
    ::CoTaskMemFree (buff);
    return ret;
}

//--------------------------------------------------------------------------------------------------

/// ::FormatMessage for error code and convert to UTF-8
std::string format_utf8message (DWORD error_code);

/// Report as text the given windows message (e.g. WM_*) identifier
const char* window_message_text (unsigned msg);

//--------------------------------------------------------------------------------------------------

/// Including file permissions and etc. errors

template<class T>
bool file_exists (T const& name) //allow const char*
{
    std::wstring w;
    utf8_to_utf16 (name.c_str (), w);
    DWORD attr = ::GetFileAttributesW (w.c_str ());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;
    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

//--------------------------------------------------------------------------------------------------

template<class Container>
bool
enumerate_files (std::string const& wildcard, Container& out)
{
    std::wstring w;
    if (!utf8_to_utf16 (wildcard.c_str (), w))
        return false;
    out.clear ();
    WIN32_FIND_DATA fd;
    auto h = ::FindFirstFile (w.c_str (), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::string s;
        if (!utf16_to_utf8 (fd.cFileName, s))
            break;
        out.emplace_back (std::move (s));
    }
    while (::FindNextFile (h, &fd));
    auto e = ::GetLastError ();
    ::FindClose (h);
    return e == ERROR_NO_MORE_FILES;
}

//--------------------------------------------------------------------------------------------------

#endif

