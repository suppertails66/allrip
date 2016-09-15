#ifdef ENABLE_LEGOISLAND

#include "legoisland.h"
#include "common.h"
#include "../RipperFormats.h"
#include "../utils/DatManip.h"
#include "../utils/logger.h"
#include "../utils/PCMData.h"
#include "../utils/BitmapData.h"
#include "../utils/DefaultException.h"

#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include <algorithm>
#include "legoisland_structs.h"

using namespace RipUtil;
using namespace RipperFormats;
using namespace Logger;

namespace LegoIsland
{


bool LegoIslandRip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
	RipperFormats::FileFormatData& fmtdat)
{
	check_params(ripset);

	/* Test:
	1. Are first 4 bytes "RIFF"?
	2. Are next 4 bytes filesize? (1. and 2. not included in count)
	3. Are next 4 bytes "OMNI"?
	4. Are next 4 bytes "MxHd"? */

	stream.seekg(0);

	char hdcheck[4];
	stream.read(hdcheck, 4);
	if (!quickcmp(RIFF_hd, hdcheck, 4))
		return false;

	int lengthcheck = stream.read_int(4, DatManip::le);
	if ((lengthcheck + 8) != stream.get_fsize())
		return false;
	
	stream.read(hdcheck, 4);
	if (!quickcmp(OMNI_hd, hdcheck, 4))
		return false;
	
	stream.read(hdcheck, 4);
	if (!quickcmp(MxHd_hd, hdcheck, 4))
		return false;

	return true;
}

