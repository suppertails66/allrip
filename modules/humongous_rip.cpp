#include "humongous_structs.h"
#include "humongous_rip.h"

#include "../utils/MembufStream.h"
#include "../utils/BitStream.h"
#include "../utils/BitmapData.h"
#include "../utils/PCMData.h"
#include "../utils/DatManip.h"
#include "../utils/ErrorLog.h"
#include "../utils/logger.h"
#include "../RipperFormats.h"
#include "common.h"
#include <fstream>
#include <string>
#include <cstring>
#include <iostream>

using namespace RipUtil;
using namespace RipperFormats;
using namespace ErrLog;
using namespace Logger;

namespace Humongous
{


// hack to work around a change in semantics of BMAP/SMAP RLE encoding (codes 8 and 9) 
// between 3DO and later games
// in 3DO, this is an unlined data format; in later games, it is lined
// we test the first n such images decoded and remember the results for later entries
RLEEncodingMethodHackValue rle_encoding_method_hack = rle_hack_is_not_set;
int rle_encoding_method_hack_images_to_test = 10;
int rle_encoding_method_hack_lined_images = 0;
int rle_encoding_method_hack_unlined_images = 0;
bool rle_encoding_method_hack_was_user_overriden = false;

// similarly, for handling nominally 2-color AKOS
AKOS2ColorDecodingHackValue akos_2color_decoding_hack = akos_2color_hack_is_not_set;
int akos_2color_decoding_hack_images_to_test = 10;
int akos_2color_decoding_hack_rle_images = 0;
int akos_2color_decoding_hack_bitmap_images = 0;
bool akos_2color_decoding_hack_was_user_overriden = false;

// misc stuff
bool cleared_tlke_file = false;


void rip_rmim(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results, int transind)
{
	for (std::vector<RMIMChunk>::size_type i = 0; 
		i < lflfc.rmim_chunk.images.size(); i++)
	{
		const IMxxChunk& imxxc = lflfc.rmim_chunk.images[i];
		
		BitmapData bmp;
		decode_imxx(imxxc, bmp, lflfc.rmhd_chunk.width, lflfc.rmhd_chunk.height,
			lflfc.trns_chunk.trns_val, transind);
		if (lflfc.apals.size() == 1)	// one palette: use abbreviated filenames
		{
			bmp.set_palettized(true);
			bmp.set_palette(lflfc.apals[0]);
			write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-rmim-" 
				+ to_string(i) + ".bmp");
			++results.graphics_ripped;
		}
		else if (lflfc.apals.size())	// multiple palettes: use full filenames
		{
			bmp.set_palettized(true);
			for (std::vector<BitmapPalette>::size_type j = 0; 
				j < lflfc.apals.size(); j++)
			{
				bmp.set_palette(lflfc.apals[j]);
				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-rmim-" 
					+ to_string(i) + "-apal-" + to_string(j) + ".bmp");
				++results.graphics_ripped;
			}
		}
	}
}

void rip_obim(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results, int transind)
{
	for (std::map<int, OBIMChunk>::const_iterator obim_it = lflfc.obim_chunks.begin(); 
			obim_it != lflfc.obim_chunks.end(); obim_it++)
		{
			std::map<int, OBCDChunk>::const_iterator obcd_it 
				= lflfc.obcd_chunks.find((*obim_it).first);
			// check if this OBIM has a corresponding OBCD
			if (obcd_it != lflfc.obcd_chunks.end())
			{
				int width = (*obcd_it).second.cdhd_chunk.width;
				int height = (*obcd_it).second.cdhd_chunk.height;
				for (std::vector<IMxxChunk>::size_type i = 0;
					i < (*obim_it).second.images.size(); i++)
				{
					BitmapData bmp;
					decode_imxx((*obim_it).second.images[i], bmp, width, height,
						lflfc.trns_chunk.trns_val, transind);
					if (lflfc.apals.size() == 1)
					{
						bmp.set_palettized(true);
						bmp.set_palette(lflfc.apals[0]);
						write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-obim-" 
							+ to_string((*obim_it).first) + "-im-" + to_string(i) + ".bmp");
						++results.graphics_ripped;
					}
					else if (lflfc.apals.size())
					{
						bmp.set_palettized(true);
						for (std::vector<BitmapPalette>::size_type j = 0;
							j < lflfc.apals.size(); j++)
						{
							bmp.set_palette(lflfc.apals[j]);
							write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-obim-" 
								+ to_string((*obim_it).first) + "-im-" + to_string(i) 
								+ "-apal-" + to_string(j) + ".bmp");
							++results.graphics_ripped;
						}
					}
				}
			}
			else	// OBIM has no OBCD
			{
				logger.error("OBIM with ID " + to_string((*obim_it).first) + " has no corresponding OBCD");
			}
		}
}

