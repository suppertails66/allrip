#ifdef ENABLE_MOHAWK

#include "mohawk.h"
#include "../RipperFormats.h"
#include "../utils/MembufStream.h"
#include "../utils/PCMData.h"
#include "../utils/BitmapData.h"
#include "../utils/IMAADPCMDecoder.h"
#include "../utils/DefaultException.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace RipUtil;
using namespace RipperFormats;

namespace Mohawk
{


bool MohawkRip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
	RipperFormats::FileFormatData& fmtdat)
{
	if (ripset.encoding != RipConsts::not_set)
		stream.set_decoding_byte(ripset.encoding);
	if (stream.get_fsize() <= 4)
		return false;
	stream.seekg(0);
	char hdcheck[4];
	while (!stream.eof())
	{
		// check for valid 4-byte file identifier
		stream.read(hdcheck, 4);
		// Mohawk header match: MHWK
		if (quickcmp(hdcheck, mhwk_id, 4))
		{
			// secondary check: 4-byte main chunk ID: RSRC
			stream.seekg(8);
			stream.read(hdcheck, 4);
			if (quickcmp(hdcheck, mhwk_rsrc_id, 4))
			{
				fmtdat.format = RipperFormats::Mohawk;
				fmtdat.encoding = stream.get_decoding_byte();
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}


		// if no valid header found, check alternate encodings
		if (fmtdat.format == RipperFormats::format_unknown)
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

void MohawkRip::check_params(int argc, char* argv[])
{
	for (int i = 0; i < argc; i++)
	{
		if (std::strcmp(argv[i], "--noseq") == 0)
		{
			ripseq = false;
		}
	}
}

RipperFormats::RipResults MohawkRip::rip(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
{
	check_params(ripset.argc, ripset.argv);

	RipperFormats::RipResults results;

//	int files_output = 0;
	bool has_strings = false;
	std::vector<MHWKHeadChunk> headers;
	std::vector<MHWKIndexTableEntry> identries;
	std::vector<MHWKAddrTableEntry> entries;
	// get address of first header chunk
	stream.seekg(20);
	int hdaddress = stream.read_int(4);
	stream.seekg(hdaddress);
	int hdlen = stream.read_int(2) + 2;
	int numhdchunks = stream.read_int(2);
	// read header chunks
	char hdcheck[4];
	for (int i = 0; i < numhdchunks; i++)
	{
		stream.read(hdcheck, 4);
		if (quickcmp(hdcheck, mhwk_cinf_id, 4))
			mhwk_read_header(stream, cinf, headers);
		else if (quickcmp(hdcheck, mhwk_conv_id, 4))
			mhwk_read_header(stream, conv, headers);
		else if (quickcmp(hdcheck, mhwk_hots_id, 4))
			mhwk_read_header(stream, hots, headers);
		else if (quickcmp(hdcheck, mhwk_invo_id, 4))
			mhwk_read_header(stream, invo, headers);
		else if (quickcmp(hdcheck, mhwk_qars_id, 4))
			mhwk_read_header(stream, qars, headers);
		else if (quickcmp(hdcheck, mhwk_regs_id, 4))
			mhwk_read_header(stream, regs, headers);
		else if (quickcmp(hdcheck, mhwk_tbmh_id, 4))
			mhwk_read_header(stream, tbmh, headers);
		else if (quickcmp(hdcheck, mhwk_tbmp_id, 4))
			mhwk_read_header(stream, tbmp, headers);
		else if (quickcmp(hdcheck, mhwk_tcnt_id, 4))
			mhwk_read_header(stream, tcnt, headers);
		else if (quickcmp(hdcheck, mhwk_tpal_id, 4))
			mhwk_read_header(stream, tpal, headers);
		else if (quickcmp(hdcheck, mhwk_tscr_id, 4))
			mhwk_read_header(stream, tscr, headers);
		else if (quickcmp(hdcheck, mhwk_stri_id, 4))
		{
			mhwk_read_header(stream, stri, headers);
			// this is used to check whether we need to open a file for strings
			has_strings = true;
		}
		else if (quickcmp(hdcheck, mhwk_strl_id, 4))
			mhwk_read_header(stream, strl, headers);
		else if (quickcmp(hdcheck, mhwk_scen_id, 4))
			mhwk_read_header(stream, scen, headers);
		else if (quickcmp(hdcheck, mhwk_twav_id, 4))
			mhwk_read_header(stream, twav, headers);
		// unrecognized header
		else
		{
			mhwk_read_header(stream, mhwk_dattype_none, headers);
//			throw(DefaultException("unrecognized header chunk"));
		}
	}
	// read index table
	for (std::vector<MHWKHeadChunk>::size_type i = 0; i < headers.size(); i++)
	{
		stream.seekg(hdaddress + headers[i].indstart);
		mhwk_read_type1_chunk(stream, headers[i].dattype, identries);
	}

	// read resource table
	stream.seekg(hdaddress + hdlen);
	mhwk_read_type2_table(stream, mhwk_dattype_none, entries);

	// sort the entries by index number so we can determine
	// palettes positionally
	if (ripset.guesspalettes)
		std::sort(identries.begin(), identries.end(), IDsByIndexNum);

	if (ripset.ripallraw)
	{
		for (std::vector<MHWKIndexTableEntry>::size_type i = 0; i < identries.size(); i++)
		{
			int indnum = identries[i].index - 1;
			stream.seekg(entries[indnum].address);
			char* outbytes = new char[entries[indnum].length];
			stream.read(outbytes, entries[indnum].length);
			std::ofstream ofs((fprefix + "-data-" 
				+ mhwk_get_dattype_name(identries[i].dattype)
				+ "-" + to_string(i)).c_str(),
				std::ios_base::binary);
			ofs.write(outbytes, entries[indnum].length);
			delete[] outbytes;
		}
		return results;
	}

	// read REGS chunks for use in ripping graphics
	REGSChunks regs_chunks;
	for (int i = 0; i < identries.size(); i++)
	{
		if (identries[i].dattype == regs)
		{
			REGSEntriesList regs_entries;
			regs_entries.id = identries[i].unknown;
			
			stream.seekg(entries[identries[i].index - 1].address);
			stream.seek_off(2);
			for (int j = 0; j < (entries[identries[i].index - 1].length - 2)/2; j++)
			{
				regs_entries.entries.push_back(to_signed(stream.read_int(2), 16));
			}

			regs_chunks.push_back(regs_entries);
		}
	}

	// rip strings
	if (ripset.ripstrings && has_strings)
	{
		std::ofstream ofs((fprefix + "-strings" + ".txt").c_str(), 
			std::ios_base::binary | std::ios_base::trunc);
		int stringnum = 0;
		for (std::vector<MHWKIndexTableEntry>::size_type i = 0; i < identries.size(); i++)
		{
			if (identries[i].dattype == stri || identries[i].dattype == strl)
			{
				ofs.width(10);
				ofs << std::left << stringnum + 1;
				int len = entries[identries[i].index - 1].length - 1;
				stream.seekg(entries[identries[i].index - 1].address);
				char* outbytes = new char[len];
				stream.read(outbytes, len);
				ofs.write(outbytes, len);
				ofs.put('\n');
				delete[] outbytes;
				++stringnum;
				++(results.strings_ripped);
			}
		}
	}

	std::vector<BitmapPalette> palettes;

	// external palette
	{
		try
		{
			RipUtil::MembufStream palstream("palette", MembufStream::rb);

			int colorstart = palstream.read_int(2);
			int numentries = palstream.read_int(2);
			BitmapPalette newpal;
			// the first and last 10 colors are unused, so we appropriate
			// color 0 for transparency
			for (int j = 0; j < 256; j++)
				newpal[j] = 0xFC00FC;
			for (int j = colorstart; j < numentries + colorstart; j++)
			{
				int color = 0;
				color |= (palstream.read_int(1));
				color |= (palstream.read_int(1) << 8);
				color |= (palstream.read_int(1) << 16);
				palstream.seek_off(1);
				newpal[j] = color;
			}
			palettes.push_back(newpal);
		}
		catch (RipUtil::FileOpenException&)
		{

		}
	}
	

	// find palettes
	for (std::vector<MHWKIndexTableEntry>::size_type i = 0; i < identries.size(); i++)
	{
		if (identries[i].dattype == tpal)
		{
			int chunkstart = entries[identries[i].index - 1].address;
			stream.seekg(chunkstart);
			int colorstart = stream.read_int(2);
			int numentries = stream.read_int(2);
			numentries = 256;
			BitmapPalette newpal;
			// the first and last 10 colors are unused, so we appropriate
			// color 0 for transparency
			for (int j = 0; j < 256; j++)
				newpal[j] = 0xFC00FC;
			for (int j = colorstart; j < numentries + colorstart; j++)
			{
				int color = 0;
				color |= (stream.read_int(1));
				color |= (stream.read_int(1) << 8);
				color |= (stream.read_int(1) << 16);
				stream.seek_off(1);
				newpal[j] = color;
			}
			palettes.push_back(newpal);
		}
	}

	int palettenum = ripset.palettenum == RipConsts::not_set ? 0
		: ripset.palettenum;
	int startentry = ripset.startentry == RipConsts::not_set ? 0
		: ripset.startentry;
	int endentry = ripset.startentry == RipConsts::not_set ? entries.size() - 1
		: ripset.endentry;

	// read data
	for (std::vector<MHWKIndexTableEntry>::size_type i = 0; i < identries.size(); i++)
	{
		if (i >= startentry && i <= endentry)
		{
			if (identries[i].dattype == twav)
			{
				if (ripset.ripaudio)
				{
					stream.seekg(entries[identries[i].index - 1].address);
					PCMData wave;
					mhwk_read_wave(stream, wave, ripset, fmtdat);
					{
						if (ripset.normalize)
							wave.normalize();
						write_pcmdata_wave(wave, fprefix
							+ "-wave-"
							+ to_string(to_string(identries[i].index)) + ".wav");
						++(results.audio_ripped);
					}
				}
			}

			else if (identries[i].dattype == tbmp)
			{
				if (ripset.ripgraphics /*&& ripset.riptbmp*/)
				{
					stream.seekg(entries[identries[i].index - 1].address);
					BitmapData bitmap;
					mhwk_read_tbmp(stream, bitmap, ripset, fmtdat);

					if (ripset.guesspalettes && ripset.palettenum == RipConsts::not_set)
						bitmap.set_palette(palettes[palettenum]);
					else if (ripset.guesspalettes)
						bitmap.set_palette(palettes[ripset.palettenum]);
					else
						bitmap.set_palette_8bit_grayscale();
					write_bitmapdata_bmp(bitmap, fprefix
						+ "-tbmp-" 
						+ to_string(identries[i].index) + ".bmp");
					++(results.graphics_ripped);
				}
			}
			else if (identries[i].dattype == tbmh)
			{
				if (ripset.ripanimations /*&& ripset.riptbmh*/)
				{
					// find REGS chunks associated with this animation
					REGSEntries regs_x;
					REGSEntries regs_y;
					for (int j = 0; j < regs_chunks.size(); j++)
					{
						if (regs_chunks[j].id == identries[i].unknown)
						{
							regs_x = regs_chunks[j].entries;
							// entries are always paired, with the second chunk
							// always directly following the first, so (j + 1)
							// is safe here
							regs_y = regs_chunks[j + 1].entries;
						}
					}

					int framenum = 0;
					int entrynum = identries[i].index - 1;
					int chunkstart = entries[entrynum].address;
					stream.seekg(chunkstart);
					int numentries = stream.read_int(2);

					// what are these??
					int value1 = stream.read_int(1);
					int value2 = stream.read_int(1);
					int value3 = stream.read_int(2);	// numentries + 1?
					int value4 = stream.read_int(1);
					int value5 = stream.read_int(1);

					// secondary compression
					if (value4 & 0x01)
					{
						int decompressedSize = stream.read_int(4) + 10000;
						int compressedSize = stream.read_int(4);
						int dat_size = stream.read_int(2);

						int start = stream.tellg();

//						const int dat_size = 1024;
						char* dat = new char[dat_size];

						// must be initialized to 0 (compressed files expect this)
						for (int j = 0; j < dat_size; j++)
						{
							dat[j] = 0;
						}

						// for WHATEVER REASON the first 66 bytes go at the end
						// of the input buffer
						int datpos = 958;
						int bytecount = 0;
						int outputcount = 0;

						std::ofstream ofs("~temporary_shitty_hack", std::ios_base::binary | std::ios_base::trunc);

						while (outputcount < decompressedSize)
						{
							int code = stream.read_int(1);
							++bytecount;
						
							for (int j = 0x01; j < 0x100; j <<= 1)
							{
								if (datpos == 75)
								{
									char n;
									n = 'c';
								}

								// bit is set: literal
								if (code & j)
								{
									char c = stream.get();

									dat[datpos % dat_size] = c;
									++datpos;

									ofs << c;

									++bytecount;
									++outputcount;
								}
								// bit is not set: reference
								else
								{
									int command = stream.read_int(2);
									bytecount += 2;

									int size = ((command & 0xFC00) >> 10) + 3;
									int distance = (command & 0x03FF);

									for (int k = 0; k < size; k++)
									{
										int target = (distance + k);
//										// if we've filled the array, account for shifting start point
//										if (datpos >= dat_size)
//										{
//											target += datpos;
//										}

										dat[datpos % dat_size] = dat[target % dat_size];
										ofs << dat[datpos % dat_size];

										++datpos;
										++outputcount;
									}
								}
							}

/*							int code = stream.read_int(1);
							std::cout << "ADDRESS: " << std::hex << stream.tellg() << '\n';
							std::cout.flush();
							std::cout << "CODE: " << std::hex << code << '\n';
							std::cout.flush();

							for (int j = 0x01; j < 0x100; j <<= 1)
							{
								// bit is set: literal
								if (code & j)
								{
									std::cout << '\t' << "LITERAL: " << std::hex << stream.read_int(1) << '\n';
									std::cout.flush();
								}
								// bit is not set: reference
								else
								{
									int command = stream.read_int(2);
									int size = ((command & 0xFC00) >> 10);
									int distance = command & 0x03FF;
									std::cout << '\t' << "REFERENCE: " << size;
									std::cout.flush();
									std::cout << " " << distance << '\n';
									std::cout.flush();
								}
							} */
						}

						delete dat;

						MembufStream dumbstream("~temporary_shitty_hack", MembufStream::rb);

						int fuck = -8;

						rip_tbmh_from_stream(dumbstream, results,
							ripset,
							fmtdat,
							regs_x, regs_y,
							palettes,
							palettenum,
							framenum, fprefix, identries,
							i,
							entrynum, fuck, numentries,
							value1, value2, value3, value4, value5);
					}
					else
					{
						rip_tbmh_from_stream(stream, results,
							ripset,
							fmtdat,
							regs_x, regs_y,
							palettes,
							palettenum,
							framenum, fprefix, identries,
							i,
							entrynum, chunkstart, numentries,
							value1, value2, value3, value4, value5);
					}


/*					int* addresses = new int[numentries];
					int numskipped = 0;

					{
						for (int j = 0; j < numentries; j++)
						{
							addresses[j] = stream.read_int(4);
						}
					}

					TBMHEntryList tbmh_entries;

					for (int j = 0; j < numentries; j++)
					{
						// hack to fix problem in case 7
						if (numentries != 0x5 && addresses[0] != 0x20AC)
						{
							TBMHEntry entry;

							stream.seekg(chunkstart + addresses[j]);
							mhwk_read_tbmp(stream, entry.image, ripset, fmtdat);

							if (ripset.guesspalettes && ripset.palettenum == RipConsts::not_set)
								entry.image.set_palette(palettes[palettenum]);
							else if (ripset.guesspalettes)
								entry.image.set_palette(palettes[ripset.palettenum]);
							else
								entry.image.set_palette_8bit_grayscale();

							entry.xoffset = -regs_x[framenum];
							entry.yoffset = -regs_y[framenum];

							tbmh_entries.push_back(entry);

							if (!ripseq)
							{
								write_bitmapdata_bmp(entry.image, fprefix
									+ "-tbmh-" 
									+ to_string(identries[i].index) + '-' 
									+ to_string(framenum + 1)  
									+ ".bmp");
							}
						}
						++framenum;
						++(results.animation_frames_ripped);
					}

					if (ripseq)
					{
						SequenceSizingInfo seqsize = compute_sequence_enclosing_dimensions(tbmh_entries);

						for (int j = 0; j < tbmh_entries.size(); j++)
						{
							RipUtil::BitmapData bmp(seqsize.width, seqsize.height, 8, true);

							if (ripset.guesspalettes && ripset.palettenum == RipConsts::not_set)
								bmp.set_palette(palettes[palettenum]);
							else if (ripset.guesspalettes)
								bmp.set_palette(palettes[ripset.palettenum]);
							else
								bmp.set_palette_8bit_grayscale();

							bmp.clear(0);

							bmp.blit_bitmapdata(tbmh_entries[j].image,
								seqsize.centerx + tbmh_entries[j].xoffset,
								seqsize.centery + tbmh_entries[j].yoffset);

							write_bitmapdata_bmp(bmp, fprefix
								+ "-tbmh-" 
								+ to_string(identries[i].index) + '-' 
								+ to_string(j + 1)  
								+ ".bmp");
						}
					}

					++(results.animations_ripped);
					delete[] addresses; */
				}
			}

			else
			{
				if (ripset.ripdata)
				{
					int address = entries[identries[i].index - 1].address;
					int len = entries[identries[i].index - 1].length;
					std::string extension;
					switch (identries[i].dattype)
					{
					case cinf: extension = "cinf"; break;
					case conv: extension = "conv"; break;
					case hots: extension = "hots"; break;
					case invo: extension = "invo"; break;
					case qars: extension = "qars"; break;
					case regs: extension = "regs"; break;
					case tbmh: extension = "tbmh"; break;
					case tbmp: extension = "tbmp"; break;
					case tcnt: extension = "tcnt"; break;
					case tpal: extension = "tpal"; break;
					case tscr: extension = "tscr"; break;
					case stri: extension = "stri"; break;
					case scen: extension = "scen"; break;
					case twav: extension = "twav"; break;
					case strl: extension = "strl"; break;
					}
					std::ofstream ofs((fprefix + '_' + extension + '-' + to_string(identries[i].index)).c_str(),
						std::ios_base::binary);
					char* outbytes = new char[len];
					stream.seekg(address);
					stream.read(outbytes, len);
					ofs.write(outbytes, len);
					delete[] outbytes;
					++(results.data_ripped);
				}
			}
		}	// end entry limiter
		
		// switch to a new palette whenever we reach one
		if (identries[i].dattype == tpal && ripset.guesspalettes)
		{
			palettenum += 1;
		}

	}

	return results;
}

void MohawkRip::rip_tbmh_from_stream(RipUtil::MembufStream& stream, RipperFormats::RipResults& results,
		const RipperFormats::RipperSettings& ripset,
		const RipperFormats::FileFormatData& fmtdat,
		REGSEntries& regs_x, REGSEntries& regs_y,
		std::vector<RipUtil::BitmapPalette>& palettes,
		int& palettenum,
		int& framenum, const std::string& fprefix, std::vector<MHWKIndexTableEntry>& identries,
		int i,
		int& entrynum, int& chunkstart, int& numentries,
		int& value1, int& value2, int& value3, int& value4, int& value5)
{
//	if (identries[i].index != 50)
//	{
//		return;
//	}

	std::vector<int> addresses;

	int numskipped = 0;

	{
		for (int j = 0; j < numentries; j++)
		{
			addresses.push_back(stream.read_int(4));
		}
	}

	TBMHEntryList tbmh_entries;

	for (int j = 0; j < numentries; j++)
	{
		// why do i even bother commenting this anymore, it's unreadable anyway
		if (addresses[j] < 0)
		{
			continue;
		}

		// hack to fix problem in case 7
		if (numentries != 0x5 && addresses[0] != 0x20AC)
		{
			TBMHEntry entry;

			stream.seekg(chunkstart + addresses[j]);
			mhwk_read_tbmp(stream, entry.image, ripset, fmtdat);

			if (ripset.guesspalettes && ripset.palettenum == RipConsts::not_set)
				entry.image.set_palette(palettes[palettenum]);
			else if (ripset.guesspalettes)
				entry.image.set_palette(palettes[ripset.palettenum]);
			else
				entry.image.set_palette_8bit_grayscale();

			if (framenum < regs_x.size())
			{
				entry.xoffset = -regs_x[framenum];
			}

			if (framenum < regs_y.size())
			{
				entry.yoffset = -regs_y[framenum];
			}

			tbmh_entries.push_back(entry);

			if (!ripseq)
			{
				write_bitmapdata_bmp(entry.image, fprefix
					+ "-tbmh-" 
					+ to_string(identries[i].index) + '-' 
					+ to_string(framenum + 1)  
					+ ".bmp");
			}
		}
		++framenum;
		++(results.animation_frames_ripped);
	}

	if (ripseq)
	{
		SequenceSizingInfo seqsize = compute_sequence_enclosing_dimensions(tbmh_entries);

		for (int j = 0; j < tbmh_entries.size(); j++)
		{
			RipUtil::BitmapData bmp(seqsize.width, seqsize.height, 8, true);

			if (ripset.guesspalettes && ripset.palettenum == RipConsts::not_set)
				bmp.set_palette(palettes[palettenum]);
			else if (ripset.guesspalettes)
				bmp.set_palette(palettes[ripset.palettenum]);
			else
				bmp.set_palette_8bit_grayscale();

			bmp.clear(0);

			bmp.blit_bitmapdata(tbmh_entries[j].image,
				seqsize.centerx + tbmh_entries[j].xoffset,
				seqsize.centery + tbmh_entries[j].yoffset);

			write_bitmapdata_bmp(bmp, fprefix
				+ "-tbmh-" 
				+ to_string(identries[i].index) + '-' 
				+ to_string(j + 1)  
				+ ".bmp");
		}
	}

	++(results.animations_ripped);
}

bool MohawkRip::IDsByIndexNum(const MHWKIndexTableEntry& first, const MHWKIndexTableEntry& second)
{
	return first.index < second.index;
}

bool MohawkRip::IDsByDatType(const MHWKIndexTableEntry& first, const MHWKIndexTableEntry& second)
{
	return first.dattype < second.dattype;
}

void MohawkRip::mhwk_read_header(MembufStream& stream, MHWKDatType dat,
	std::vector<MHWKHeadChunk>& entries)
{
	int first = stream.read_int(2);
	int second = stream.read_int(2);
	entries.push_back(MHWKHeadChunk(dat, first, second));
}

void MohawkRip::mhwk_skip_type1_table(MembufStream& stream)
{
	int chunklen = stream.read_int(2);
	while (chunklen != 0)
	{
		stream.seek_off(chunklen * 4);
		chunklen = stream.read_int(4);
	}
}

void MohawkRip::mhwk_read_type1_table(MembufStream& stream,
	std::vector<MHWKIndexTableEntry>& entries)
{
	int chunklen = stream.read_int(2);
	while (chunklen != 0)
	{
		for (int i = 0; i < chunklen; i++)
		{
			int unknown = stream.read_int(2);
			int index = stream.read_int(2);
			entries.push_back(MHWKIndexTableEntry(mhwk_dattype_none,
				unknown, index));
		}
		chunklen = stream.read_int(4);
	}
}

void MohawkRip::mhwk_read_type1_chunk(MembufStream& stream, MHWKDatType dat,
	std::vector<MHWKIndexTableEntry>& entries)
{
	int chunklen = stream.read_int(2);
	for (int i = 0; i < chunklen; i++)
	{
		int unknown = stream.read_int(2);
		int index = stream.read_int(2);
		entries.push_back(MHWKIndexTableEntry(dat, unknown, index));
	}
}

void MohawkRip::mhwk_skip_type2_table(MembufStream& stream)
{
	stream.seek_off(stream.read_int(2) * 10);
}

void MohawkRip::mhwk_read_type2_table(MembufStream& stream, MHWKDatType dat,
	 std::vector<MHWKAddrTableEntry>& entries)
{
	int numentries = stream.read_int(2);
	for (int i = 0; i < numentries; i++)
	{
		int addr = stream.read_int(4);
		int len = stream.read_int(2);
		int unk1 = stream.read_int(2);
		int unk2 = stream.read_int(2);
		// read address, length, and unknown values
		entries.push_back(MHWKAddrTableEntry(dat, addr, len, unk1, unk2));
	}
}

std::string MohawkRip::mhwk_get_dattype_name(MHWKDatType dattype)
{
	switch (dattype)
	{
	case cinf: return "cinf"; break;
	case conv: return "conv"; break;
	case hots: return "hots"; break;
	case invo: return "invo"; break;
	case qars: return "qars"; break;
	case regs: return "regs"; break;
	case tbmh: return "tbmh"; break;
	case tbmp: return "tbmp"; break;
	case tcnt: return "tcnt"; break;
	case tpal: return "tpal"; break;
	case tscr: return "tscr"; break;
	case stri: return "stri"; break;
	case scen: return "scen"; break;
	case twav: return "twav"; break;
	case strl: return "strl"; break;
	default: return "unknown";
	}
}

void MohawkRip::mhwk_read_wave(MembufStream& stream, PCMData& dat, 
	const RipperFormats::RipperSettings& ripset, RipperFormats::FileFormatData fmtdat, int len)
{
	char hdcheck[4];
	int start = stream.tellg();
	int numsamps;
	int format;
	bool looping = false;
	int loopstart = 0;
	int loopend = 0;
	// check for Cue# chunk and skip if present
	stream.seekg(start + 12);
	stream.read(hdcheck, 4);
	if (quickcmp(hdcheck, mhwk_cuenum_id, 4))
	{
		stream.seek_off(stream.read_int(4));
		stream.read(hdcheck, 4);
	}
	// check for ADPC chunk and skip if present
	if (quickcmp(hdcheck, mhwk_adpc_id, 4))
	{
		stream.seek_off(stream.read_int(4));
		stream.read(hdcheck, 4);
	}
	// in at least one instance (Carmen case 15 dialog),
	// the previous chunk length is 4 bytes too high
	if (!quickcmp(hdcheck, mhwk_wavedata_id, 4))
	{
		stream.seek_off(-8);
		stream.read(hdcheck, 4);
	}
	if (quickcmp(hdcheck, mhwk_wavedata_id, 4))
	{
		// read Data chunk
		if (len == RipConsts::not_set)
			len = stream.read_int(4) - 28;
		else
			stream.seek_off(4);
		dat.set_samprate(stream.read_int(2));
		numsamps = stream.read_int(4);
		dat.set_sampwidth(stream.read_int(1));
		dat.set_channels(stream.read_int(1));
		format = stream.read_int(2);
		if (stream.read_int(2) == 0xFFFF && ripset.numloops > 0)
		{
			looping = true;
			loopstart = stream.read_int(4);
			loopend = stream.read_int(4);
		}
		else
		{
			stream.seek_off(8);
		}
		if (ripset.ignorebytes != 0)
		{
			stream.seek_off(ripset.ignorebytes);
			len -= ripset.ignorebytes;
		}
		dat.resize_wave(len * 4);
		format_PCMData(dat, ripset);
		// rip sample data
		if (format == mhwk_imaadpcm_fmtid)
		{
			// the sample width we read earlier should have been 
			// 16... but sometimes it isn't
			dat.set_sampwidth(16);
			IMAADPCMDecoder decoder;
			char outbytes[2];
			for (int i = 0; i < len; i++)
			{
				int byte = stream.read_int(1);
				for (int j = 0; j < 2; j++)
				{
					char samp;
					if (j == 0)
						samp = byte >> 4;
					else
						samp = byte & 0xF;
					int decoded_samp = decoder.decode_samp(samp);
					to_bytes(decoded_samp, outbytes, 2, DatManip::le);
					std::memcpy(dat.get_waveform() + i * 4 + j * 2, outbytes, 2);
				}
			}
		}
		else
		{
			// assume 8-bit unsigned PCM
			dat.resize_wave(len);
			dat.set_sampwidth(8);
			for (int i = 0; i < len; i++)
			{
				dat.get_waveform()[i] = stream.read_int(1);
			}
		}
		if (looping)
		{
			dat.add_loop(loopstart, loopend, ripset.numloops - 1, ripset.loopstyle,
				ripset.loopfadelen, ripset.loopfadesil);
		}
	}
}

void MohawkRip::mhwk_read_tbmp(MembufStream& stream, BitmapData& dat,
	const RipperFormats::RipperSettings& ripset, RipperFormats::FileFormatData fmtdat, int len)
{
	int width = stream.read_int(2) & 0x3FFF;
	int height = stream.read_int(2) & 0x3FFF;
	int bytesperrow = stream.read_int(2) & 0x3FFE;
	int format = stream.read_int(2);

	// LZ compression
	if (format & 0x0100)
	{
		RawData rawdat;
		decompress_lz(stream, rawdat);

		std::ofstream ofs("~temporary_shitty_hack_2", std::ios_base::binary | std::ios_base::trunc);
		ofs.write(rawdat.data, rawdat.size);

		RipUtil::MembufStream dumbstream("~temporary_shitty_hack_2", MembufStream::rb);
		extract_tbmp(dumbstream, dat,
			width, height, bytesperrow, format);

	}
	// no compression
	else
	{
		extract_tbmp(stream, dat,
			width, height, bytesperrow, format);
	}
}

void MohawkRip::extract_tbmp(RipUtil::MembufStream& stream, RipUtil::BitmapData& dat,
	int width, int height, int bytesperrow, int format)
{	
	int putpos = 0;
	
	// RLE8 encoding
	if (format & 0x10)
	{
		dat.set_palettized(true);
		dat.resize_pixels(width, height, 8);
		dat.clear(0);
		for (int j = 0; j < height; j++)
		{
			int rowbytecount = stream.read_int(2);
			int startpos = stream.tellg();
			int* pixels = dat.get_pixels();
			int remaining = width;
			while (remaining > 0)
			{
				int code = stream.read_int(1);
				int runlen = (code & 0x7F) + 1;
				if (runlen > remaining)
					runlen = remaining;
				if (code & 0x80)
				{
					int val = stream.read_int(1);
					for (int k = 0; k < runlen; k++)
					{
						pixels[putpos + k] = val;
					}
				}
				else
				{
					char* outbytes = new char[runlen];
					stream.read(outbytes, runlen);
					for (int k = 0; k < runlen; k++)
					{
						pixels[putpos + k] = to_int(outbytes + k, 1, DatManip::be);
					}
					delete[] outbytes;
				}
				remaining -= runlen;
				putpos += runlen;
			}
			if (startpos + rowbytecount >= stream.get_fsize())
			{
				// error
				break;
			}
			else
			{
				stream.seekg(startpos + rowbytecount);
			}
		}
	}
	// raw
	else if ((format & 0x10) == 0)
	{
		dat.set_palettized(true);
		dat.resize_pixels(bytesperrow, height, 8);
		int* pixelstart = dat.get_pixels();
		int* putpos = pixelstart;
		for (int i = 0; i < height; i++)
		{
			for (int j = 0; j < bytesperrow; j++)
			{
				*putpos++ = stream.read_int(1);
			}
		}
	}
	// other encodings
	else
	{
		// ...
	}
}

// decompress standard LZ format data pointed to by stream
void MohawkRip::decompress_lz(RipUtil::MembufStream& stream, RawData& outdat)
{
	int decompressedSize = stream.read_int(4) + 10000;
	int compressedSize = stream.read_int(4);
	int dat_size = stream.read_int(2);

	int start = stream.tellg();

	char* dat = new char[dat_size];

	// must be initialized to 0 (compressed files expect this)
	for (int j = 0; j < dat_size; j++)
	{
		dat[j] = 0;
	}

	// for WHATEVER REASON the first 66 bytes go at the end
	// of the input buffer
	int datpos = 958;
	int bytecount = 0;
	int outputcount = 0;

	outdat.resize(decompressedSize);

	while (outputcount < decompressedSize)
	{
		int code = stream.read_int(1);
		++bytecount;
						
		for (int j = 0x01; j < 0x100; j <<= 1)
		{
			// bit is set: literal
			if (code & j)
			{
				char c = stream.get();

				dat[datpos % dat_size] = c;
				++datpos;

				outdat.data[outputcount] = c;

				++bytecount;
				++outputcount;

				if (outputcount >= decompressedSize)
				{
					break;
				}
			}
			// bit is not set: reference
			else
			{
				int command = stream.read_int(2);
				bytecount += 2;

				int size = ((command & 0xFC00) >> 10) + 3;
				int distance = (command & 0x03FF);

				for (int k = 0; k < size; k++)
				{
					int target = (distance + k);

					dat[datpos % dat_size] = dat[target % dat_size];
					outdat.data[outputcount] = dat[datpos % dat_size];

					++datpos;
					++outputcount;

					if (outputcount >= decompressedSize)
					{
						break;
					}
				}
			}

			if (outputcount >= decompressedSize)
			{
				break;
			}
		}

		if (outputcount > outdat.size)
		{
			char c;
			c = 'n';
		}

	}

	delete dat;
}


};	// end of namespace Mohawk









#endif