RipperFormats::RipResults LegoIslandRip::rip(RipUtil::MembufStream& stream, const std::string& fprefix, 
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
{
	logger.open(fprefix + "-log.txt");

	RipResults results;

	stream.reset();

	RIFFOMNIChunk riffomnic;
	readRIFFOMNI(stream, riffomnic);
	
	if (reporting)
		logRIFFOMNI(riffomnic, fprefix + "-report.txt");

	std::map<int, MxObChunk*> MxObIDMap;
	std::map<int, std::vector<MxChChunk*> > MxChIDMap;

	// add MxObs to map
	for (int i = 0; i < riffomnic.listmxstc.entries.size(); i++)
	{
		addMxObs(riffomnic.listmxstc.entries[i].mxobc, MxObIDMap);

		for (int j = 0; j < riffomnic.listmxstc.entries[i].listmxchc.entries.size(); j++)
		{
			addMxObs(riffomnic.listmxstc.entries[i].listmxchc.entries[j], MxObIDMap);
		}
	}

	// add MxChs to map
	for (int i = 0; i < riffomnic.listmxstc.entries.size(); i++)
	{
		LISTMxStChunk& listmxstc = riffomnic.listmxstc;

		for (int j = 0; j < listmxstc.entries[i].listmxdac.entries.size(); j++)
		{
			MxChChunk& mxchc = listmxstc.entries[i].listmxdac.entries[j];

			MxChIDMap[mxchc.thingid].push_back(&mxchc);
		}
	}

	// rip stuff
	for (std::map<int, std::vector<MxChChunk*> >::iterator it = MxChIDMap.begin();
		it != MxChIDMap.end(); it++)
	{
		int id = (*it).first;
		std::vector<MxChChunk*>& mxchs = (*it).second;

		MxObChunk& header = *(MxObIDMap[id]);
		switch (header.mxobid)
		{
		// type 1 and 2 don't exist?
		case (3): // movie or image set
			{
				MxChChunk& objheader = *(mxchs[0]);

				stream.seekg(objheader.datastart());
			
				// check if is this an image set or a Smacker video
				stream.seekg(objheader.datastart() + 14);
				char smkcheck[4];
				stream.read(smkcheck, 4);

				// Smacker
				if (quickcmp(smkcheck, "SMK2", 4))
				{
					if (rip_smackers)
						rip_typeid3_smacker(stream, id, mxchs, header, fprefix);
				}
				// image set
				else
				{
					// check if this is an FLC or a phoneme
					MxChChunk& objheader = *(mxchs[0]);

					stream.seekg(objheader.datastart() + 40);
					char compcheck[4];
					stream.read(compcheck, 4);

					// FLC
					if (quickcmp(compcheck, "BABL", 4))
					{
						if (rip_flcs)
							rip_typeid3_flcs(stream, id, mxchs, header, fprefix);
					}
					// phoneme
					else
					{
						if (rip_phonemes)
							rip_typeid3_phonemes(stream, id, mxchs, header, fprefix);
					}
				}
			}
			break;
		case (4):	// wave file
			if (rip_waves)
				rip_typeid4_wave(stream, id, mxchs, header, fprefix);
			break;
		case (6):	// dunno
			break;
		case (7):	// "controller"
			break;
		case (8):	// "event"
			break;
		case (9):	// dunno
			break;
		case (10):	// bitmap
			if (rip_bmaps)
				rip_typeid10_image(stream, id, mxchs, header, fprefix);
			break;
		case (11):	// model or animation
			break;
		default:
			logger.warning("Unrecognized MxOb ID " + to_string(header.mxobid)
				+ " for object " + to_string(id) 
				+ " (address: " + to_string(header.address()) + ")");
			break;
		}
	}

	return results;
}

void LegoIslandRip::check_params(const RipperFormats::RipperSettings& ripset)
{
	for (int i = 1; i < ripset.argc; i++)
	{
		if (std::strcmp(ripset.argv[i], "--nolog") == 0)
		{
			logging = false;
		}
		else if (std::strcmp(ripset.argv[i], "--enable_report") == 0)
		{
			reporting = true;
		}
		else if (std::strcmp(ripset.argv[i], "--nosmacker") == 0)
		{
			rip_smackers = false;
		}
		else if (std::strcmp(ripset.argv[i], "--noflc") == 0)
		{
			rip_flcs = false;
		}
		else if (std::strcmp(ripset.argv[i], "--nobmap") == 0)
		{
			rip_bmaps = false;
		}
		else if (std::strcmp(ripset.argv[i], "--nowave") == 0)
		{
			rip_waves = false;
		}
		else if (std::strcmp(ripset.argv[i], "--smackeronly") == 0)
		{
			rip_smackers = true;
			rip_flcs = false;
			rip_bmaps = false;
			rip_waves = false;
		}
		else if (std::strcmp(ripset.argv[i], "--flconly") == 0)
		{
			rip_smackers = false;
			rip_flcs = true;
			rip_bmaps = false;
			rip_waves = false;
		}
		else if (std::strcmp(ripset.argv[i], "--bmaponly") == 0)
		{
			rip_smackers = false;
			rip_flcs = false;
			rip_bmaps = true;
			rip_waves = false;
		}
		else if (std::strcmp(ripset.argv[i], "--waveonly") == 0)
		{
			rip_smackers = false;
			rip_flcs = false;
			rip_bmaps = false;
			rip_waves = true;
		}
		else if (std::strcmp(ripset.argv[i], "--enable_partial_phonemes") == 0)
		{
			rip_phonemes = true;
		}
		else if (std::strcmp(ripset.argv[i], "--force_wave_loop") == 0)
		{
			force_wave_loop = true;
		}
	}

	if (ripset.loopfadelen != RipperFormats::RipConsts::not_set)
	{
		loopfadelen = ripset.loopfadelen;
	}
	else if (ripset.loopfadesil != RipperFormats::RipConsts::not_set)
	{
		loopendsilence = ripset.loopfadesil;
	}
}

void LegoIslandRip::rip_typeid3(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	MxChChunk& objheader = *(mxchs[0]);

	stream.seekg(objheader.datastart());
			
	// check if is this an image set or a Smacker video
	stream.seekg(objheader.datastart() + 14);
	char smkcheck[3];
	stream.read(smkcheck, 3);

	if (quickcmp(smkcheck, "SMK", 3))
	{
		rip_typeid3_smacker(stream, id, mxchs, header, fprefix);
	}
	else
	{
		rip_typeid3_images(stream, id, mxchs, header, fprefix);
	}
}

void LegoIslandRip::rip_typeid3_smacker(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	std::ofstream ofs((fprefix + "-smk-"
		+ to_string(id) + ".smk").c_str(), std::ios_base::binary);

	// concatenate SMK data contents of each chunk into one file
	for (int i = 0; i < mxchs.size(); i++)
	{
		stream.seekg((*mxchs[i]).datastart() + 14);
		unsigned int length = (*mxchs[i]).data_nopad_size() - 14;

		char* copydat = new char[length];
		stream.read(copydat, length);
		ofs.write(copydat, length);
		delete[] copydat;
	}
}

void LegoIslandRip::rip_typeid3_images(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	MxChChunk& objheader = *(mxchs[0]);

	// get compression type
	stream.seekg(objheader.datastart() + 40);
	char compcheck[4];
	stream.read(compcheck, 4);

	if (quickcmp(compcheck, "BABL", 4))
	{
		rip_typeid3_flcs(stream, id, mxchs, header, fprefix);
	}
	else
	{
		rip_typeid3_phonemes(stream, id, mxchs, header, fprefix);
	}
}

void LegoIslandRip::rip_typeid3_flcs(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	MxChChunk& objheader = *(mxchs[0]);

	// get dimensions from header
	stream.seekg(objheader.datastart() + 22);
	int width = stream.read_int(2, DatManip::le);
	int height = stream.read_int(2, DatManip::le);
	int bpp = stream.read_int(2, DatManip::le);

	MxChChunk& phonfirst = *(mxchs[1]);

	stream.seekg(phonfirst.datastart() + 14);
	int type = stream.read_int(4, DatManip::le);
	stream.seekg(phonfirst.datastart() + 60);

	BitmapPalette palette;
	for (int i = 0; i < 256; i++)
	{
		int r = stream.read_int(1);
		int g = stream.read_int(1);
		int b = stream.read_int(1);

		palette[i] = (b << 16) | (g << 8) | r;
	}

	int length = stream.read_int(4, DatManip::le);
	length -= 6;
	int unk3 = stream.read_int(2, DatManip::le);
				
	int start = stream.tellg();
	int end = start + length;

	char* imgdat = new char[length];
	stream.read(imgdat, length);

	BitmapData bmpbase(width, height, 8);
	bmpbase.set_palette(palette);

	decode_lego_type2_rle8(bmpbase, imgdat, length);

	write_bitmapdata_8bitpalettized_bmp(bmpbase, fprefix
		+ "-flc-" + to_string(id) + "-1" + ".bmp");

	for (int i = 2; i < mxchs.size() - 1; i++)
	{
		MxChChunk& imgchunk = *(mxchs[i]);

		/* check for proper chunk type should go here,
		but this seems to work for all existing cases */
		if (imgchunk.datasize() < 40) continue;

		stream.seekg(imgchunk.datastart() + 14);
		int type = stream.read_int(4, DatManip::le);
		stream.seekg(imgchunk.datastart() + 50);

		int length = stream.read_int(4, DatManip::le);
		length -= 10;
		int unk3 = stream.read_int(2, DatManip::le);
		int unk4 = stream.read_int(2, DatManip::le);
		int unk5 = stream.read_int(2, DatManip::le);
				
		int start = stream.tellg();
		int end = start + length;

		char* imgdat = new char[length];
		stream.read(imgdat, length);

		BitmapData bmp(width, height, 8);
		bmp.set_palette(palette);

		decode_lego_type2_skipblock(bmp, imgdat, length);

		delete[] imgdat;

		write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-flc-"
		+ to_string(id) + "-" + to_string(i) + ".bmp");
	}
}