void rip_akos(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results, int transind)
{
	for (std::vector<AKOSChunk>::size_type i = 0;
		i < lflfc.akos_chunks.size(); i++)
	{
		const AKOSChunk& akosc = lflfc.akos_chunks[i];

		for (std::vector<AKOFEntry>::size_type j = 0; j < akosc.akof_entries.size(); j++)
		{
			BitmapData bmp;

			// if AKOS has a full palette, use its local palette
			if (akosc.palette.size() == 256)
			{
				// if REMP chunk exists, remap colors
				if (lflfc.remp_chunk.type == remp)
					decode_akos(akosc, bmp, j, akosc.palette,
						lflfc.trns_chunk.trns_val, transind,
						akosc.colormap, true,
						lflfc.remp_chunk.colormap, true);
				else
					decode_akos(akosc, bmp, j, akosc.palette,
						lflfc.trns_chunk.trns_val, transind,
						akosc.colormap, true,
						lflfc.remp_chunk.colormap, false);
				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-akos-" 
					+ to_string(i) + "-im-" + to_string(j) + ".bmp");
				++results.animation_frames_ripped;
			} 
			// if palette not full, use room palette(s)
			else if (lflfc.apals.size() == 1)
			{
				// if REMP chunk exists, remap colors
				if (lflfc.remp_chunk.type == remp)
					decode_akos(akosc, bmp, j, lflfc.apals[0],
						lflfc.trns_chunk.trns_val, transind,
						akosc.colormap, true,
						lflfc.remp_chunk.colormap, true);
				else
					decode_akos(akosc, bmp, j, lflfc.apals[0],
						lflfc.trns_chunk.trns_val, transind,
						akosc.colormap, true,
						lflfc.remp_chunk.colormap, false);
				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-akos-" 
					+ to_string(i) + "-im-" + to_string(j) + ".bmp");
				++results.animation_frames_ripped;
			}
			else if (lflfc.apals.size())
			{
				for (std::vector<BitmapPalette>::size_type k = 0;
					k < lflfc.apals.size(); k++)
				{
					// if REMP chunk exists, remap colors
					if (lflfc.remp_chunk.type == remp)
						decode_akos(akosc, bmp, j, lflfc.apals[k],
							lflfc.trns_chunk.trns_val, transind,
							akosc.colormap, true,
							lflfc.remp_chunk.colormap, true);
					else
						decode_akos(akosc, bmp, j, lflfc.apals[k],
							lflfc.trns_chunk.trns_val, transind,
							akosc.colormap, true,
							lflfc.remp_chunk.colormap, false);
					write_bitmapdata_8bitpalettized_bmp(bmp, fprefix + "-akos-" 
						+ to_string(i) + "-im-" + to_string(j)
						+ "-apal-" + to_string(k) + ".bmp");
				++results.animation_frames_ripped;
				}
			}
		}

		// rip AKAX frames
		const AKAXChunk& akaxc = akosc.akax_chunk;

		BitmapData bmp(640, 480, 8, true);
		bmp.set_palette(lflfc.apals[0]);
		bmp.clear(lflfc.trns_chunk.trns_val);

		for (std::vector<AUXDChunk>::size_type j = 0; 
			j < akaxc.auxd_chunks.size(); j++)
		{
			const AUXDChunk& auxdc = akaxc.auxd_chunks[j];
			if (auxdc.axfd_chunk.imgdat_size != 0)
			{
				decode_auxd(auxdc, bmp, lflfc.trns_chunk.trns_val, transind);

				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
					+ "-akos-" + to_string(i)
					+ "-auxd-" + to_string(j)
					+ ".bmp");

				++results.animation_frames_ripped;
			}
		}
		++results.animations_ripped;
	}
}

void rip_awiz(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results, int transind)
{
	// rewrite these to call a common function

	// rip regular AWIZ
	for (std::vector<AWIZChunk>::size_type i = 0;
		i < lflfc.awiz_chunks.size(); i++)
	{
		const AWIZChunk& awizc = lflfc.awiz_chunks[i];

		// some games have "empty" AWIZs just to make us mad
		if (awizc.wizd_chunk.type == wizd)
		{

			BitmapData bmp;

			if (awizc.palette.size())
			{
				decode_awiz(awizc, bmp, awizc.palette,
					lflfc.trns_chunk.trns_val, transind);
				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
					+ "-awiz-" + to_string(i) + ".bmp");
				++results.graphics_ripped;
			}
			else if (lflfc.apals.size() == 1)
			{
				decode_awiz(awizc, bmp, lflfc.apals[0],
					lflfc.trns_chunk.trns_val, transind);
				write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
					+ "-awiz-" + to_string(i) + ".bmp");
				++results.graphics_ripped;
			}
			else if (lflfc.apals.size())
			{
				for (std::vector<BitmapPalette>::size_type j = 0;
					j < lflfc.apals.size(); j++)
				{
					decode_awiz(awizc, bmp, lflfc.apals[j],
						lflfc.trns_chunk.trns_val, transind);
					write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
						+ "-awiz-" + to_string(i) 
						+ "-apal-" + to_string(j) + ".bmp");
					++results.graphics_ripped;
				}
			}
		}
	}

	// rip MULT-embedded AWIZ
	for (std::vector<MULTChunk>::size_type i = 0; 
		i < lflfc.mult_chunks.size(); i++)
	{
		const MULTChunk& multc = lflfc.mult_chunks[i];

		for (std::vector<AWIZChunk>::size_type j = 0;
			j < multc.awiz_chunks.size(); j++)
		{
			const AWIZChunk& awizc = multc.awiz_chunks[j];

			if (awizc.wizd_chunk.type == wizd)
			{

				BitmapData bmp;

				if (multc.defa_chunk.palette.size() != 0)
				{
					decode_awiz(awizc, bmp, multc.defa_chunk.palette,
						lflfc.trns_chunk.trns_val, transind,
						multc.defa_chunk.rmap_chunk.colormap, multc.defa_chunk.rmap_chunk.colormap.size() != 0);
					write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
						+ "-mult-" + to_string(i)
						+ "-awiz-" + to_string(j) + ".bmp");
					++results.graphics_ripped;
				}
				else 
				if (awizc.palette.size() != 0)
				{
					decode_awiz(awizc, bmp, awizc.palette,
						lflfc.trns_chunk.trns_val, transind,
						multc.defa_chunk.rmap_chunk.colormap, multc.defa_chunk.rmap_chunk.colormap.size() != 0);
					write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
						+ "-mult-" + to_string(i)
						+ "-awiz-" + to_string(j) + ".bmp");
					++results.graphics_ripped;
				}
				else if (lflfc.apals.size() == 1)
				{
					decode_awiz(awizc, bmp, lflfc.apals[0],
						lflfc.trns_chunk.trns_val, transind,
						multc.defa_chunk.rmap_chunk.colormap, multc.defa_chunk.rmap_chunk.colormap.size() != 0);
					write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
						+ "-mult-" + to_string(i)
						+ "-awiz-" + to_string(j) + ".bmp");
					++results.graphics_ripped;
				}
				else if (lflfc.apals.size() != 0)
				{
					for (std::vector<BitmapPalette>::size_type k = 0;
						k < lflfc.apals.size(); k++)
					{
						decode_awiz(awizc, bmp, lflfc.apals[k],
							lflfc.trns_chunk.trns_val, transind,
							multc.defa_chunk.rmap_chunk.colormap, multc.defa_chunk.rmap_chunk.colormap.size() != 0);
						write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
							+ "-mult-" + to_string(i)
							+ "-awiz-" + to_string(j) 
							+ "-apal-" + to_string(k) + ".bmp");
						++results.graphics_ripped;
					}
				}
			}
		}
	}
}

