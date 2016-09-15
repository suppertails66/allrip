#ifdef ENABLE_INDIAN

#include "indian.h"
#include "../RipperFormats.h"
#include "common.h"
#include <cstring>

using namespace RipperFormats;
using namespace RipUtil;

namespace IndCup
{


bool IndianRip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
	RipperFormats::FileFormatData& fmtdat)
{
	if (ripset.encoding != RipConsts::not_set)
		stream.set_decoding_byte(ripset.encoding);
	if (stream.get_fsize() <= 4)
		return false;
	char hdcheck[4];
	while (!stream.eof())
	{
		stream.read(hdcheck, 4);
		// Indian: first 4 bytes give address of file table
		int hdaddrcheck = to_int(hdcheck, 4);
		if (hdaddrcheck < stream.get_fsize())
		{
			stream.seekg(hdaddrcheck);
			// get number of entries in count table and skip it
			int countentries = stream.read_int(2);
			if (countentries + stream.tellg() < stream.get_fsize())
			{
				stream.seek_off(countentries * 2 + 4);
				// if the first address is 4, we're convinced
				int initialaddr = stream.read_int(4);
				if (initialaddr == 4)
				{
					fmtdat.format = IndianCup;
					fmtdat.encoding = stream.get_decoding_byte();
					formatsubtype = fmt_indian;
					return true;
				}
			}
		} stream.seekg(4);
		
		// No valid formats found; see if we can try something else
		// check alternate encodings
		if (fmtdat.format == format_unknown)
		{
			// if the encoding byte was overriden, stop checking here
			if (ripset.encoding != RipConsts::not_set)
			{
				stream.set_decoding_byte(0);
				return false;
			}
			// if no valid encoded header found, give up
			else if (stream.get_decoding_byte() == 0)
			{
				stream.set_decoding_byte(0);
				return false;
			}
			// rewind stream for next pass
			stream.seekg(0);
		}
	}
	return false;
}

