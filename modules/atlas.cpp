#ifdef ENABLE_ATLAS

#include "atlas.h"
#include "common.h"
#include "../RipperFormats.h"
#include "../utils/DatManip.h"

#include <vector>
#include <fstream>
#include <string>

#include <iostream>

using namespace RipUtil;
using namespace RipperFormats;

namespace Atlas
{

	// needs a stronger test
	bool AtlasRip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat)
	{
		if (stream.get_fsize() < 68)
			return false;

		stream.seekg(0);
		int version = stream.read_int(4, DatManip::le);
		if (version > 4)
			return false;

		stream.seekg(12);
		int tabl1addr = stream.read_int(4, DatManip::le);
		int tabl1entries = stream.read_int(4, DatManip::le);
//		if (tabl1addr <= 24 || tabl1addr > stream.get_fsize() - 24)
//			return false;
		int tabl2addr = stream.read_int(4, DatManip::le);
		int tabl2entries = stream.read_int(4, DatManip::le);
//		if (tabl2addr <= 24 || tabl2addr > stream.get_fsize() - 24)
//			return false;
		stream.seekg(60);
		int tabl3addr = stream.read_int(4, DatManip::le);
		int tabl3entries = stream.read_int(4, DatManip::le);

		
		if (!(tabl2addr < 68 || tabl2addr > stream.get_fsize()))
		{
			stream.seekg(tabl2addr);
			int entry1addr = stream.read_int(4, DatManip::le);
			int entry1len = stream.read_int(4, DatManip::le);
			if (entry1addr >= 0 && entry1len >= 0
				&& entry1addr < stream.get_fsize() && entry1len <= stream.get_fsize() - 68)
			{
				if (version == 2)
					formatsubtype = fmt_res_type2;
				else if (version == 4)
					formatsubtype = fmt_res_type4;
				else
					formatsubtype = fmt_unknown_addr14;
				addrtableaddr = 20;
				return true;
			}
		}

		if (!(tabl3addr < 68 || tabl3addr > stream.get_fsize()))
		{
			stream.seekg(tabl3addr);
			int entry2addr = stream.read_int(4, DatManip::le);
			int entry2len = stream.read_int(4, DatManip::le);
			if (entry2addr >= 68 && entry2len >= 0
				&& entry2addr < stream.get_fsize() && entry2len <= stream.get_fsize() - 68)
			{
				if (version == 3)
					formatsubtype = fmt_res_type3;
				else
					formatsubtype = fmt_unknown_addr3C;
				addrtableaddr = 60;
				return true;
			}
		}

		return false;
	}

	RipperFormats::RipResults AtlasRip::rip(RipUtil::MembufStream& stream, const std::string& fprefix, 
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
	{
		check_params(ripset);

		RipResults results;

		stream.seekg(addrtableaddr);
		int addrtable = stream.read_int(4, DatManip::le);
		int addrtableentries = stream.read_int(4, DatManip::le);
		stream.seekg(addrtable);
//		printf("%d\n", stream.tellg());
//		stream.seek_off(8);
		// most games, but not all, start with a null entry
		int firstend = stream.read_int(4, DatManip::le);
		int firstlen = stream.read_int(4, DatManip::le);
		if (firstend != 0)
			stream.seek_off(-8);
		std::vector<AtlasAddrTableEntry> entries;
		int entriesread = 0;
		while (!stream.eof() && entriesread < addrtableentries)
		{
			AtlasAddrTableEntry entry;
			entry.address = stream.read_int(4, DatManip::le);
			entry.length = stream.read_int(4, DatManip::le);
			// some games just don't know when to quit
			if (entriesread > 0 && (entry.address < 68 
				|| entry.address > stream.get_fsize()
				|| entry.address + entry.length > stream.get_fsize()))
				break;
			entries.push_back(entry);
			++entriesread;
//			printf("%d %d\n", entry.address, entry.length);
//			char c;
//			std::cin >> c;
		}
		stream.clear();
//		printf("%d\n", entries.size());
		int files_ripped = 0;

		if (ripset.ripallraw)
		{
			for (int i = 0; i < entries.size(); i++)
			{
				stream.seekg(entries[i].address);
				char* outbytes = new char[entries[i].length];
				stream.read(outbytes, entries[i].length);
				std::ofstream ofs((fprefix + "-data-"
					+ to_string(i)).c_str(), std::ios_base::binary);
				ofs.write(outbytes, entries[i].length);
				delete[] outbytes;
				++results.data_ripped;
			}
			return results;
		}

		BitmapData lastbmap;
		bool lastbmap_ripped = false;
		int lastbmapnum = -2;
		
		char hdcheck[4];
		for (int i = 0; i < entries.size(); i++)
		{
			stream.seekg(entries[i].address);
			stream.read(hdcheck, 4);
			if (quickcmp(hdcheck, "BM", 2))
			{
				if (ripset.ripgraphics)
				{
					if (copybmp)
					{
						stream.seekg(entries[i].address);
						char* outbytes = new char[entries[i].length];
						stream.read(outbytes, entries[i].length);

						std::ofstream ofs((fprefix + "-bmp-"
							+ to_string(files_ripped) + ".bmp").c_str(),
							std::ios_base::binary);
						ofs.write(outbytes, entries[i].length);
						delete[] outbytes;
						++results.graphics_ripped;
					}
					else
					{
						// last entry was bmp, but has no frame identifier
						if (lastbmapnum >= 0 && !lastbmap_ripped)
						{
							write_bitmapdata_bmp(lastbmap, fprefix + "-bmp-" 
								+ to_string(lastbmapnum) + ".bmp");
							++results.graphics_ripped;
						}
						stream.seekg(entries[i].address);
						BitmapData bmp;
						CommFor::bmp::read_bmp_bitmapdata(stream, bmp);
						lastbmap = bmp;
						lastbmap_ripped = false;
						lastbmapnum = i;
//						for (int k = 0; k < bmp.get_allocation_size(); k++)
//						{
//							if (i > 0)
//								printf("%d %d %d\n", i, bmp.get_pixels()[k], lastbmap.get_pixels()[k]);
//						}
/*						stream.seekg(entries[i].address);
						BitmapData bmp;
						CommFor::bmp::read_bmp_bitmapdata(stream, bmp);
						write_bitmapdata_bmp(bmp, "test-" + to_string(i) + ".bmp");
						++results.graphics_ripped; */
					}
				}
			}
			else if (quickcmp(hdcheck, "RIFF", 4))
			{
				if (ripset.ripaudio)
				{
					stream.seekg(entries[i].address);
					char* outbytes = new char[entries[i].length];
					stream.read(outbytes, entries[i].length);
					std::ofstream ofs((fprefix + "-wave-"
						+ to_string(files_ripped) + ".wav").c_str(),
						std::ios_base::binary);
					ofs.write(outbytes, entries[i].length);
					delete[] outbytes;
					++results.audio_ripped;
				}
			}
			else
			{
				if (ripset.ripanimations)
				{
					if (!lastbmap_ripped && i == lastbmapnum + 1)
					{
						stream.seekg(entries[i].address);
						int numframes = entries[i].length/8;
						for (int j = 0; j < numframes; j++)
						{
							int up_x = stream.read_int(2, DatManip::le);
							int up_y = stream.read_int(2, DatManip::le);
							int low_x = stream.read_int(2, DatManip::le);
							int low_y = stream.read_int(2, DatManip::le);

							// frame ids aren't headered, so we can only check for sanity
							if (up_x < 0 || up_x > lastbmap.get_width()
								|| up_y < 0 || up_y > lastbmap.get_height()
								|| low_x < 0 || low_x > lastbmap.get_width()
								|| low_y < 0 || low_y > lastbmap.get_height()
								|| up_x >= low_x || up_y >= low_y)
							{
								break;
							}

							BitmapData bmap;
							lastbmap.copy_rect(bmap, up_x, up_y, low_x - up_x, low_y - up_y);
							write_bitmapdata_bmp(bmap, fprefix + "-anim-" 
								+ to_string(lastbmapnum) + "-frame-" + to_string(j + 1) + ".bmp");
							++results.animation_frames_ripped;
						}
						lastbmap_ripped = true;
						++results.animations_ripped;
					}
					// last entry was bmp, but this data entry is not a frame identifier
					else if (!lastbmap_ripped && lastbmapnum >= 0)
					{
						write_bitmapdata_bmp(lastbmap, fprefix + "-bmp-" 
							+ to_string(lastbmapnum) + ".bmp");
						lastbmap_ripped = true;
						++results.graphics_ripped;
					}
				}
				if (ripset.ripdata)
				{
					stream.seekg(entries[i].address);
					char* outbytes = new char[entries[i].length];
					stream.read(outbytes, entries[i].length);
					std::ofstream ofs((fprefix + "-data-"
						+ to_string(files_ripped)).c_str(),
						std::ios_base::binary);
					ofs.write(outbytes, entries[i].length);
					delete[] outbytes;
					++results.data_ripped;
				}
			}

			++files_ripped;
		}
		return results;
	}

	void AtlasRip::check_params(const RipperFormats::RipperSettings& ripset)
	{
		for (int i = 0; i < ripset.argc; i++)
		{
			if (quickstrcmp(ripset.argv[i], "--atlas_copybmp"))
			{
				copybmp = true;
			}
			else if (quickstrcmp(ripset.argv[i], "--atlas_splitbmp"))
			{
				copybmp = false;
			}
		}
	}


};	// end of namespace Atlas

#endif