void rip_char(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results, int transind)
{
	// chars have variable palettes; this one is arbitrary, but
	// should at least provide distinct tones
	BitmapPalette testpal;
	for (int k = 0; k < 256; k++)
		testpal[k] = (k | (k << 8) | (k << 16));
	testpal[0] = 0x000000;
	testpal[1] = 0xFFFFFE;
	testpal[2] = 0x555554;
	testpal[3] = 0xAAAAA9;
	testpal[4] = 0x888887;
	testpal[lflfc.trns_chunk.trns_val] = 0xAB00AB;	// background
	for (std::vector<CHARChunk>::size_type i = 0;
		i < lflfc.char_chunks.size(); i++)
	{
		const CHARChunk& charc = lflfc.char_chunks[i];
		for (std::vector<CHAREntry>::size_type j = 0;
			j < charc.char_entries.size(); j++)
		{
			const CHAREntry& chare = charc.char_entries[j];
			BitmapData bmp;
			decode_char(bmp, chare, charc.compr, testpal,
				lflfc.trns_chunk.trns_val, transind);
			write_bitmapdata_8bitpalettized_bmp(bmp, fprefix
				+ "-char-" + to_string(i)
				+ "-num-" + to_string(j)
				+ ".bmp");
			++results.graphics_ripped;
		}
	}
}

void rip_sound(std::vector<SoundChunk>& sound_chunks, 
	const RipperFormats::RipperSettings& ripset, const std::string& fprefix,
	RipperFormats::RipResults& results)
{
	for (std::vector<SoundChunk>::size_type i = 0;
		i < sound_chunks.size(); i++)
	{
		SoundChunk& soundc = sound_chunks[i];

		if (ripset.normalize)
			soundc.wave.normalize();

		write_pcmdata_wave(soundc.wave, fprefix
			+ to_string(i)
			+ ".wav",
			ripset.ignorebytes,
			ripset.ignoreend);

		++results.audio_ripped;
	}
}

void rip_wsou(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& fprefix, RipperFormats::RipResults& results,
	bool decode_audio)
{
	for (std::vector<WSOUChunk>::size_type i = 0;
		i < lflfc.wsou_chunks.size(); i++)
	{
		const RIFFEntry& riff_entry = lflfc.wsou_chunks[i].riff_entry;

		if (!decode_audio)
		{
			std::ofstream ofs((fprefix
				+ "-wsou-" + to_string(i)
				+ ".wav").c_str(), std::ios_base::binary);
			ofs.write(riff_entry.riffdat, riff_entry.riffdat_size);
		}
		else
		{
			// who would do this to us :(
			PCMData wave;
			CommFor::riff::decode_riff(riff_entry.riffdat, riff_entry.riffdat_size,
				wave);
			if (ripset.normalize)
				wave.normalize();
			write_pcmdata_wave(wave, fprefix
				+ "-wsou-" + to_string(i)
				+ ".wav",
				ripset.ignorebytes,
				ripset.ignoreend);
		}

		++results.audio_ripped;
	}
}

void rip_tlke(LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& filename, int rmnum, RipperFormats::RipResults& results)
{
	// only create the TLKE file if there are TLKEs to rip
	if (!cleared_tlke_file)
	{
		std::ofstream(filename.c_str(), std::ios_base::trunc);
		cleared_tlke_file = true;
	}

	std::ofstream ofs(filename.c_str(), std::ios_base::app);

	ofs << "room " << rmnum << '\n';
	for (std::vector<TLKEChunk>::size_type i = 0;
		i < lflfc.tlke_chunks.size(); i++)
	{
		ofs << '\t' << lflfc.tlke_chunks[i].text_chunk.text << '\n';
		++results.strings_ripped;
	}
	ofs << '\n';
}

void rip_metadata(const LFLFChunk& lflfc, const RipperFormats::RipperSettings& ripset,
	const std::string& filename, int rmnum, RipperFormats::RipResults& results)
{
	std::ofstream ofs(filename.c_str(), std::ios_base::app);
	ofs << "room " << rmnum << '\n';
	
	ofs << '\t' << "TRNS: " << lflfc.trns_chunk.trns_val << '\n';

	for (std::vector<AKOSChunk>::size_type i = 0;
		i < lflfc.akos_chunks.size(); i++)
	{
		const AKOSChunk& akosc = lflfc.akos_chunks[i];

		if (akosc.file_date.size()
			|| akosc.file_name.size()
			|| akosc.file_compr.size()
			|| akosc.sqdb_chunk.seqi_chunks.size())
		{
			ofs << '\t' << "AKOS " << i << ":" << '\n';
			if (akosc.file_date.size())
			{
				ofs << '\t' << '\t' << "SP2C: " << akosc.file_date << '\n';
			}
			if (akosc.file_name.size())
			{
				ofs << '\t' << '\t' << "SPLF: " << akosc.file_name << '\n';
			}
			if (akosc.file_compr.size())
			{
				ofs << '\t' << '\t' << "CLRS: " << akosc.file_compr << '\n';
			}
			if (akosc.sqdb_chunk.seqi_chunks.size())
			{
				ofs << '\t' << '\t' << "SQDB:" << '\n';
				for (std::vector<SEQIChunk>::size_type j = 0;
					j < akosc.sqdb_chunk.seqi_chunks.size(); j++)
				{
					const SEQIChunk& seqic = akosc.sqdb_chunk.seqi_chunks[j];

					ofs << '\t' << '\t' << '\t' << "SEQI " << j << ": "
						<< seqic.name_val << '\n';
				}
			}
		}
	}

	for (std::map<ObjectID, OBCDChunk>::const_iterator it = lflfc.obcd_chunks.begin();
		it != lflfc.obcd_chunks.end(); it++)
	{
		const OBCDChunk& obcdc = (*it).second;

		if (obcdc.obna_val.size())
		{
			ofs << '\t' << "OBCD " << (*it).first << ":" << '\n';

			ofs << '\t' << '\t' << "OBNA: " << obcdc.obna_val << '\n';
		}
	}

	ofs << '\n';
}