RipResults IndianRip::rip(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
{
	check_params(ripset.argc, ripset.argv);

	stream.reset();
	stream.set_decoding_byte(fmtdat.encoding);

	RipResults results;

	// read the file table
	std::vector<AddrTabEnt> entries;
	int filetab_addr = stream.read_int(4);
	stream.seekg(filetab_addr);
	// skip count table
	int counttab_entries = stream.read_int(2);
	int addrtab_entries = stream.read_int(2);
	stream.seek_off(counttab_entries * 2 + 2);
	// read address table
	for (int i = 0; i < addrtab_entries; i++)
	{
		AddrTabEnt entry;
		entry.address = stream.read_int(4);
		// use the current address to find length of previous
		if (i > 0)
			entries[i - 1].length = entry.address - entries[i - 1].address;
		if (i == addrtab_entries - 1)
			entry.length = filetab_addr - entry.address;
		entries.push_back(entry);
	}

	// seeking all the way back across the file would take a long time,
	// so we reset it and start from the beginning
	stream.reset();

	// perform an initial pass, building the palette table
	// and determining the type of each entry
	std::vector<BitmapPalette> palettes;
	for (std::vector<AddrTabEnt>::size_type 
		i = 0; i < entries.size(); i++)
	{
		stream.seekg(entries[i].address);
		int id = stream.read_int(2);
		entries[i].id = id;

		// 0x8001: palette-included bitmap
		if (id == 0x8001)
			entries[i].dattype = pal_bitmap;
		// 0x8002: bitmap
		else if (id == 0x8002)
			entries[i].dattype = bitmap;
		// 0x8006: animation
		else if (id == 0x8006)
			entries[i].dattype = animation;
		// "FORM": AIFF
		else if (id == 0x464F)
		{
			if (stream.read_int(2) == 0x524D)
				entries[i].dattype = aiff;
			stream.seek_off(-2);
		}
		// palettes are unheadered, so we can only check for sanity
		else if ((id <= 256) && entries[i].length <= 770)
		{
			entries[i].dattype = palette;
			int numcolors = id;
			BitmapPalette pal;

			// fill first palette with background color
			if (!palettes.size())
				for (int j = 0; j < 256; j++)
					pal[j] = ripset.backgroundcolor;
			// game expects previous colors to remain in palette
			else
				for (int j = 0; j < 256; j++)
					pal[j] = palettes[palettes.size() - 1][j];

			// read colors from palette
			stream.seek_off(3);
			for (int j = 0; j < numcolors; j++)
			{
				pal[j] = indcup_read_color(stream);
			}
			palettes.push_back(pal);
		}
		else
		{
			entries[i].dattype = datatype_unknown;
		}
	}

	int palettenum;
	if (ripset.palettenum != RipConsts::not_set)
		palettenum = ripset.palettenum;
	else
		palettenum = RipConsts::not_set;
	int startentry = ripset.startentry == RipConsts::not_set ? 0
		: ripset.startentry;
	int endentry = ripset.startentry == RipConsts::not_set ? entries.size() - 1
		: ripset.endentry;
	int raws_ripped = 0;
	int bmaps_ripped = 0;
	int pal_bmaps_ripped = 0;
	int anis_ripped = 0;
	int ani_frames_ripped = 0;
	int pals_ripped = 0;
	int aiffs_ripped = 0;

	stream.reset();

	if (ripset.ripallraw)
	{
		for (std::vector<AddrTabEnt>::size_type
			i = 0; i < entries.size(); i++)
		{
			char* outbytes = new char[entries[i].length];
			stream.read(outbytes, entries[i].length);

			std::string filename = fprefix;
			switch (entries[i].dattype) {
			case datatype_none:
			case datatype_unknown:
				filename += "-unknown-id-" 
					+ to_string(entries[i].id) + "-";
				break;
			case bitmap:
				filename += "-bitmap-";
				break;
			case pal_bitmap:
				filename += "-pal_bitmap-";
				break;
			case animation:
				filename += "-animation-";
				break;
			case palette:
				filename += "-palette-";
				break;
			case aiff:
				filename += "-aiff-";
				break;
			}
			filename += to_string(i);

			std::ofstream ofs(filename.c_str(),
				std::ios_base::binary);
			ofs.write(outbytes, entries[i].length);
			delete[] outbytes;
			++(results.data_ripped);
		}
		return results;
	}

	// rip the data
	for (std::vector<AddrTabEnt>::size_type
		i = 0; i < entries.size(); i++)
	{
		if (i >= startentry && i <= endentry)
		{
			stream.seekg(entries[i].address);

			if (entries[i].dattype == datatype_unknown && ripset.ripdata)
			{
				std::ofstream ofs((fprefix + "-data-" + to_string(++raws_ripped)).c_str(),
					std::ios_base::binary);
				char* out = new char[entries[i].length];
				stream.read(out, entries[i].length);
				ofs.write(out, entries[i].length);
			}
			else if (entries[i].dattype == pal_bitmap && ripset.ripgraphics)
			{
				stream.seek_off(2);
				int numcolors = stream.read_int(2);
				if (numcolors == 255)
					numcolors -= 1;
				stream.seek_off(3);
				BitmapPalette pal;
				for (int j = 0; j < numcolors + 1; j++)
					pal[j] = indcup_read_color(stream);
				BitmapData bmap;
				bmap.set_palettized(true);
				bmap.set_palette(pal);
				indcup_read_bitmap(stream, bmap, ripset);
				write_bitmapdata_8bitpalettized_bmp(bmap, fprefix + "-pal_bmap-"
					+ to_string(++pal_bmaps_ripped) + ".bmp");
			}
			else if (entries[i].dattype == bitmap && ripset.ripgraphics)
			{
				BitmapData bmap;
				bmap.set_palettized(true);

				stream.seek_off(4);
				indcup_read_bitmap(stream, bmap, ripset);
				if (ripset.guesspalettes)
					bmap.set_palette(palettes[palettenum]);
				else
					bmap.set_palette_8bit_grayscale();
				write_bitmapdata_8bitpalettized_bmp(bmap, fprefix + "-bmap-"
					+ to_string(++bmaps_ripped) + ".bmp");
			}
			else if (entries[i].dattype == animation && ripset.ripanimations)
			{
				stream.seek_off(18);
				int fulllen = stream.read_int(4);
//				int datalen = fulllen - 9;
				int framenum = 0;

				AnimationFrameList frames;

				while (fulllen != 0)
				{
					AnimationFrame frame;

					frame.fulllen = fulllen;

					frame.unk1 = stream.read_int(4);
					frame.unk2 = stream.read_int(4);
					frame.unk3 = stream.read_int(4);
					frame.unk4 = stream.read_int(4);

					frame.unk5 = stream.read_int(2);
					frame.xoffset = to_signed(stream.read_int(2), 16);
					frame.yoffset = to_signed(stream.read_int(2), 16);
					frame.unk8 = stream.read_int(2);
					frame.unk9 = stream.read_int(2);
					frame.bytesperrow = stream.read_int(2);
					frame.width = stream.read_int(2);
					frame.height = stream.read_int(2);
					frame.unk14 = stream.read_int(2);

					int nextaddr = stream.tellg() + fulllen - 8;
					
					frame.image.set_palettized(true);
					frame.image.clear(0);

					indcup_read_aniframe(stream, frame.image, frame.bytesperrow, 
						frame.width, frame.height, ripset);

					// why doesn't this work? bad copy constructor?
					if (ripset.guesspalettes)
						frame.image.set_palette(palettes[palettenum]);
					else
						frame.image.set_palette_8bit_grayscale();

					frames.push_back(frame);
					
					stream.seekg(nextaddr);
					fulllen = stream.read_int(4);
				}

				if (ripseq)
				{
					SequenceSizingInfo seqsize = compute_sequence_enclosing_dimensions(frames);
				
					RipUtil::BitmapData bmp(seqsize.width, seqsize.height,
						8, true);

					if (ripset.guesspalettes)
						bmp.set_palette(palettes[palettenum]);
					else
						bmp.set_palette_8bit_grayscale();

					for (AnimationFrameList::iterator it = frames.begin();
						it != frames.end(); it++)
					{
						bmp.clear(0);

						bmp.blit_bitmapdata(it->image,
							seqsize.centerx + it->xoffset,
							seqsize.centery + it->yoffset,
							0);

						write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-ani-"
							+ to_string(anis_ripped + 1) + "-frame-"
							+ to_string(++framenum) + ".bmp");
					}
				}
				else
				{
					for (AnimationFrameList::iterator it = frames.begin();
						it != frames.end(); it++)
					{
						if (ripset.guesspalettes)
							it->image.set_palette(palettes[palettenum]);
						else
							it->image.set_palette_8bit_grayscale();

						write_bitmapdata_8bitpalettized_bmp(it->image, fprefix + "-ani-"
							+ to_string(anis_ripped + 1) + "-frame-"
							+ to_string(++framenum) + ".bmp");
					}
				}

				ani_frames_ripped += framenum;
				++anis_ripped;
			}
			else if (entries[i].dattype == aiff && ripset.ripaudio)
			{
				if (ripset.copycommon)
				{
					stream.seek_off(4);
					int filelen = stream.read_int(4);
					stream.seek_off(-8);
					char* outbytes = new char[filelen];
					stream.read(outbytes, filelen);
					std::ofstream ofs((fprefix + "-aiff-"
						+ to_string(aiffs_ripped++ + 1) + ".aif").c_str(),
						std::ios_base::binary);
					ofs.write(outbytes, filelen);
					delete[] outbytes;
				}
				else
				{
					PCMData wave;
					indcup_read_aiff(stream, wave, ripset);
					if (wave.get_looping() && ripset.numloops > 0)
						wave.add_loop(wave.get_loopstart(), wave.get_loopend(),
						ripset.numloops - 1, ripset.loopstyle, ripset.loopfadelen, 
						ripset.loopfadesil);
					if (ripset.normalize)
						wave.normalize();
					format_PCMData(wave, ripset);
					write_pcmdata_wave(wave, fprefix + "-aiff-"
						+ to_string(++aiffs_ripped) + ".wav");
				}
			}
			else if (entries[i].dattype == palette && ripset.ripdata)
			{
				std::ofstream ofs((fprefix + "-pal-" 
					+ to_string(pals_ripped++ + 1)).c_str(), std::ios_base::binary);
				char* out = new char[entries[i].length];
				stream.read(out, entries[i].length);
				ofs.write(out, entries[i].length);
			}
		} // end entry-number ripping limiter
		// change the palette whenever we reach a new one
		if (entries[i].dattype == palette)
		{
			if (ripset.palettenum == RipConsts::not_set)
				++palettenum;
		}
	}
	results.data_ripped = raws_ripped;
	results.graphics_ripped = bmaps_ripped + pal_bmaps_ripped;
	results.animations_ripped = anis_ripped;
	results.animation_frames_ripped = ani_frames_ripped;
	results.palettes_ripped = pals_ripped;
	results.audio_ripped = aiffs_ripped;
	return results;
}

