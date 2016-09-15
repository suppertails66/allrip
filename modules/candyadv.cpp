#ifdef ENABLE_CANDYADV

#include "candyadv.h"
#include "../utils/DatManip.h"
#include "../utils/MembufStream.h"
#include "../utils/PCMData.h"
#include "../RipperFormats.h"
#include <string>
#include <vector>
#include <iostream>

using namespace RipUtil;
using namespace RipperFormats;

namespace CandyAdv
{


bool CandyAdvRip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat)
{
	if (stream.get_fsize() <= 16)
		return false;

	stream.seekg(0);
	char hdcheck[4];
	stream.read(hdcheck, 4);
	if (!quickcmp(hdcheck, cndadv_chunkid, 4))
		return false;

	int chunklen = stream.read_int(4, DatManip::le);
	if (chunklen > stream.get_fsize())
		return false;

	int numentries = stream.read_int(2, DatManip::le);
	if (10 + numentries * 8 > stream.get_fsize())
		return false;

	stream.seek_off(6);
	int offset1 = stream.read_int(4, DatManip::le);
	stream.seekg(offset1);
	stream.read(hdcheck, 4);
	if (!quickcmp(hdcheck, cndadv_chunkid, 4))
		return false;

	return true;
}

RipperFormats::RipResults CandyAdvRip::rip(RipUtil::MembufStream& stream, const std::string& fprefix,
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
{
	RipResults results;
	
	stream.seekg(0);
	std::vector<CandyAdvOfftabEntry> entries;
	cndadv_read_offtab(stream, entries);

	if (ripset.ripallraw)
	{
		for (int i = 0; i < entries.size(); i++)
		{
			int entriesread = 0;
			int baseoff = entries[i].offset;
			stream.seekg(baseoff);
			std::vector<CandyAdvOfftabEntry> subchunkentries;
			cndadv_read_offtab(stream, subchunkentries);
			// if chunk does not itself contain a chunk, rip directly
			if (!subchunkentries.size())
			{
				stream.seekg(baseoff);
				char* outbytes = new char[entries[i].length];
				stream.read(outbytes, entries[i].length);
				std::ofstream ofs((fprefix + "-chunk-" + to_string(i)).c_str(), std::ios_base::binary);
				ofs.write(outbytes, entries[i].length);
				delete[] outbytes;
			}
			else
			{
				for (int j = 0; j < subchunkentries.size(); j++)
				{
					stream.seekg(baseoff + subchunkentries[j].offset);
					char* dat = new char[subchunkentries[j].length];
					stream.read(dat, subchunkentries[j].length);
					std::ofstream ofs((fprefix + "-chunk-" + to_string(i)
						+ "-data-" + to_string(entriesread++)).c_str(), std::ios_base::binary);
					ofs.write(dat, subchunkentries[j].length);
					delete[] dat;
					++results.data_ripped;
				}
			}
		}
		return results;
	}

	std::vector<BitmapPalette> palettes;

	// read palettes
	// palettes are the third subchunk of each room chunk
	if (ripset.guesspalettes)
	{
		stream.seekg(entries[1].offset);
		cndadv_read_palettes(stream, entries[1], palettes);
	}


	// chunk 1: animations
	if (ripset.ripanimations)
	{
		int chunk_base = entries[0].offset;
		stream.seekg(chunk_base);
		std::vector<CandyAdvOfftabEntry> subchunkentries;
		cndadv_read_offtab(stream, subchunkentries);
		int animpalette = 0;
		for (int i = 0; i < subchunkentries.size(); i++)
		{
			// we can't tell which palette goes to which animation,
			// so this is hardcoded
			// most palettes are almost identical, so we only handle
			// animations that are actually miscolored with palette 1
			if (i == 6)
				animpalette = 24;
			else if (i == 20)
				animpalette = 20;
			else if (i == 33)
				animpalette = 3;
			else if (i == 72)
				animpalette = 3;
			else if (i == 89)
				animpalette = 15;
			else if (i == 93)
				animpalette = 9;
			else if (i == 126)
				animpalette = 13;
			else if (i == 147 || i == 162 || i == 166)
				animpalette = 15;
			else if (i >= 150 && i <= 152)
				animpalette = 15;
			else if (i >= 167 && i <= 171)
				animpalette = 16;
			else if (i >= 173 && i <= 174)
				animpalette = 17;
			else if (i == 175)
				animpalette = 8;
			else
				animpalette = 1;

			int anim_base = chunk_base + subchunkentries[i].offset;
			stream.seekg(anim_base);
			std::vector<CandyAdvOfftabEntry> animentries;
			cndadv_read_offtab(stream, animentries);
			int grpdat_base = anim_base + animentries[0].offset;
			stream.seekg(grpdat_base);
			CandyAdvGrptab grptab;
			cndadv_read_grptab(stream, grptab);
			stream.seekg(grpdat_base + grptab.datoffset);
			for (int j = 0; j < grptab.entries.size(); j++)
			{
				// hack for correct animation 89 palettes
				if (i == 89)
				{
					if (j <= 25)
						animpalette = 9;
					else
						animpalette = 15;
				}

				BitmapData bmap;

				if (ripset.guesspalettes)
					bmap.set_palette(palettes[animpalette]);
				else
					bmap.set_palette_8bit_grayscale();

				cndadv_decode_image(stream, grptab.entries[j], bmap);
				write_bitmapdata_8bitpalettized_bmp(bmap, fprefix + "-anim-"
					+ to_string(i) + "-frame-" + to_string(j) + ".bmp");
				++results.animation_frames_ripped;
			}
			++results.animations_ripped;
		}
	}

	// chunk 2: graphics
	if (ripset.ripgraphics || ripset.ripanimations || ripset.ripaudio)
	{
		int chunk_base = entries[1].offset;
		stream.seekg(chunk_base);
		std::vector<CandyAdvOfftabEntry> subchunkentries;
		cndadv_read_offtab(stream, subchunkentries);
		for (int i = 0; i < subchunkentries.size(); i++)
		{
			int grp_base = chunk_base + subchunkentries[i].offset;
			stream.seekg(grp_base);
			std::vector<CandyAdvOfftabEntry> grpentries;
			cndadv_read_offtab(stream, grpentries);

			if (ripset.ripgraphics)
			{
				// get the background, which is always the first entry in the chunk
				// backgrounds are uncompressed 640x480 bitmaps composed of 32x32 blocks
				stream.seekg(grp_base + grpentries[0].offset);
				std::vector<CandyAdvOfftabEntry> bgentry;
				cndadv_read_offtab(stream, bgentry);
				int bghd_base = grp_base + grpentries[0].offset + bgentry[0].offset;
				stream.seekg(bghd_base);
				CandyAdvGrptab grpblocks;
				cndadv_read_grptab(stream, grpblocks);
				stream.seekg(bghd_base + grpblocks.datoffset);
				// ignore the above data, since we know what the format is
				BitmapData background;

				if (ripset.guesspalettes)
					background.set_palette(palettes[i]);
				else
					background.set_palette_8bit_grayscale();

				cndadv_read_blockdata(stream, background, 640, 480, 32, 32);
				write_bitmapdata_8bitpalettized_bmp(background, fprefix + "-bmap-" + to_string(i) + ".bmp");
				++results.graphics_ripped;
			}

			// get the extra animation data, if it exists
			if (ripset.ripanimations && grpentries.size() >= 7)
			{
				int grpani_base = grp_base + grpentries[5].offset;
				stream.seekg(grpani_base);
				CandyAdvGrptab grpanitab;
				cndadv_read_grptab(stream, grpanitab);
				stream.seekg(grpani_base + grpanitab.datoffset);
				for (int j = 0; j < grpanitab.entries.size(); j++)
				{
					BitmapData bmap;

					if (ripset.guesspalettes)
						bmap.set_palette(palettes[1]);
					else
						bmap.set_palette_8bit_grayscale();

					cndadv_decode_image(stream, grpanitab.entries[j], bmap);
					write_bitmapdata_8bitpalettized_bmp(bmap, fprefix + "-bmap-" + to_string(i)
						+ "-anim-" + to_string(j) + ".bmp");
					++results.animation_frames_ripped;
				}
				++results.animations_ripped;
			}

			// this chunk may also contain audio for loading screens
			if (ripset.ripaudio && grpentries.size() >= 6)
			{
				int audio_base;
				int sndlen;
				if (grpentries.size() >= 7)
				{
					audio_base = grp_base + grpentries[6].offset;
					sndlen = grpentries[6].length;
				}
				else
				{
					audio_base = grp_base + grpentries[5].offset;
					sndlen = grpentries[5].length;
				}
				stream.seekg(audio_base);
				PCMData wave(sndlen, 1, 11025, 8);
				wave.set_signed(DatManip::has_sign);
				stream.read(wave.get_waveform(), sndlen);
				if (ripset.numloops > 0)
				{
					wave.set_looping(true);
					wave.set_loopstart(0);
					wave.set_loopend(sndlen);
					wave.add_loop(wave.get_loopstart(), wave.get_loopend(), ripset.numloops - 1,
						ripset.loopstyle, ripset.loopfadelen, ripset.loopfadesil);
				}
				wave.convert_signedness(DatManip::has_nosign);
				write_pcmdata_wave(wave, fprefix + "-roomsnd-"
					+ to_string(i) + ".wav");
				++results.audio_ripped;
			}
		}
	}

	// chunk 3: audio
	if (ripset.ripaudio)
	{
		int snd_base = entries[2].offset;
		stream.seekg(snd_base);
		std::vector<CandyAdvOfftabEntry> subchunkentries;
		cndadv_read_offtab(stream, subchunkentries);

		for (int i = 0; i < subchunkentries.size(); i++)
		{
			PCMData wave(1, 11025, 8);
			// allow parameter overrides etc etc here
			stream.seekg(snd_base + subchunkentries[i].offset);
			wave.set_signed(DatManip::has_sign);
			wave.resize_wave(subchunkentries[i].length);
			stream.read(wave.get_waveform(), subchunkentries[i].length);

			// set loop
			// sounds that loop always do so beginning to end
			// looped sounds aren't marked, so this is hardcoded
			if (ripset.numloops > 0 && 
				(i == 61 || i == 66 || i == 284 || i == 287
				|| i == 307 || i == 456 || i == 459 || i == 639
				|| i == 640 || i == 928 || i == 930 || i == 1006
				|| i == 1045 || i == 1089 || i == 1134 || i == 1233
				|| i == 1330))
			{
				wave.set_looping(true);
				wave.set_loopstart(0);
				wave.set_loopend(subchunkentries[i].length);
				wave.add_loop(wave.get_loopstart(), wave.get_loopend(), ripset.numloops - 1,
					ripset.loopstyle, ripset.loopfadelen, ripset.loopfadesil);
			}

			wave.convert_signedness(DatManip::has_nosign);
			write_pcmdata_wave(wave, fprefix + "-snd-" + 
				to_string(results.audio_ripped++) + ".wav");
		}
	}


	return results;
}

void CandyAdvRip::cndadv_read_offtab(RipUtil::MembufStream& stream, 
	std::vector<CandyAdvOfftabEntry>& entries)
{
	char hdcheck[4];
	stream.read(hdcheck, 4);
	if (!quickcmp(hdcheck, cndadv_chunkid, 4))
		return;
	stream.seek_off(4);
	int numentries = stream.read_int(2, DatManip::le);
	stream.seek_off(6);

	for (int i = 0; i < numentries; i++)
	{
		CandyAdvOfftabEntry entry;
		entry.offset = stream.read_int(4, DatManip::le);
		entry.length = stream.read_int(4, DatManip::le);
		entries.push_back(entry);
	}
}

void CandyAdvRip::cndadv_read_grptab(RipUtil::MembufStream& stream,
		CandyAdvGrptab& grptab)
{
	grptab.unknown1 = stream.read_int(4, DatManip::le);
	grptab.unknown2 = stream.read_int(4, DatManip::le);
	grptab.unknown3 = stream.read_int(4, DatManip::le);
	grptab.unknown4 = stream.read_int(4, DatManip::le);
	grptab.unknown5 = stream.read_int(4, DatManip::le);
	grptab.unknown6 = stream.read_int(4, DatManip::le);
	grptab.datoffset = stream.read_int(4, DatManip::le);
	grptab.numimages = stream.read_int(4, DatManip::le);
	grptab.unknown7 = stream.read_int(4, DatManip::le);

	for (int i = 0; i < grptab.numimages; i++)
	{
		CandyAdvGrptabEntry entry;
		entry.compression = stream.read_int(4, DatManip::le);
		entry.datalen = stream.read_int(4, DatManip::le);
		entry.unknown1 = stream.read_int(4, DatManip::le);
		entry.width = stream.read_int(2, DatManip::le);
		entry.height = stream.read_int(2, DatManip::le);
		entry.unknown2 = stream.read_int(2, DatManip::le);
		entry.unknown3 = stream.read_int(2, DatManip::le);

		grptab.entries.push_back(entry);
	}
}

void CandyAdvRip::cndadv_decode_image(RipUtil::MembufStream& stream,
		const CandyAdvGrptabEntry& entry, BitmapData& bmap)
{
	bmap.resize_pixels(entry.width, entry.height, 8);
	bmap.set_palettized(true);
	for (int i = 0; i < bmap.get_allocation_size(); i++)
		bmap.get_pixels()[i] = 0x0;
	// no compression
	if (entry.compression == 0)
	{
		int* putpos = bmap.get_pixels();
		int imgsize = entry.width * entry.height;
		for (int i = 0; i < imgsize; i++)
			*putpos++ = stream.read_int(1);
	}
	// RLE8
	else if (entry.compression == 1)
	{
		int* putpos = bmap.get_pixels();
		int remaining = entry.width;
		int rowsripped = 0;
		while (rowsripped < entry.height)
		{
			int code = stream.read_int(1);
			int runlen = code & 0x7F;

			// end of line
			if (code == 0)
			{
				putpos += remaining;
				remaining = 0;
			}
			else 
			{
				if (runlen > remaining)
					runlen = remaining;

				// absolute pixel run
				if (code & 0x80)
				{
					for (int i = 0; i < runlen; i++)
						*putpos++ = stream.read_int(1);
				}
				// encoded pixel run
				else
				{
					int byte = stream.read_int(1);
					for (int i = 0; i < runlen; i++)
						*putpos++ = byte;
				}

				remaining -= runlen;
			}

			if (remaining <= 0)
			{
				// if the code wasn't EOL but we reached it anyway,
				// skip to EOL byte
				// should probably change to just making sure we only write pixels if remaining > 0
				if (code != 0)
					while (stream.read_int(1) != 0);

				++rowsripped;
				remaining = entry.width;
			}
		}
	}
}

void CandyAdvRip::cndadv_read_blockdata(RipUtil::MembufStream& stream,
		RipUtil::BitmapData& bmap, int width, int height, int blockw, int blockh)
{
	bmap.resize_pixels(width, height, 8);
	bmap.set_palettized(true);

	int blocksperrow = width/blockw;
	int numblocks = blocksperrow * height/blockh;
	int row = 0;
	int col = 0;
	for (int i = 0; i < numblocks; i++)
	{
		int* putpos = bmap.get_pixels() + col * blockw + row * width * blockh;
		for (int j = 0; j < 32; j++)
		{
			for (int k = 0; k < 32; k++)
			{
				putpos[k] = stream.read_int(1);
			}
			putpos += width;
		}

		++col;
		if ((i != 0) && !((i + 1) % blocksperrow))
		{
			++row;
			col = 0;
		}
	}
}

void CandyAdvRip::cndadv_read_palettes(RipUtil::MembufStream& stream,
		const CandyAdvOfftabEntry& entry, std::vector<RipUtil::BitmapPalette>& palettes)
{
	int chunk_base = stream.tellg();
	std::vector<CandyAdvOfftabEntry> subchunkentries;
	cndadv_read_offtab(stream, subchunkentries);
	for (int i = 0; i < subchunkentries.size(); i++)
	{
		int subchunk_base = chunk_base + subchunkentries[i].offset;
		stream.seekg(subchunk_base);
		std::vector<CandyAdvOfftabEntry> grpentries;
		cndadv_read_offtab(stream, grpentries);
		stream.seekg(subchunk_base + grpentries[2].offset);
		int numcolors = grpentries[2].length/4;
		BitmapPalette palette;
		for (int j = 0; j < numcolors; j++)
		{
			int color = 0;
			int r = stream.read_int(1);
			int g = stream.read_int(1);
			int b = stream.read_int(1);
			stream.seek_off(1);
			g <<= 8;
			b <<= 16;
			color = b | g | r;
			palette[j] = color;
		}
		palettes.push_back(palette);
	}
}


};	// end namespace CandyAdv

#endif