void LegoIslandRip::rip_typeid3_phonemes(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	MxChChunk& objheader = *(mxchs[0]);

	// get dimensions from header
	stream.seekg(objheader.datastart() + 22);
	int width = stream.read_int(2, DatManip::le);
	int height = stream.read_int(2, DatManip::le);
	int bpp = stream.read_int(2, DatManip::le);

	MxChChunk& phonfirst = *(mxchs[1]);

	stream.seekg(phonfirst.datastart() + 14);
	int type = stream.read_int(4, DatManip::le);
	stream.seekg(phonfirst.datastart() + 60);

	BitmapPalette palette;
	for (int i = 0; i < 256; i++)
	{
		int r = stream.read_int(1);
		int g = stream.read_int(1);
		int b = stream.read_int(1);

		palette[i] = (b << 16) | (g << 8) | r;
	}

	int length = stream.read_int(4, DatManip::le);
	length -= 6;
	int unk3 = stream.read_int(2, DatManip::le);
				
	int start = stream.tellg();
	int end = start + length;

	char* imgdat = new char[length];
	stream.read(imgdat, length);

	BitmapData bmpbase(width, height, 8);
	bmpbase.set_palette(palette);

	decode_lego_rle8(bmpbase, imgdat, length);
				
	write_bitmapdata_8bitpalettized_bmp(bmpbase, fprefix + "-phoneme-"
		+ to_string(id) + "-1" + ".bmp");

	for (int i = 2; i < mxchs.size() - 1; i++)
	{
		MxChChunk& imgchunk = *(mxchs[i]);

		/* check for proper chunk type should go here,
		but this seems to work for all extant cases */
		if (imgchunk.datasize() < 40) continue;

		stream.seekg(imgchunk.datastart() + 14);
		int type = stream.read_int(4, DatManip::le);
		stream.seekg(imgchunk.datastart() + 50);

		int length = stream.read_int(4, DatManip::le);
		length -= 10;

		int unk3 = stream.read_int(2, DatManip::le);

		// number of rows in image
		int numrows = stream.read_int(2, DatManip::le);

		// y-offset of this image when superimposed over previous
		int yoffset = stream.read_int(2, DatManip::le);
		// this is a signed 16-bit value
		if (yoffset >= 0x8000)
			yoffset = -((~yoffset & 0xFFFF) + 1);
		else
			yoffset = -yoffset;
		// convert to number of topmost row of this image
		// when blitted onto the previous
		yoffset += height - numrows;
				
		int start = stream.tellg();
		int end = start + length;

		char* imgdat = new char[length];
		stream.read(imgdat, length);

		BitmapData bmp(width, height, 8);
		bmp.set_palette(palette);
		bmp.blit_bitmapdata(bmpbase, 0, 0);

		decode_lego_skipblock(bmp, imgdat, length, numrows, yoffset);

		delete[] imgdat;

		// put previous frame in buffer for use by next frame
		bmpbase.blit_bitmapdata(bmp, 0, 0);

		write_bitmapdata_8bitpalettized_bmp(bmpbase, fprefix + "-phoneme-"
		+ to_string(id) + "-" + to_string(i) + ".bmp");
	}
}

void LegoIslandRip::rip_typeid4_wave(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	PCMData wave;
	wave.set_channels(1);

	// get sample rate and resolution from header
	MxChChunk& waveheader = *(mxchs[0]);
	stream.seekg(waveheader.datastart());
	stream.seek_off(16);
	wave.set_channels(stream.read_int(2, DatManip::le));
	wave.set_samprate(stream.read_int(4, DatManip::le));
	stream.seek_off(6);
	wave.set_sampwidth(stream.read_int(2, DatManip::le));

	// copy all sample blocks into wave data
	for (int i = 1; i < mxchs.size() - 1; i++)
	{
		if (wave.get_sampwidth() == 16)
		{
			wave.set_end(DatManip::le);
			wave.set_signed(DatManip::has_sign);
		}
		else
		{
			wave.set_end(DatManip::end_none);
			wave.set_signed(DatManip::has_nosign);
		}

		stream.seekg((*mxchs[i]).datastart() + 14);
		char* samps = new char[(*mxchs[i]).data_nopad_size() - 14];
		stream.read(samps, (*mxchs[i]).data_nopad_size() - 14);
		wave.append(samps, (*mxchs[i]).data_nopad_size() - 14);
		delete[] samps;
	}

	if (force_wave_loop)
	{
		wave.set_loopstart(0);
		wave.set_loopend(wave.get_wavesize()/(wave.get_sampwidth()/8));
		wave.add_loop(wave.get_loopstart(), wave.get_loopend(), 0,
			RipUtil::PCMData::fadeloop, loopfadelen, loopendsilence);
	}

	write_pcmdata_wave(wave, fprefix + "-wav-" + to_string(id) + ".wav");
}