void IndianRip::check_params(int argc, char* argv[])
{
	for (int i = 0; i < argc; i++)
	{
		if (std::strcmp(argv[i], "--noseq") == 0)
		{
			ripseq = false;
		}
	}
}

int IndianRip::indcup_read_color(MembufStream& stream)
{
	int color = 0;
	color |= stream.read_int(1);
	color |= (stream.read_int(1) << 8);
	color |= (stream.read_int(1) << 16);
	return color;
}

void IndianRip::indcup_read_bitmap(MembufStream& stream, BitmapData& dat,
	const RipperSettings& ripset)
{
	int bytesperrow = stream.read_int(2);
	int width = stream.read_int(2);
	int height = stream.read_int(2);
	int skiplen = bytesperrow - width;
	int datalen = width * height;
	dat.resize_pixels(width, height, 8);
	int* pixels = dat.get_pixels();
	stream.seek_off(2);
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pixels[i * width + j] = stream.read_int(1);
		}
		if (skiplen > 0)
			stream.seek_off(skiplen);
	}
}

void IndianRip::indcup_read_aniframe(MembufStream& stream, BitmapData& dat,
	int bytesperrow, int width, int height, const RipperSettings& ripset)
{
	dat.resize_pixels(width, height, 8);
	int* pixels = dat.get_pixels();
	for (int i = 0; i < dat.get_allocation_size(); i++)
		pixels[i] = 0x0;
	for (int i = 0; i < height; i++)
	{
		int* putpos = pixels + i * bytesperrow;
		int remaining = bytesperrow;
		while (remaining > 0)
		{
			int code = stream.read_int(1);
			int runlen = code & 0x7F;
			if (runlen > remaining)
				runlen = remaining;
			if (code & 0x80)
				putpos += runlen;
			else
				for (int j = 0; j < runlen; j++)
					*putpos++ = stream.read_int(1);
			remaining -= runlen;
		}
	}
}