void decode_imxx(const IMxxChunk& imxxc, RipUtil::BitmapData& bitmap, int width, int height,
	int localtransind, int transind)
{
	if (imxxc.image_chunk.type == smap)
	{
		decode_smap(imxxc.image_chunk, bitmap, width, height, localtransind, transind);
	}
	else if (imxxc.image_chunk.type == bmap)
	{
		decode_bmap(imxxc.image_chunk, bitmap, width, height, localtransind, transind);
	}
	else if (imxxc.image_chunk.type == bomp)
	{
		decode_bomp(imxxc.image_chunk, bitmap, localtransind, transind);
	}
	else
	{
		logger.error("\tunrecognized bitmap subtype " + imxxc.image_chunk.name);
	}
}

void decode_bmap(const SputmChunk& bmapc, RipUtil::BitmapData& bmap, int width, int height,
	int localtransind, int transind)
{
	bmap.resize_pixels(width, height, 8);
	bmap.clear(transind);
	int encoding = to_int(bmapc.data + 8, 1);
	decode_encoded_bitmap(bmapc.data + 9, encoding, bmapc.datasize - 9, bmap,
		0, 0, width, height, localtransind, transind);
}

void decode_smap(const SputmChunk& smapc, RipUtil::BitmapData& bmap, int width, int height,
	int localtransind, int transind)
{
	// width and height are always rounded down to a multiple of 8... not??
//	bmap.resize_pixels(width/8*8, height/8*8, 8);
	bmap.resize_pixels(width, height, 8);
	bmap.clear(transind);
	int strips = width/8;
	char* offset = smapc.data + 8;

	for (int i = 0; i < strips; i++)
	{
		int offset_int = to_int(offset, 4, DatManip::le);
		int encoding = to_int(smapc.data + offset_int, 1);
		decode_encoded_bitmap(smapc.data + offset_int + 1, encoding,
			smapc.datasize - offset_int, bmap, i * 8, 0, 8, height,
			localtransind, transind);
		offset += 4;
	}
}

void decode_bomp(const SputmChunk& bompc, RipUtil::BitmapData& bmap, int localtransind, 
	int transind, ColorMap colormap, bool deindex)
{
	char* data = bompc.data + 8;
	int unknown = to_int(data++, 1);
	int bomptrans = to_int(data++, 1);
	int width = to_int(data, 2, DatManip::le);
	data += 2;
	int height = to_int(data, 2, DatManip::le);
	data += 2;
	int unknown3 = to_int(data, 2, DatManip::le);
	data += 2;
	int unknown4 = to_int(data, 2, DatManip::le);
	data += 2;

	bmap.resize_pixels(width, height, 8);
	bmap.clear(transind);

	int currx = 0;
	int curry = 0;

	int pos = 0;
	int next_pos = pos;
	int datlen = bompc.datasize - 18;
	while (pos < datlen && curry < height)
	{
		int bytecount = to_int(data + next_pos, 2, DatManip::le);
		pos = next_pos + 2;
		next_pos += bytecount + 2;
		while (pos < datlen && pos < next_pos)
		{
			int code = to_int(data + pos, 1);
			++pos;

			if (code & 1)		// encoded run
			{
				int count = (code >> 1) + 1;
				int color = to_int(data + pos, 1);
				++pos;
				if (color != bomptrans)
				{
					if (deindex)
						color = colormap[color];
					bmap.draw_row(color, count, currx, curry, 0, 0, width, height);
				}
				currx += count;
			}
			else				// absolute run
			{
				int count = (code >> 1) + 1;
				for (int i = 0; i < count; i++)
				{
					int color = to_int(data + pos, 1);
					if (color != bomptrans)
					{
						if (deindex)
							color = colormap[color];
						bmap.draw_row(color, 1, currx, curry, 0, 0, width, height);
					}
					++pos;
					++currx;
				}
			}
		}
		currx = 0;
		++curry;
	}
}

void decode_bomp(const SputmChunk& bompc, RipUtil::BitmapData& bmap, int localtransind, int transind)
{
	decode_bomp(bompc, bmap, localtransind, transind, dummy_colormap, false);
}