void LegoIslandRip::rip_typeid10_image(
	RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
	MxObChunk& header, const std::string& fprefix)
{
	BitmapData bmp;

	// read dimensions from header
	MxChChunk& bmpheader = *(mxchs[0]);
	stream.seekg(bmpheader.datastart() + 10);
	int unk1 = stream.read_int(4, DatManip::le);
	int unk2 = stream.read_int(4, DatManip::le);
	unsigned int width = stream.read_int(4, DatManip::le);
	unsigned int height = stream.read_int(4, DatManip::le);
	unsigned int compression = stream.read_int(2, DatManip::le);
	unsigned int bpp = stream.read_int(2, DatManip::le);
	int unk3 = stream.read_int(4, DatManip::le);
	unsigned int imgsize = stream.read_int(4, DatManip::le) + 1;
	int unk4 = stream.read_int(4, DatManip::le);
	int unk5 = stream.read_int(4, DatManip::le);

	if (compression != 1)
		throw (DefaultException("unexpected image compression"));
	if (bpp != 8)
		throw (DefaultException("unexpected image bpp"));

	// read color table: always 256 colors?
	stream.seekg(bmpheader.datastart() + 54);
	BitmapPalette palette;
	for (int i = 0; i < 256; i++)
	{
		unsigned int b = stream.read_int(1);
		unsigned int g = stream.read_int(1);
		unsigned int r = stream.read_int(1);
		unsigned int color = (b << 16) | (g << 8) | r;
		stream.seek_off(1);

		palette[i] = color;
	}

	bmp.resize_pixels(width, height, 8);
	bmp.set_palette(palette);

	// copy pixel data out of each subchunk
	char* bmpdata = new char[imgsize];
	char* dataputpos = bmpdata;
	for (int i = 1; i < mxchs.size() - 1; i++)
	{
		stream.seekg((*mxchs[i]).datastart() + 14);

		stream.read(dataputpos, (*mxchs[i]).data_nopad_size() - 14);
		dataputpos += (*mxchs[i]).data_nopad_size() - 14;
	}
			
	// pixel data is in native Windows DIB/BMP format (bottom to top scanlines,
	// each line padded to 4-byte boundary), which we must account for
	unsigned int bytesperline = width;
	if (width % 4 == 1)
		bytesperline += 3;
	else if (width % 4 == 2)
		bytesperline += 2;
	else if (width % 4 == 3)
		bytesperline += 1;
	char* datagetpos = bmpdata + width;
	int* bmpputpos = bmp.get_pixels() + bmp.get_allocation_size() - 1;
	for (int i = 0; i < height; i++)
	{
		for (int i = 0; i < width; i++)
			*bmpputpos-- = *datagetpos--;

		datagetpos += bytesperline + width;
	}

	delete[] bmpdata;

	write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-bmp-"
		+ to_string(id) + ".bmp");
}

void LegoIslandRip::decode_lego_rle8(RipUtil::BitmapData& bmp, const char* source, int length)
{
	const char* gpos = source;
	int* putpos = bmp.get_pixels();

	for (int i = bmp.get_height() - 1; i >= 0; i--)
	{
		int unk1 = *gpos++;	// number of encoding bytes in this row?

		putpos = bmp.get_pixels() + bmp.get_width() * i;
		int remaining = bmp.get_width();
		
		while (remaining > 0)
		{
			unsigned char encoding = *gpos++;

			// MSB set: absolute run
			if (encoding & 0x80)
			{
				encoding = ~encoding + 1;

				for (int j = 0; j < encoding; j++)
				{
					*putpos++ = *gpos++;
					--remaining;
				}
			}
			// MSB not set: encoded run
			else
			{
				for (int j = 0; j < encoding; j++)
				{
					*putpos++ = *gpos;
					--remaining;
				}
				++gpos;
			}
		}
	}
}

void LegoIslandRip::decode_lego_type2_rle8(RipUtil::BitmapData& bmp, const char* source, int length)
{
	const char* gpos = source;
	int rownum = 0;
	int* putpos = bmp.get_pixels();

	
	for (int i = 0; i < bmp.get_height(); i++)
	{
		int unk1 = *gpos++;	// number of encoding bytes in this row?

		putpos = bmp.get_pixels() + bmp.get_width() * i;
		int remaining = bmp.get_width();
		
		while (remaining > 0)
		{
			unsigned char encoding = *gpos++;

			// MSB set: absolute run
			if (encoding & 0x80)
			{
				encoding = ~encoding + 1;

				for (int j = 0; j < encoding; j++)
				{
					*putpos++ = *gpos++;
					--remaining;
				}
			}
			// MSB not set: encoded run
			else
			{
				for (int j = 0; j < encoding; j++)
				{
					*putpos++ = *gpos;
					--remaining;
				}
				++gpos;
			}
		}
	}
}