void IndianRip::indcup_read_aiff(MembufStream& stream, PCMData& dat,
	const RipperSettings& ripset)
{
	dat.set_signed(DatManip::has_sign);

	stream.seek_off(4);
	int filelen = stream.read_int(4);
	int endpos = stream.tellg() + filelen;
	stream.seek_off(4);
	
	std::vector<CommFor::aiff::Mark> marks;
	int ssnd_pos;
	int loopstartmark;
	int loopendmark;
	char hdcheck[4];
	while (stream.tellg() < endpos)
	{
		stream.read(hdcheck, 4);
		int chunklen = stream.read_int(4);
		int nextaddr = stream.tellg() + chunklen;
		if (quickcmp(hdcheck, CommFor::aiff::comm_id, 4))
		{
			dat.set_channels(stream.read_int(2));
			stream.seek_off(4);
			dat.set_sampwidth(stream.read_int(2));
			stream.seek_off(2);
			dat.set_samprate(stream.read_int(2)/2);
		}
		else if (quickcmp(hdcheck, CommFor::aiff::ssnd_id, 4))
		{
			ssnd_pos = stream.tellg() + 8;
			int len = chunklen - 8;
			if (ripset.ignorebytes)
			{
				ssnd_pos += ripset.ignorebytes;
				len -= ripset.ignorebytes;
			}
			dat.set_wavesize(len);
		}
		else if (quickcmp(hdcheck, CommFor::aiff::mark_id, 4))
		{
			int nummarks = stream.read_int(2);
			for (int i = 0; i < nummarks; i++)
			{
				CommFor::aiff::Mark mark;
				mark.id = stream.read_int(2);
				mark.pos = stream.read_int(4);
				std::string markname;
				int namelen = stream.read_int(1);
				for (int j = 0; j < namelen; j++)
					markname += stream.read_int(1);
				mark.name = markname;
				marks.push_back(mark);
				stream.seek_off(1);
			}
		}
		else if (quickcmp(hdcheck, CommFor::aiff::inst_id, 4))
		{
			stream.seek_off(8);
			int playmode = stream.read_int(2);
			if (playmode == CommFor::aiff::playmode_forloop)
			{
				dat.set_looping(true);
				loopstartmark = stream.read_int(2);
				loopendmark = stream.read_int(2);
			}
		}
		else if (quickcmp(hdcheck, CommFor::aiff::appl_id, 4))
		{

		}

		stream.seekg(nextaddr);

		// hacks to work around format errors in the original files

		// chunk length was too short
		while (stream.read_int(1) == 0);
		stream.seek_off(-1);
		while (stream.read_int(1) == '\"');
		stream.seek_off(-1);
		while (stream.read_int(1) == 'D');
		stream.seek_off(-1);
		while (stream.read_int(1) == '?');
		stream.seek_off(-1);
		// chunk length was too long
		if (stream.get() == 'P')
			stream.seek_off(-1);
		stream.seek_off(-1);
	}
	stream.seekg(ssnd_pos);
	char* waveform = new char[dat.get_wavesize()];
	stream.read(waveform, dat.get_wavesize());
	delete[] dat.get_waveform();
	dat.set_waveform(waveform);
	if (dat.get_looping())
	{
		for (int i = 0; i < marks.size(); i++)
			if (marks[i].id == loopstartmark)
				dat.set_loopstart(marks[i].pos);
		for (int i = 0; i < marks.size(); i++)
			if (marks[i].id == loopendmark)
				dat.set_loopend(marks[i].pos);
	}
	dat.convert_signedness(DatManip::has_nosign);
}


};	// end of namespace IndCup

#endif