// decode AKOS (any palette, any colormap, deindexed or indexed
void decode_akos(const AKOSChunk& akosc, RipUtil::BitmapData& bmap,
	int entrynum, RipUtil::BitmapPalette palette, int localtransind, int transind,
	ColorMap colormap, bool deindex, ColorMap colorremap, bool remap)
{
	bmap.resize_pixels(akosc.akcd_entries[entrynum].width, 
		akosc.akcd_entries[entrynum].height, 8);
	bmap.set_palettized(true);
	bmap.set_palette(palette);
	bmap.clear(transind);

	// in "some" games, nominally 2-color AKOSs actually use bitmap encoding
	// however, "other" games, mostly newer ones, use lined RLE
	// unfortunately, there seems to be no distinction between these
	// in the data files, so we have to do some guesswork

	if (akosc.numcolors == 2)
	{
		if (!akos_2color_decoding_hack_was_user_overriden
			&& akos_2color_decoding_hack_images_to_test)
		{
			if (is_lined_rle(akosc.akcd_entries[entrynum].imgdat,
				akosc.akcd_entries[entrynum].size))
			{
				++akos_2color_decoding_hack_rle_images;
			}
			else
			{
				++akos_2color_decoding_hack_bitmap_images;
			}

			--akos_2color_decoding_hack_images_to_test;

			AKOS2ColorDecodingHackValue newval;

			if (akos_2color_decoding_hack_rle_images
				>= akos_2color_decoding_hack_bitmap_images)
			{
				newval = akos_2color_hack_always_use_rle;
			}
			else
			{
				newval = akos_2color_hack_always_use_bitmap;
			}

			if (akos_2color_decoding_hack != akos_2color_hack_is_not_set
				&& newval != akos_2color_decoding_hack)
			{
				logger.warning("changing value of AKOS 2-color encoding method hack. "
					"Initial images were probably incorrectly ripped; "
					"to rip these, try using --force_akos2c_rle or "
					"--force_akos2c_bitmap");
			}

			akos_2color_decoding_hack = newval;

		}
		if (akos_2color_decoding_hack == akos_2color_hack_always_use_rle)
		{
			decode_lined_rle(akosc.akcd_entries[entrynum].imgdat, akosc.akcd_entries[entrynum].size,
				bmap, 0, 0, akosc.akcd_entries[entrynum].width, akosc.akcd_entries[entrynum].height,
				akosc.akpl_chunk.alttrans, transind, true, dummy_colormap, false);
		}
		else if (akos_2color_decoding_hack == akos_2color_hack_always_use_bitmap)
		{
			int encoding = akosc.akcd_entries[entrynum].imgdat[0];

			// check if data is actually lined RLE before accepting encoding
			if ((encoding != 8 
				|| (encoding == 8 && is_lined_rle(akosc.akcd_entries[entrynum].imgdat, akosc.akcd_entries[entrynum].size)))
				&& !akos_2color_decoding_hack_was_user_overriden)
			{
				logger.warning("guessed bitmap encoding for 2-color AKOS, but specified "
					"encoding was not 8. "
					"Image data will be treated as lined RLE instead; if problems result, "
					"try using --force_akos2c_bitmap");

				decode_lined_rle(akosc.akcd_entries[entrynum].imgdat, akosc.akcd_entries[entrynum].size,
					bmap, 0, 0, akosc.akcd_entries[entrynum].width, akosc.akcd_entries[entrynum].height,
					akosc.akpl_chunk.alttrans, transind, true, dummy_colormap, false);
			}
			else if (encoding == 8 || akos_2color_decoding_hack_was_user_overriden)
			{
				// all games seem to use an encoding of 0x8 with the semantics
				// of encoding 0x58 + transparent color 0, possibly combined with an REMP
				encoding = 0x58;
				decode_bitstream_img(akosc.akcd_entries[entrynum].imgdat + 1,
					akosc.akcd_entries[entrynum].size - 1, bmap, 0, 0,
					akosc.akcd_entries[entrynum].width, akosc.akcd_entries[entrynum].height,
					8, 3, true, true, false,
					akosc.akpl_chunk.alttrans, transind, colorremap, remap);
			}
			else
			{
				logger.error("2-color AKOS has invalid encoding " + to_string(encoding)
					+ " and is not lined RLE");
			}
		}
	}
	else
	{
		decode_multicomp_rle(akosc.akcd_entries[entrynum].imgdat, akosc.akcd_entries[entrynum].width,
			akosc.akcd_entries[entrynum].height, bmap, palette, akosc.numcolors, localtransind, transind,
			colormap, deindex, colorremap, remap);
	}
}

void decode_akos(const AKOSChunk& akosc, RipUtil::BitmapData& bmap,
	int entrynum, RipUtil::BitmapPalette palette, int localtransind, int transind)
{
	decode_akos(akosc, bmap, entrynum, palette, localtransind, transind,
		dummy_colormap, false, dummy_colormap, false);
}

void decode_akos(const AKOSChunk& akosc, RipUtil::BitmapData& bmap,
	int entrynum, RipUtil::BitmapPalette palette, int localtransind, int transind,
	ColorMap colormap, bool deindex)
{
	decode_akos(akosc, bmap, entrynum, palette, localtransind, transind,
		colormap, deindex, dummy_colormap, false);
}

void decode_auxd(const AUXDChunk& auxdc, RipUtil::BitmapData& bmap,
	int localtransind, int transind)
{
	const AXFDChunk& axfdc = auxdc.axfd_chunk;

	decode_type2_lined_rle(axfdc.imgdat, axfdc.imgdat_size, bmap, 
		axfdc.off1 + 320, axfdc.off2 + 240, axfdc.width, axfdc.height, localtransind, transind);
}

// utility function to detect whether data uses a lined format
bool is_lined(const char* data, int datlen)
{
	// total the line counts until we exceed datlen
	int count = 0;
	int pos = 0;
	while (pos < datlen)
	{
		// entries are null-terminated; datlen isn't always accurate
		if (*(data + pos) == 0)
			return true;

		int jump = to_int(data + pos, 2, DatManip::le) + 2;
		count += jump;
		pos += jump;
	}
	// if lined, total should be datlen
	if (count == datlen)
		return true;
	else
		return false;
}

bool is_lined_rle(const char* data, int datlen)
{
	int pos = 0;

	if (datlen < 2)
		return false;

	while (pos < datlen - 1)
	{
		int jump = to_int(data + pos, 2, DatManip::le) + 2;
		pos += jump;
	}

	// entries are null-terminated; datlen isn't always accurate
	if ((pos >= datlen - 1) && pos <= datlen)
		return true;

	return false;
}



void decode_awiz(const AWIZChunk& awizc, RipUtil::BitmapData& bmap, 
	RipUtil::BitmapPalette palette, int localtransind, int transind,
	ColorMap colormap, bool deindex)
{
	bmap.resize_pixels(awizc.width, awizc.height, 8);
	bmap.clear(transind);
	bmap.set_palettized(true);
	bmap.set_palette(palette);

	// quick'n'dirty uncompressed image detection
	if (awizc.width * awizc.height == awizc.wizd_chunk.size - 8)
	{
		int* putpos = bmap.get_pixels();
		int numpix = awizc.wizd_chunk.size - 8;
		for (int i = 0; i < numpix; i++)
			*putpos++ = to_int(awizc.wizd_chunk.data + 8 + i, 1);
	}
	else
	{
		decode_lined_rle(awizc.wizd_chunk.data + 8, awizc.wizd_chunk.datasize, bmap, 
			0, 0, awizc.width, awizc.height, localtransind, transind, true, colormap, 
			deindex);
	}
}