void LegoIslandRip::decode_lego_skipblock(
	RipUtil::BitmapData& bmp, const char* source, int length,
	int numrows, int yoffset)
{
	int position = 0;
	const char* gpos = source;
	int* putpos = bmp.get_pixels();

	int remaining = bmp.get_width();
	
	unsigned char startskip = *gpos++;
	unsigned char startsize = *gpos++;
	position += 2;
	bool startskipped = false;
	
	for (int i = numrows - 1 + yoffset; i >= yoffset; i--)
	{
		int remaining = bmp.get_width();
		putpos = bmp.get_pixels() + bmp.get_width() * i;

		if (!startskipped)
		{
			if (startsize == 0)
			{
				putpos += startskip;
				remaining -= startskip;
			}
			else
			{
				gpos -= 2;
				position -= 2;
			} 
			startskipped = true;
		}

		bool newline = false;

		while (!newline)
		{
			unsigned char skipcount = *gpos++;
			unsigned char bsize = *gpos++;
			position += 2;
		
			remaining -= skipcount;

			// null: newline
			if (bsize == 0)
			{
				newline = true;
			}
			else if (remaining <= 0)
			{

			}
			else
			{
				// advance put position by skip count
				putpos += skipcount;

				// MSB set: 2-byte pattern repeat
				if (bsize & 0x80)
				{
					// take 2's complement to get repeat count
					bsize = ~bsize + 1;

					char copybytes[2];
					std::memcpy(copybytes, gpos, 2);
					gpos += 2;
					position += 2;

					for (int j = 0; j < bsize; j++)
					{
						char* copypos = copybytes;

						for (int k = 0; k < 2; k++)
							*putpos++ = *copypos++;

						remaining -= 2;
					}
				}
				// MSB not set: absolute run
				else
				{
					bsize <<= 1;	// multiply by 2 to get block size

					for (int j = 0; j < bsize; j++)
					{
						*putpos++ = *gpos++;
						position++;
						--remaining;
					}
				}
			}
		}
	}
}

void LegoIslandRip::decode_lego_type2_skipblock(RipUtil::BitmapData& bmp, const char* source, int length)
{
	const char* gpos = source;
	int rownum = 0;
	int* putpos = bmp.get_pixels();

	int remaining = bmp.get_width();
	
	unsigned char startskip = *gpos++;
	unsigned char startsize = *gpos++;

	if (startsize == 0)
	{
		putpos += startskip;
		remaining -= startskip;
	}
	else
	{
		gpos -= 2;
	}
	
	while (gpos < source + length)
	{
		unsigned char skipcount = *gpos++;
		unsigned char bsize = *gpos++;
		
		remaining -= skipcount;

		// null: newline
		if (bsize == 0)
		{
			++rownum;
			putpos = bmp.get_pixels() + bmp.get_width() * rownum;
			remaining = bmp.get_width();
		}
		else if (remaining <= 0)
		{

		}
		else
		{
			// advance put position by skip count
			putpos += skipcount;

			// MSB set: 2-byte pattern repeat
			if (bsize & 0x80)
			{
				// take 2's complement to get repeat count
				bsize = ~bsize + 1;

				char copybytes[2];
				std::memcpy(copybytes, gpos, 2);
				gpos += 2;

				for (int j = 0; j < bsize; j++)
				{
					char* copypos = copybytes;

					for (int k = 0; k < 2; k++)
						*putpos++ = *copypos++;

					remaining -= 2;
				}
			}
			// MSB not set: absolute run
			else
			{
				bsize <<= 1;	// multiply by 2 to get block size

				for (int j = 0; j < bsize; j++)
				{
					*putpos++ = *gpos++;
					--remaining;
				}
			}
		}
	}
}

void LegoIslandRip::read_cstring_stream(RipUtil::MembufStream& stream, std::string& dest)
{
	char c;
	while (!(stream.eof()) && (c = stream.get()))
		dest += c;
}

void LegoIslandRip::addMxObs(MxObChunk& mxobc, std::map<int, MxObChunk*>& MxObIDMap)
{
	// add this MxOb to map
	MxObIDMap[mxobc.thingid] = &mxobc;

	// recursively add all LIST/MXCh contained MxObs to map
	for (int i = 0; i < mxobc.listmxchc.entries.size(); i++)
	{
		addMxObs(mxobc.listmxchc.entries[i], MxObIDMap);
	}
}

bool LegoIslandRip::MxChAddressOrder(MxChChunk* mxchc_p1, MxChChunk* mxchc_p2)
{
	return (*mxchc_p1).address() < (*mxchc_p2).address();
}

void LegoIslandRip::readRIFFOMNI(RipUtil::MembufStream& stream, RIFFOMNIChunk& dest)
{
	readChunkHead(stream, dest);

	stream.seek_off(12);

	while (stream.tellg() < dest.nextaddress())
	{
		bool seektonext = true;

		ChunkHead hdcheck(stream);
		switch (hdcheck.type())
		{
		case MxHd:
			readMxHd(stream, dest.mxhdc);
			break;
		case MxOf:
			readMxOf(stream, dest.mxofc);
			break;
		case LIST:
		{
			LISTChunkHead listhd(stream);
			switch (listhd.listtype())
			{
			case MxSt:
				readLISTMxSt(stream, dest.listmxstc);
				break;
			case chunkid_unknown:
				logger.error("Unrecognized RIFFOMNI LIST type "
					+ listhd.liststring() + " at " + to_string(stream.tellg()));
				break;
			default:
				logger.error("Unexpected RIFFOMNI LIST type "
					+ listhd.liststring() + " (ID "
					+ to_string(listhd.listtype()) + " at " + to_string(stream.tellg()));
				break;
			}
			break;
		}
		case chunkid_unknown:
			logger.error("Unrecognized RIFFOMNI subchunk chunk type "
				+ hdcheck.typestring() + " at " + to_string(stream.tellg()));
			break;
		default:
			logger.error("Unexpected RIFFOMNI subchunk type "
				+ hdcheck.typestring() + " (ID "
				+ to_string(hdcheck.type()) + " at " + to_string(stream.tellg()));
			break;
		}

		if (seektonext)
			stream.seekg(hdcheck.nextaddress());
	}

}

