/**
 * @file addrlib.hpp
 * @brief Interface to the Nexus Address Library project.
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
 * @ingroup Core
 *
 * @note The particular code for reading the file database is subject to the owner's license terms.
 * While it is open source, there might be additional conditions.
 *
 * @see https://www.nexusmods.com/skyrimspecialedition/mods/32444
 *
 * @details
 * Highly cut-down and tuned, apart from the file format reading, version of the publicly made
 * available header file from that project.
 */

#ifndef SSEH_ADDRLIB_HPP
#define SSEH_ADDRLIB_HPP

#include <cstdint>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <charconv>

#include <utils/winutils.hpp>

std::ofstream& log ();
//--------------------------------------------------------------------------------------------------

class address_library
{
    std::vector<std::pair<std::uint64_t, std::uint64_t>> data;
    std::vector<std::pair<std::string, std::uint64_t>> names;

	template<typename T>
	static inline T read (std::ifstream& file)
	{
		T v;
		file.read ((char*) &v, sizeof (T));
		return v;
	}

    template<typename V, typename C>
    static std::uint64_t find (V const& v, C const& c)
    {
        auto it = std::lower_bound (c.cbegin (), c.cend (), v,
                [] (auto const& kv, V const& v) { return kv.first < v; });
        if (it != c.cend () && it->first == v)
            return it->second;
        return 0;
    }

public:

    std::uintptr_t find (std::uint64_t id) const {
        return find (id, data);
    }

    std::uintptr_t find (const char* name) const
    {
        if (auto id = find_id (name); id)
            return find (id);
        return 0;
    }

    std::uint64_t find_id (std::string const& name) const {
        return find (name, names);
    }

    bool load_txt ()
    {
        names.clear ();

        std::string folder = "Data\\SKSE\\Plugins\\sse-hooks\\";
        std::vector<std::string> filenames;
        if (!enumerate_files (folder + "addrlib-names-*.txt", filenames))
            return false;

        for (auto const& fname: filenames)
        {
            std::ifstream file (folder + fname);
            if (!file.is_open ())
                return false;

            for (std::string l; std::getline (file, l); )
                if (auto n = l.find (' '); n != std::string::npos && n > 0 && n+1 < l.size ())
                    if (auto k = l.find_first_not_of (' ', n+1); k != std::string::npos)
                {
                    std::uint64_t id;
                    if (std::from_chars (&l[k], &l[l.size ()], id).ec != std::errc ())
                        continue;

                    names.emplace_back (std::string (&l[0], &l[n]), id);
                }
        }

        std::sort (names.begin (), names.end ());
        names.erase (std::unique (names.begin (), names.end ()), names.end ());
        return true;
    }

	bool load_bin (int major, int minor, int revision, int build)
	{
        data.clear ();

        std::stringstream ss;
        ss << "Data\\SKSE\\Plugins\\versionlib-"
            << major << '-' << minor << '-' << revision << '-' << build << ".bin";

		std::ifstream file (ss.str (), std::ios::binary);
		if (!file.is_open ())
			return false;

		if (int format = read<int> (file); format != 2)
			return false;

        // Version fields, unused - relying on the filename instead.
		for (int i = 0; i < 4; i++)
		    read<int> (file);

        // Unknown blob
		int unkn = read<int> (file);
		if (unkn < 0 || unkn >= 0x10000)
			return false;
		for (int i = 0; i < unkn; ++i)
            read<char> (file);

		int const ptr_size = read <int> (file);
		int const rec_size = read <int> (file);
        data.resize (rec_size);

        std::uint8_t  b1 = 0, b2 = 0;
        std::uint16_t w1 = 0, w2 = 0;
        std::uint32_t d1 = 0, d2 = 0;
        std::uint64_t q1 = 0, q2 = 0;
        std::uint64_t pvid = 0, poffset = 0, tpoffset = 0;

		for (int i = 0; i < rec_size; i++)
		{
			auto record_type = read<unsigned char> (file);
			int low = record_type & 0xF;
			int high = record_type >> 4;

			switch (low)
			{
                case 0: q1 = read<std::uint64_t> (file); break;
                case 1: q1 = pvid + 1; break;
                case 2: b1 = read<std::uint8_t > (file); q1 = pvid + b1; break;
                case 3: b1 = read<std::uint8_t > (file); q1 = pvid - b1; break;
                case 4: w1 = read<std::uint16_t> (file); q1 = pvid + w1; break;
                case 5: w1 = read<std::uint16_t> (file); q1 = pvid - w1; break;
                case 6: w1 = read<std::uint16_t> (file); q1 = w1; break;
                case 7: d1 = read<std::uint32_t> (file); q1 = d1; break;
			}

            tpoffset = poffset;
            if (high & 8)
                tpoffset /= ptr_size;

			switch (high & 7)
			{
                case 0: q2 = read<std::uint64_t> (file); break;
                case 1: q2 = tpoffset + 1; break;
                case 2: b2 = read<std::uint8_t > (file); q2 = tpoffset + b2; break;
                case 3: b2 = read<std::uint8_t > (file); q2 = tpoffset - b2; break;
                case 4: w2 = read<std::uint16_t> (file); q2 = tpoffset + w2; break;
                case 5: w2 = read<std::uint16_t> (file); q2 = tpoffset - w2; break;
                case 6: w2 = read<std::uint16_t> (file); q2 = w2; break;
                case 7: d2 = read<std::uint32_t> (file); q2 = d2; break;
			}

			if (high & 8)
				q2 *= ptr_size;

			pvid = q1;
			poffset = q2;
            data[i] = std::make_pair (q1, q2);
		}

        // Just in any case, but seems unlikely to do an actual work
        std::sort (data.begin (), data.end ());
		return true;
	}

	void dump (const std::string& path) const
	{
		if (std::ofstream f (path); f.is_open ())
            for (auto const& kv: data)
                f << std::dec << kv.first  << '\t'
                  << std::hex << kv.second << '\n';
	}
};

//--------------------------------------------------------------------------------------------------

#endif //SSEH_ADDRLIB_HPP