void decode_awiz(const AWIZChunk& awizc, RipUtil::BitmapData& bmap, 
	RipUtil::BitmapPalette palette, int localtransind, int transind)
{
	decode_awiz(awizc, bmap, palette, localtransind, transind, dummy_colormap, false);
}



void decode_char(RipUtil::BitmapData& bmap, const CHAREntry& chare,
	int compr, const RipUtil::BitmapPalette& palette, int localtransind,
	int transind, ColorMap colormap, bool deindex)
{
	LRBitStream bits(chare.data, chare.datalen);

	bmap.resize_pixels(chare.width, chare.height, 8);
	bmap.set_palettized(true);
	bmap.set_palette(palette);
	bmap.clear(transind);

	if (compr == 1 || compr == 2 || compr == 4)		// uncompressed bitmap, n bpp
	{
		int* pix = bmap.get_pixels();
		int numpix = chare.width * chare.height;
		for (int i = 0; i < numpix; i++)
		{
			int color = bits.get_nbit_int(compr);
			if (color != 0)
				*pix++ = color;
			else
				*pix++;
		}
	}
	else if (compr == 0 || compr == 8)				// lined RLE
	{
		decode_lined_rle(chare.data, chare.datalen, bmap,
			0, 0, chare.width, chare.height, localtransind, transind, true);
	}
	else
	{
		logger.error("\tunrecognized CHAR encoding " + to_string(compr));
	}
}

void decode_char(RipUtil::BitmapData& bmap, const CHAREntry& chare,
	int compr, const RipUtil::BitmapPalette& palette, int localtransind,
	int transind)
{
	decode_char(bmap, chare, compr, palette, localtransind,
		transind, dummy_colormap, false);
}



void decode_encoded_bitmap(char* data, int encoding, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind)
{
	bool rle = false;		// uses RLE encoding?
	bool horiz = false;		// draws horizontal or vertical?
	bool trans = false;		// has transparency?
	bool exprange = false;	// expanded range for 3-bit relative palette set?
							// ([-4, -1] and [1, 4] instead of [-4, 3])
	int bpabsol;			// bits per absolute palette set
	int bprel;				// bits per relative palette set

	if (encoding == 1 || encoding == 149)	// uncompressed: 1 byte per pixel
	{
		rle = false;
		trans = false;
	}
	else if (encoding == 8)		// RLE, with transparency
	{
		rle = true;
		trans = true;
	}
	else if (encoding == 9)		// RLE, no transparency
	{
		rle = true;
		trans = false;
	}
	else if (encoding == 143)	// same as 150??
	{
		rle = false;
		trans = false;
	}
	else if (encoding == 150)	// always 1 byte giving fill color?
	{
		rle = false;
		trans = false;
	}
	else
	{
		rle = false;

		if (encoding >= 0xE && encoding <= 0x12)		// group 2
		{
			horiz = false;
			trans = false;
		}
		else if (encoding >= 0x18 && encoding <= 0x1C)	// group 3
		{
			horiz = true;
			trans = false;
		}
		else if (encoding >= 0x22 && encoding <= 0x26)	// group 4
		{
			horiz = false;
			trans = true;
		}
		else if (encoding >= 0x2C && encoding <= 0x30)	// group 5
		{
			horiz = true;
			trans = true;
		}
		else if (encoding >= 0x40 && encoding <= 0x44)	// group 6
		{
			horiz = true;
			trans = false;
		}
		else if (encoding >= 0x54 && encoding <= 0x58)	// group 7
		{
			horiz = true;
			trans = true;
		}
		else if (encoding >= 0x68 && encoding <= 0x6C)	// group 8
		{
			horiz = true;
			trans = false;
		}
		else if (encoding >= 0x7C && encoding <= 0x80)	// group 9
		{
			horiz = true;
			trans = true;
		}
		else if (encoding >= 0x86 && encoding <= 0x8A)	// group 10
		{
			horiz = true;
			trans = false;
			exprange = true;
		}
		else if (encoding >= 0x90 && encoding <= 0x94)	// group 11
		{
			horiz = true;
			trans = true;
			exprange = true;
		}
		else
		{
			logger.error("\tunrecognized bitmap encoding " + to_string(encoding));
			return;
		}

		bpabsol = encoding % 10;

		if (encoding <= 0x30)
			bprel = 1;
		else
			bprel = 3;
	}

	if (encoding == 1 || encoding == 149)			// uncompressed
	{
		decode_uncompressed_img(data, datlen, bmap, x, y, width, height, true, false, 
			localtransind, transind);
	}
	else if (encoding == 143 || encoding == 150)	// solid fill (probably)
	{
		int fillcolor = to_int(data, 1);
		bmap.clear(fillcolor);
	}
	else if (rle)			// RLE
	{
		// see hack explanation at start of file
		if (!rle_encoding_method_hack_was_user_overriden
			&& rle_encoding_method_hack_images_to_test)
		{
			if (is_lined_rle(data, datlen))
				++rle_encoding_method_hack_lined_images;
			else
				++rle_encoding_method_hack_unlined_images;

			--rle_encoding_method_hack_images_to_test;

			RLEEncodingMethodHackValue newval;

			// set encoding method to whichever type is in majority
			// (guessing lined if a tie)
			if (rle_encoding_method_hack_lined_images
				>= rle_encoding_method_hack_unlined_images)
			{
				newval = rle_hack_always_use_lined;
			}
			else
			{
				newval = rle_hack_always_use_unlined;
			}

			if (rle_encoding_method_hack != rle_hack_is_not_set
				&& newval != rle_encoding_method_hack)
			{	
				logger.warning("changing value of RLE encoding method hack. "
					"Initial images were probably incorrectly ripped; "
					"to rip these, try using --force_lined_rle or "
					"--force_unlined_rle");
			}
			rle_encoding_method_hack = newval;
		}

		if (rle_encoding_method_hack == rle_hack_always_use_lined)
			decode_lined_rle(data, datlen, bmap, x, y, width, height,
				localtransind, transind, trans);
		else if (rle_encoding_method_hack == rle_hack_always_use_unlined)
			decode_unlined_rle(data, datlen, bmap, x, y, width, height, 
				localtransind, transind, trans); 

/*	I sure wish this code worked	*/
/*		if (is_lined_rle(data, datlen))
			decode_lined_rle(data, datlen, bmap, x, y, width, height,
				localtransind, transind, trans);
		else
			decode_unlined_rle(data, datlen, bmap, x, y, width, height, 
				localtransind, transind, trans); */
	}
	else					// bitstream
	{
		decode_bitstream_img(data, datlen, bmap, x, y, width, height,
			bpabsol, bprel, horiz, trans, exprange, localtransind, transind);
	}
}