void LegoIslandRip::readMxHd(RipUtil::MembufStream& stream, MxHdChunk& dest)
{
	readChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	dest.unk1 = stream.read_int(2, DatManip::le);
	dest.unk2 = stream.read_int(2, DatManip::le);
	dest.unk3 = stream.read_int(2, DatManip::le);
	dest.unk4 = stream.read_int(2, DatManip::le);
	dest.unk5 = stream.read_int(4, DatManip::le);
}

void LegoIslandRip::readMxOf(RipUtil::MembufStream& stream, MxOfChunk& dest)
{
	readChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	dest.numentries = stream.read_int(4, DatManip::le);
	for (int i = 0; i < dest.datasize()/4 - 1; i++)
	{
		dest.entries.push_back(stream.read_int(4, DatManip::le));
	}
	stream.seekg(dest.nextaddress());
}

void LegoIslandRip::readLISTMxSt(RipUtil::MembufStream& stream, LISTMxStChunk& dest)
{
	readLISTChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	while (stream.tellg() < dest.nextaddress())
	{
		bool seektonext = true;

		ChunkHead hdcheck(stream);
		switch (hdcheck.type())
		{
		case MxSt:
		{
			MxStChunk mxstc;
			readMxSt(stream, mxstc);
			dest.entries.push_back(mxstc);
			break;
		}
		case pad:
			readpad(stream, dest.padc);
			break;
		case chunkid_unknown:
			logger.error("Unrecognized LISTMxSt subchunk type "
				+ hdcheck.typestring() + " at " + to_string(stream.tellg()));
			break;
		default:
			logger.error("Unexpected LISTMxSt subchunk type "
				+ hdcheck.typestring() + " (ID "
				+ to_string(hdcheck.type()) + ")" + " at " + to_string(stream.tellg()));
			break;
		}

		if (seektonext)
			stream.seekg(hdcheck.nextaddress());
	}

	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readMxSt(RipUtil::MembufStream& stream, MxStChunk& dest)
{
	readChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	while (stream.tellg() < dest.nextaddress())
	{
		bool seektonext = true;

		ChunkHead hdcheck(stream);
		switch (hdcheck.type())
		{
		case MxOb:
			readMxOb(stream, dest.mxobc);
			break;
		case LIST:
		{
			LISTChunkHead listhd(stream);
			switch (listhd.listtype())
			{
			case MxDa:
				readLISTMxDa(stream, dest.listmxdac);
				break;
			case chunkid_unknown:
				logger.error("Unrecognized MxSt LIST type "
					+ listhd.liststring() + " at " + to_string(stream.tellg()));
				break;
			default:
				logger.error("Unexpected MxSt LIST type "
					+ listhd.liststring() + " (ID "
					+ to_string(listhd.listtype()) + " at " + to_string(stream.tellg()));
				break;
			}
			break;
		}
		case chunkid_unknown:
			logger.error("Unrecognized MxSt subchunk chunk type "
				+ hdcheck.typestring() + " at " + to_string(stream.tellg()));
			break;
		default:
			logger.error("Unexpected MxSt subchunk type "
				+ hdcheck.typestring() + " (ID "
				+ to_string(hdcheck.type()) + " at " + to_string(stream.tellg()));
			break;
		}

		if (seektonext)
			stream.seekg(hdcheck.nextaddress());
	}

	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readMxOb(RipUtil::MembufStream& stream, MxObChunk& dest)
{
	readChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	dest.mxobid = stream.read_int(2, DatManip::le);
	read_cstring_stream(stream, dest.typecstr);
	dest.unk1 = stream.read_int(4, DatManip::le);
	read_cstring_stream(stream, dest.name);
	dest.thingid = stream.read_int(4, DatManip::le);
	dest.unk5 = stream.read_int(4, DatManip::le);
	dest.unk6 = stream.read_int(4, DatManip::le);
	dest.unk7 = stream.read_int(4, DatManip::le);
	dest.unk8 = stream.read_int(4, DatManip::le);
	stream.read(dest.unk2, 72);
	dest.objinf_runlen = stream.read_int(2, DatManip::le);
	if (dest.objinf_runlen != 0)
		read_cstring_stream(stream, dest.objinf);

	char hdcheck[4];
	stream.read(hdcheck, 4);
	stream.seek_off(-4);
	if (quickcmp(hdcheck, LIST_hd, 4))
	{
		readLISTMxCh(stream, dest.listmxchc);
	}
	else
	{
		read_cstring_stream(stream, dest.filename);
		stream.read(dest.unk3, 24);// NOT RIGHT!!!
	}

	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readLISTMxCh(RipUtil::MembufStream& stream, LISTMxChChunk& dest)
{
	readLISTChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	// sometimes there's an extra bit of "RANDOM" data here
	char randcheck[4];
	stream.read(randcheck, 4);
	if (quickcmp(randcheck, "RAND", 4))
	{
		dest.set_size(dest.size() - 1);
		stream.seek_off(15);
	}
	else
	{
		stream.seek_off(-4);
		dest.mxobcount = stream.read_int(4, DatManip::le);
	}

	while (stream.tellg() < dest.nextaddress())
	{
		bool seektonext = true;

		ChunkHead hdcheck(stream);
		switch (hdcheck.type())
		{
		case MxOb:
		{
			MxObChunk mxobc;
			readMxOb(stream, mxobc);
			dest.entries.push_back(mxobc);
			break;
		}
		case LIST:
		{
			LISTChunkHead listhd(stream);
			switch (listhd.listtype())
			{
			case chunkid_unknown:
				logger.error("Unrecognized LISTMxCh LIST type "
					+ listhd.liststring() + " at " + to_string(stream.tellg()));
				break;
			default:
				logger.error("Unexpected LISTMxCh LIST type "
					+ listhd.liststring() + " (ID "
					+ to_string(listhd.listtype()) + " at " + to_string(stream.tellg()));
				break;
			}
			break;
		}
		case chunkid_unknown:
			logger.error("Unrecognized LISTMxCh subchunk chunk type "
				+ hdcheck.typestring() + " at " + to_string(stream.tellg()));
			break;
		default:
			logger.error("Unexpected LISTMxCh subchunk type "
				+ hdcheck.typestring() + " (ID "
				+ to_string(hdcheck.type()) + " at " + to_string(stream.tellg()));
			break;
		}

		if (seektonext)
			stream.seekg(hdcheck.nextaddress());
	}
	
	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readLISTMxDa(RipUtil::MembufStream& stream, LISTMxDaChunk& dest)
{
	readLISTChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	while (stream.tellg() < dest.nextaddress())
	{
		bool seektonext = true;

		ChunkHead hdcheck(stream);
		switch (hdcheck.type())
		{
		case MxCh:
		{
			MxChChunk mxchc;
			readMxCh(stream, mxchc);
			dest.entries.push_back(mxchc);
			break;
		}
		case pad:
			readpad(stream, dest.padc);
			break;
		case LIST:
		{
			LISTChunkHead listhd(stream);
			switch (listhd.listtype())
			{
			case chunkid_unknown:
				logger.error("Unrecognized LISTMxDa LIST type "
					+ listhd.liststring() + " at " + to_string(stream.tellg()));
				break;
			default:
				logger.error("Unexpected LISTMxDa LIST type "
					+ listhd.liststring() + " (ID "
					+ to_string(listhd.listtype()) + " at " + to_string(stream.tellg()));
				break;
			}
			break;
		}
		case chunkid_unknown:
		{
			// some chunks underrun the size by 2, 4, or 6 bytes
			// try 2 bytes
			stream.seekg(hdcheck.address() + 2);
			ChunkHead hdcheck2(stream);
			// if chunk is valid, restart from this point
			if (hdcheck2.type() != chunkid_unknown)
			{
				hdcheck = hdcheck2;
				seektonext = false;
				logger.warning("Initially unrecognized LISTMxDa subchunk type "
						+ hdcheck.typestring() + " at " + to_string(stream.tellg())
						+ ", recovered in 2 bytes");
			}
			else
			{
				// try 4 bytes
				stream.seekg(hdcheck.address() + 4);
				readChunkHead(stream, hdcheck2);
				// if chunk is valid, restart from this point
				if (hdcheck2.type() != chunkid_unknown)
				{
					hdcheck = hdcheck2;
					seektonext = false;
					logger.warning("Initially unrecognized LISTMxDa subchunk type "
						+ hdcheck.typestring() + " at " + to_string(stream.tellg())
						+ ", recovered in 4 bytes");
				}
				else
				{
					// try 6 bytes
					stream.seekg(hdcheck.address() + 6);
					readChunkHead(stream, hdcheck2);
					// if chunk is valid, restart from this point
					if (hdcheck2.type() != chunkid_unknown)
					{
						hdcheck = hdcheck2;
						seektonext = false;
						logger.warning("Initially unrecognized LISTMxDa subchunk type "
							+ hdcheck.typestring() + " at " + to_string(stream.tellg())
							+ ", recovered in 6 bytes");
					}
					else
					{
						// give up
						logger.error("Unrecognized LISTMxDa subchunk type "
							+ hdcheck.typestring() + " at " + to_string(stream.tellg()));

						// try to avoid going out of range
						if (hdcheck.nextaddress() > dest.nextaddress()
							|| hdcheck.nextaddress() <= hdcheck.address())
							stream.seekg(dest.nextaddress());
					}
				}
			}
			break;
		}
		default:
			logger.error("Unexpected LISTMxDa subchunk type "
				+ hdcheck.typestring() + " (ID "
				+ to_string(hdcheck.type()) + " at " + to_string(stream.tellg()));
			break;
		}

		if (seektonext)
			stream.seekg(hdcheck.nextaddress());
	}
	
	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readpad(RipUtil::MembufStream& stream, padChunk& dest)
{
	readChunkHead(stream, dest);
	
	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::readMxCh(RipUtil::MembufStream& stream, MxChChunk& dest)
{
	readChunkHead(stream, dest);
	stream.seekg(dest.datastart());

	dest.unk1 = stream.read_int(2, DatManip::le);
	dest.thingid = stream.read_int(4, DatManip::le);
	
	stream.seekg(dest.nextaddress());
};

void LegoIslandRip::printMxObInfo(MxObChunk& mxobc, std::ofstream& ofs)
{
	ofs << '\t' << '\t' << '\t' << "MxOb" << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Address: " << mxobc.address() << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Length: " << mxobc.datasize() << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Type ID: " << mxobc.mxobid << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Type: " << mxobc.typecstr << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "unk1: " << mxobc.unk1 << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Name: " << mxobc.name << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "Obj ID: " << mxobc.thingid << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "unk5: " << mxobc.unk5 << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "unk6: " << mxobc.unk6 << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "unk7: " << mxobc.unk7 << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "unk8: " << mxobc.unk8 << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "objinf_runlen: " << mxobc.objinf_runlen << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "objinf: " << mxobc.objinf << '\n';
	ofs << '\t' << '\t' << '\t' << '\t' << "filename: " << mxobc.filename << '\n';

	if (mxobc.listmxchc.listtype() == MxCh)
	{
		ofs << '\t' << '\t' << '\t' << '\t' << "LISTMxCh" << '\n';
		ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Address: " << mxobc.listmxchc.address() << '\n';
		ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Length: " << mxobc.listmxchc.datasize() << '\n';
		ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Entries: " << mxobc.listmxchc.entries.size() << '\n';
		ofs << "!!!=== START ENTRY LIST (" << mxobc.listmxchc.entries.size() << " ENTRIES) ===!!!" << '\n';
		for (int i = 0; i < mxobc.listmxchc.entries.size(); i++)
		{
			printMxObInfo(mxobc.listmxchc.entries[i], ofs);
		}
		ofs << "!!!=== END ENTRY LIST (" << mxobc.listmxchc.entries.size() << " ENTRIES) ===!!!" << '\n';
	}
}

void LegoIslandRip::logRIFFOMNI(RIFFOMNIChunk& src, std::string filename)
{
	std::ofstream ofs(filename.c_str(), std::ios_base::binary | std::ios_base::trunc);
	
	ofs << "RIFF/OMNI: length " << src.datasize() << '\n';

	ofs << '\t' << "MxHd" << '\n';
	ofs << '\t' << '\t' << "Address: " << src.mxhdc.address() << '\n';
	ofs << '\t' << '\t' << "Length: " << src.mxhdc.datasize() << '\n';
	ofs << '\t' << '\t' << "unk1: " << src.mxhdc.unk1 << '\n';
	ofs << '\t' << '\t' << "unk2: " << src.mxhdc.unk2 << '\n';
	ofs << '\t' << '\t' << "unk3: " << src.mxhdc.unk3 << '\n';
	ofs << '\t' << '\t' << "unk4: " << src.mxhdc.unk4 << '\n';
	ofs << '\t' << '\t' << "unk5: " << src.mxhdc.unk5 << '\n';

	ofs << '\t' << "MxOf" << '\n';
	ofs << '\t' << '\t' << "Address: " << src.mxofc.address() << '\n';
	ofs << '\t' << '\t' << "Length: " << src.mxofc.datasize() << '\n';
	ofs << '\t' << '\t' << "Reported entries: " << src.mxofc.numentries << '\n';
	ofs << '\t' << '\t' << "Actual entries: " << src.mxofc.entries.size() << '\n';
//	ofs << '\t' << '\t' << "Address table: " << '\n';
//	for (int i = 0; i < src.mxofc.entries.size(); i++)
//	{
//		ofs << '\t' << '\t' << src.mxofc.entries[i] << '\n';
//	}

	ofs << '\t' << "LIST/MxSt" << '\n';
	ofs << '\t' << '\t' << "Address: " << src.listmxstc.address() << '\n';
	ofs << '\t' << '\t' << "Length: " << src.listmxstc.datasize() << '\n';
	ofs << '\t' << '\t' << "Entries: " << src.listmxstc.entries.size() << '\n';
	for (int i = 0; i < src.listmxstc.entries.size(); i++)
	{
		MxStChunk& mxstc = src.listmxstc.entries[i];
		ofs << '\t' << '\t' << "MxSt " << i << '\n';
		ofs << '\t' << '\t' << '\t' << "Address: " << mxstc.address() << '\n';
		ofs << '\t' << '\t' << '\t' << "Length: " << mxstc.datasize() << '\n';

		if (mxstc.mxobc.type() == MxOb)
		{
			printMxObInfo(mxstc.mxobc, ofs);
		}
		if (mxstc.listmxchc.listtype() == MxCh)
		{
			ofs << '\t' << '\t' << '\t' << "LISTMxCh" << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Address: " << mxstc.listmxchc.address() << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Length: " << mxstc.listmxchc.datasize() << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Entries: " << mxstc.listmxchc.entries.size() << '\n';
			for (int i = 0; i < mxstc.listmxchc.entries.size(); i++)
			{
				printMxObInfo(mxstc.listmxchc.entries[i], ofs);
			}
		}
		if (mxstc.listmxdac.listtype() == MxDa)
		{
			ofs << '\t' << '\t' << '\t' << "LISTMxDa" << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Address: " << mxstc.listmxdac.address() << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Length: " << mxstc.listmxdac.datasize() << '\n';
			ofs << '\t' << '\t' << '\t' << '\t' << "Entries: " << mxstc.listmxdac.entries.size() << '\n';
			for (int i = 0; i < mxstc.listmxdac.entries.size(); i++)
			{
				MxChChunk& mxchc = mxstc.listmxdac.entries[i];

				ofs << '\t' << '\t' << '\t' << '\t' << "MxCh " << i << '\n';
				ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Address: " << mxchc.address() << '\n';
				ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Length: " << mxchc.datasize() << '\n';
				ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "unk1: " << mxchc.unk1 << '\n';
				ofs << '\t' << '\t' << '\t' << '\t' << '\t' << "Obj ID: " << mxchc.thingid << '\n';
			}
		}
	}
}


};	// end of namespace LegoIsland

#endif