void decode_unlined_rle(const char* data, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind, bool trans)
{
	decode_unlined_rle(data, datlen, bmap, x, y, width, height,
		localtransind, transind, trans, dummy_colormap, false);
}

void decode_unlined_rle(const char* data, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind, bool trans,
	ColorMap colormap, bool deindex)
{
	RipUtil::DrawPos pos = { 0, 0 };

	const char* gpos = data;
	while (pos.y < height)
	{
		unsigned char code = *gpos++;
		unsigned int runlen = (code >> 1) + 1;

		if (code & 1)		// encoded run
		{
			unsigned int color = *gpos++;
			if (deindex)
				color = colormap[color];
			if (trans && color == localtransind)
				color = transind;
			draw_and_update_pos(bmap, pos, color, runlen, x, y, width, height, true, false);
		}
		else				// absolute run
		{
			for (unsigned int i = 0; i < runlen; i++)
			{
				unsigned int color = *gpos++;
				if (deindex)
					color = colormap[color];
				if (trans && color == localtransind)
					color = transind;
				draw_and_update_pos(bmap, pos, color, 1, x, y, width, height, true, false);
			}
		}
	}
}

void decode_multicomp_rle(const char* data, int width, int height, RipUtil::BitmapData& bmap,
	RipUtil::BitmapPalette palette, int clrcmp, int localtransind, int transind, 
	const ColorMap& colormap, bool deindex, const ColorMap& colorremap, bool remap)
{
	if (clrcmp == 16 || clrcmp == 32 || clrcmp == 64)
	{
		int clrmask;
		int runmask;
		int clrshift;
		switch(clrcmp)
		{
		case 16:
			clrmask = 0xF0;
			runmask = 0xF;
			clrshift = 4;
			break;
		case 32:
			clrmask = 0xF8;
			runmask = 0x7;
			clrshift = 3;
			break;
		case 64:
			clrmask = 0xFC;
			runmask = 0x3;
			clrshift = 2;
			break;
		}
		
		int totalpix = width * height;
		int drawn = 0;
		int x = 0;
		int y = 0;
		while (drawn < totalpix)
		{
			int code = to_int(data++, 1);
			int color = (code & clrmask) >> clrshift;
			int runlen = code & runmask;
			if (runlen == 0)
			{
				runlen = to_int(data++, 1);
			}
			if (color != 0)
			{
				// some games index into a reduced palette instead of the full
				// 256 color range given in the palette index chunk
				if (deindex)
					color = colormap[color];
				// some games additionally remap the deindexed colors into
				// another index into the room palette
				if (remap)
					color = colorremap[color];
				RipUtil::DrawPos result = bmap.draw_col_wrap(color, runlen, x, y);
				x = result.x;
				y = result.y;
			}
			else
			{
				y += runlen;
				if (y >= height)
				{
					x += y/height;
					y = y % height;
				}
			}
			drawn += runlen;
		} 
	}
	else if (clrcmp == 256)
	{
		int x = 0;
		int y = 0;

		const char* nextstart = data;
		while (y < height)
		{
			int bytecount = to_int(nextstart, 2, DatManip::le);
			data = nextstart + 2;
			nextstart += bytecount + 2;
			while (data < nextstart)
			{
				int code = to_int(data++, 1);
				if (code & 1)		// skip count
				{
					x += (code >> 1);
				}
				else if (code & 2)	// encoded run
				{
					int count = (code >> 2) + 1;
					int color = to_int(data++, 1);
					bmap.draw_row(color, count, x, y);
					x += count;
				}
				else				// absolute run
				{
					int count = (code >> 2) + 1;
					for (int i = 0; i < count; i++)
					{
						bmap.draw_row(to_int(data++, 1), 1, x, y);
						++x;
					}
				}
			}
			x = 0;
			++y;
		}
	}
	else
	{
		logger.error("\tinvalid RLE color compression " + to_string(clrcmp));
	}
}

void decode_uncompressed_img(char* data, int datlen, RipUtil::BitmapData& bmap, int x, int y,
	int width, int height, bool horiz, bool trans, int localtransind, int transind)
{
	int remaining = width * height;
	RipUtil::DrawPos pos = { 0, 0 };
	while (remaining > 0)
	{
		int color = to_int(data++, 1);
		draw_and_update_pos(bmap, pos, color, 1, x, y, width, height, true, false);
		--remaining;
	}
}

void decode_lined_rle(const char* data, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind, bool trans,
	ColorMap colormap, bool deindex)
{
	int currx = 0;
	int curry = 0;

	int pos = 0;
	int next_pos = pos;
	while (pos < datlen && curry < height)
	{
		int bytecount = to_int(data + next_pos, 2, DatManip::le);
		pos = next_pos + 2;
		next_pos += bytecount + 2;
		while (pos < datlen && pos < next_pos)
		{
			int code = to_int(data + pos, 1);
			++pos;

			if (code & 1)		// skip count
			{
				currx += (code >> 1);
			}
			else if (code & 2)	// encoded run
			{
				int count = (code >> 2) + 1;
				int color = to_int(data + pos, 1);
				++pos;
				if (deindex)
					color = colormap[color];
				if (trans && color == localtransind)
					color = transind;
				bmap.draw_row(color, count, currx, curry, x, y, width, height);
				currx += count;
			}
			else				// absolute run
			{
				int count = (code >> 2) + 1;
				for (int i = 0; i < count; i++)
				{
					int color = to_int(data + pos, 1);
					if (deindex)
						color = colormap[color];
					if (trans && color == localtransind)
						color = transind;
					bmap.draw_row(color, 1, currx, curry, x, y, width, height);
					++pos;
					++currx;
				}
			}
		}
		currx = 0;
		++curry;
	}
}

void decode_lined_rle(const char* data, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind, bool trans)
{
	decode_lined_rle(data, datlen, bmap, x, y, width, height, 
		localtransind, transind, trans, dummy_colormap, false);
}

void decode_type2_lined_rle(const char* data, int datlen, RipUtil::BitmapData& bmap,
	int x, int y, int width, int height, int localtransind, int transind)
{
	int currx = 0;
	int curry = 0;

	int pos = 0;
	int next_pos = pos;
	while (pos < datlen && curry < height)
	{
		int bytecount = to_int(data + next_pos, 2, DatManip::le);
		pos = next_pos + 2;
		next_pos += bytecount + 2;
		while (pos < datlen && pos < next_pos)
		{
			int code = to_int(data + pos, 1);
			++pos;

			if (code & 1)		// carry over from previous image
			{					// we assume the data has already been copied and simply skip it
				currx += (code >> 1);
			}
			else if (code & 2)	// encoded run
			{
				int count = (code >> 2) + 1;
				int color = to_int(data + pos, 1);
				++pos;
				bmap.draw_row(color, count, currx, curry, x, y, width, height);
				currx += count;
			}
			else				// skip count
			{					// we draw the transparent color on top of whatever's already there
				int count = (code >> 2) + 1;
				bmap.draw_row(transind, count, currx, curry, x, y, width, height);
				currx += count;
			}
		}
		currx = 0;
		++curry;
	}
}

void decode_bitstream_img(char* data, int datlen, RipUtil::BitmapData& bmap, int x, int y,
	int width, int height, int bpabsol, int bprel, bool horiz, bool trans, bool exprange,
	int localtransind, int transind, const ColorMap& colorremap, bool remap)
{
	int remaining = width * height;
	RipUtil::DrawPos pos = { 0, 0 };

	int color = to_int(data, 1);
	int firstdrawcolor = color;
	if (remap)
		firstdrawcolor = colorremap[color];
	if (trans && firstdrawcolor == localtransind)
		firstdrawcolor = transind;
	draw_and_update_pos(bmap, pos, firstdrawcolor, 1, x, y, width, height, horiz, trans);
	--remaining;
	RLBitStream bstr(data + 1, datlen - 1);
	bool shiftisdown = true;
	while (remaining > 0)
	{
		// for each 0, draw 1 pixel of the current color
		while (remaining > 0 && bstr.get_bit() == 0)
		{
			int drawcolor = color;
			if (remap)
				drawcolor = colorremap[color];
			if (trans && drawcolor == localtransind)
				drawcolor = transind;
			draw_and_update_pos(bmap, pos, drawcolor, 1, x, y, width, height, horiz, trans);
			--remaining;
		}
		if (remaining > 0)				// we hit a 1
		{
			if (bstr.get_bit() == 0)	// 01: absolute set
			{
				int newcol = 0;
				for (int i = 0; i < bpabsol; i++)
				{
					int bit = bstr.get_bit();
					newcol |= (bit << i);
				}
				color = newcol;

				int drawcolor = color;
				if (remap)
					drawcolor = colorremap[color];
				if (trans && drawcolor == localtransind)
					drawcolor = transind;

				// draw 1 pixel of the new color
				draw_and_update_pos(bmap, pos, drawcolor, 1, x, y, width, height, horiz, trans);
				--remaining;

				// reset direction of 1-bit draw shift
				if (bprel == 1)
					shiftisdown = true;
			}
			else						// 11: relative set
			{
				int shift = 0;
				int length = 1;
				for (int i = 0; i < bprel; i++)
				{
					shift |= (bstr.get_bit() << i);
				}

				if (bprel != 1)
				{
					shift -= (1 << (bprel - 1));

					if (exprange)
					{
						if (shift >= 0)
							shift += 1;
					}
					else if (!exprange)	// 0 shift = run length
					{
						if (shift == 0)	// get 8-bit count
						{
							int newlen = 0;
							for (int i = 0; i < 8; i++)
							{
								newlen |= (bstr.get_bit() << i);
							}
							length = newlen;
						}
					}

				}
				else if (bprel == 1)	// 1-bit mode: get/set shift direction
				{
					if (shift == 1)		// toggle direction of 0 shift
					{
						shiftisdown ? shiftisdown = false
							: shiftisdown = true;
					}
					shiftisdown ? shift = -1
						: shift = 1;
				}

				color += shift;

				int drawcolor = color;
				if (remap)
					drawcolor = colorremap[color];
				if (trans && drawcolor == localtransind)
					drawcolor = transind;

				// draw pixel(s) of the new color
				draw_and_update_pos(bmap, pos, drawcolor, length, x, y, width, height, horiz, trans);
				remaining -= length;
			}
		}
	}
}

void decode_bitstream_img(char* data, int datlen, RipUtil::BitmapData& bmap, int x, int y,
	int width, int height, int bpabsol, int bprel, bool horiz, bool trans, bool exprange,
	int localtransind, int transind)
{
	decode_bitstream_img(data, datlen, bmap, x, y, width, height, bpabsol, bprel,
		horiz, trans, exprange, localtransind, transind, dummy_colormap, false);
}

void draw_and_update_pos(RipUtil::BitmapData& bmap, DrawPos& pos, int color, int count,
	int xoff, int yoff, int width, int height, bool horiz, bool trans)
{
	if (horiz)
		pos = bmap.draw_row_wrap(color, count, pos.x, pos.y, xoff, yoff, width, height);
	else
		pos = bmap.draw_col_wrap(color, count, pos.x, pos.y, xoff, yoff, width, height);
}


};	// end of namespace Humongous
