#include "humongous.h"
#include "humongous_read.h"
#include "humongous_rip.h"
#include "humongous_structs.h"

#include "../utils/MembufStream.h"
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
#include <ctime>

using namespace RipUtil;
using namespace RipperFormats;
using namespace ErrLog;
using namespace Logger;

namespace Humongous
{


bool HERip::can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat)
{
	if (stream.get_fsize() < 16)
		return false;

	check_params(ripset);

	int initial_encoding = stream.get_decoding_byte();
	bool detected = false;
	FormatSubtype fmtguess = fmt_unknown;

	if (ripset.encoding != 0)
		stream.set_decoding_byte(ripset.encoding);
	else
		stream.set_decoding_byte(0x69);
	while (detected == false)
	{
		detected = true;

		stream.seekg(0);
		SputmChunkHead chunkhd1;
		read_sputm_chunkhead(stream, chunkhd1);

		// fail if first chunk ID is invalid
		if (chunkhd1.type == lecf)
			fmtguess = lecf_type2;
		else if (chunkhd1.type == tlkb)
			fmtguess = tlkb_type1;
		else if (chunkhd1.type == song)
			fmtguess = song_type1;
		else if (chunkhd1.type == mraw)
			fmtguess = song_dmu;
		else
			detected = false;

		
		// fail if first chunksize != filesize
		if (detected != false && chunkhd1.size != stream.get_fsize())
			detected = false;

		// check for subformat; fail if next chunk type does not match expected
		if (detected != false)
		{
			SputmChunkHead chunkhd2;
			read_sputm_chunkhead(stream, chunkhd2);
			if (chunkhd1.type == lecf)
			{
				if (chunkhd2.type == lflf)
					fmtguess = lecf_type2;
				else if (chunkhd2.type == loff)
					fmtguess = lecf_type1;
				else
					detected = false;
			}
			else if (chunkhd1.type == tlkb)
			{
				if (chunkhd2.type == talk || chunkhd2.type == wsou)
					fmtguess = tlkb_type1;
				else
					detected = false;
			}
			else if (chunkhd1.type == song && chunkhd2.type == sghd)
			{
				// special case: file is valid, but has no tracks (pajama3 demo)
				if (chunkhd2.nextaddr() >= stream.get_fsize())
				{
					/* error message here? */
					return false;
				}

				// check additional chunks to determine format subtype
				stream.seekg(chunkhd2.address + chunkhd2.size);
				SputmChunkHead chunkhd3;
				read_sputm_chunkhead(stream, chunkhd3);

				// next chunk unheadered: type 1 or type 2
				if (chunkhd3.type == chunk_unknown)
					fmtguess = song_type1;
				// next chunk is SGEN: type 3 or type 4
				else if (chunkhd3.type == sgen)
					fmtguess = song_type3;

				// type2 and type4 detection would go here, but the ripping
				// functions as written don't actually need to know
			}
			else if (chunkhd1.type == mraw && chunkhd2.type == hshd)
			{
				fmtguess = song_dmu;
			}
			else
				detected = false;
		}

		// pass was successful: we can rip
		if (detected == true)
		{
			formatsubtype = fmtguess;
			encoding = stream.get_decoding_byte();
			stream.set_decoding_byte(initial_encoding);
			return true;
		}
		// pass was unsuccessful
		else
		{
			// if we haven't tried some encodings, use them
			if (ripset.encoding != 0 && stream.get_decoding_byte() == ripset.encoding)
			{
				// first encoding was user-specified
				if (ripset.encoding != 0)
					stream.set_decoding_byte(0x69);
				// first encoding was user-specified 0x69
				else
					stream.set_decoding_byte(0);
			}
			// if 0x69 didn't work, try 0
			else if (stream.get_decoding_byte() == 0x69)
			{
				stream.set_decoding_byte(0);
			}
			// give up
			else if (stream.get_decoding_byte() == 0)
			{
				stream.set_decoding_byte(initial_encoding);
				return false;
			}
		}
	}
	// should never reach this
	stream.set_decoding_byte(initial_encoding);
	return false;
}

RipperFormats::RipResults HERip::rip(RipUtil::MembufStream& stream, const std::string& fprefix,
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat)
{
	disablelog ? logger.disable() :
		logger.open(fprefix + "-log.txt");

	logger.qprint("initiating rip of " + stream.get_fname()
		+ ", size " + to_string(stream.get_fsize()) + " bytes"
		+ ", encoding " + to_string(encoding));
	logger.qprint("output prefix: " + fprefix);

	RipResults results;

	if (encoding != -1)
		stream.set_decoding_byte(encoding);

	stream.seekg(0);

	if (decode_only)
	{
		logger.qprint("decoding only");

		std::ofstream ofs((fprefix + "-decoded").c_str(),
		    std::ios_base::binary);
		char* outbytes = new char[stream.get_fsize()];
		stream.read(outbytes, stream.get_fsize());
		ofs.write(outbytes, stream.get_fsize());
		delete[] outbytes;
		return results;
	}

	// HE1/(A) data file
	if (formatsubtype == lecf_type1 || formatsubtype == lecf_type2)
	{
		rip_lecf(stream, fprefix, ripset, fmtdat, results);
	}
	// HE2 dialog file
	else if (formatsubtype == tlkb_type1)
	{
		rip_tlkb(stream, fprefix, ripset, fmtdat, results);
	}
	// HE4, type 1 or 2
	else if (formatsubtype == song_type1 || formatsubtype == song_type2)
	{
		rip_song_type12(stream, fprefix, ripset, fmtdat, results);
	}
	// HE4, type 3 or 4
	else if (formatsubtype == song_type3 || formatsubtype == song_type4)
	{
		rip_song_type34(stream, fprefix, ripset, fmtdat, results);
	}
	// DMU music file
	else if (formatsubtype == song_dmu)
	{
		rip_song_dmu(stream, fprefix, ripset, fmtdat, results);
	}

	if (logger.get_errflag())
	{
		std::cout << "One or more errors occured; please check the log file" << '\n';
		logger.qprint("Reported number of errors: " + to_string(logger.get_errcount()));
	}
	if (logger.get_warnflag())
	{
		std::cout << "One or more warnings were issued; "
		"please check the log file" << '\n';
		logger.qprint("Reported number of warnings: "
			+ to_string(logger.get_warncount()));
	}

	return results;
}

void HERip::check_params(const RipperFormats::RipperSettings& ripset)
{
	if (!ripset.ripgraphics)
	{
		rmimrip = false;
		obimrip = false;
		awizrip = false;
		charrip = false;
	}

	if (!ripset.ripanimations)
	{
		akosrip = false;
	}

	if (!ripset.ripaudio)
	{
		digirip = false;
		talkrip = false;
		wsourip = false;
	}

	if (ripset.startentry != RipConsts::not_set)
	{
		roomstart = ripset.startentry;
	}

	if (ripset.endentry != RipConsts::not_set)
	{
		roomend = ripset.endentry;
	}

	for (int i = 2; i < ripset.argc; i++)
	{
		if (quickstrcmp(ripset.argv[i], "--decodeonly"))
			decode_only = true;
		else if (quickstrcmp(ripset.argv[i], "--normim"))
			rmimrip = false;
		else if (quickstrcmp(ripset.argv[i], "--noobim"))
			obimrip = false;
		else if (quickstrcmp(ripset.argv[i], "--noakos"))
			akosrip = false;
		else if (quickstrcmp(ripset.argv[i], "--noawiz"))
			awizrip = false;
		else if (quickstrcmp(ripset.argv[i], "--nochar"))
			charrip = false;
		else if (quickstrcmp(ripset.argv[i], "--nosound"))
		{
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--notlke"))
			tlkerip = false;
		else if (quickstrcmp(ripset.argv[i], "--noextdmu"))
			extdmurip = false;
		else if (quickstrcmp(ripset.argv[i], "--nometadata"))
			metadatarip = false;
		else if (quickstrcmp(ripset.argv[i], "--rmimonly"))
		{
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--obimonly"))
		{
			rmimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--akosonly"))
		{
			rmimrip = false;
			obimrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--awizonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--charonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--soundonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--extdmuonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--tlkeonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--metadataonly"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			extdmurip = false;
			tlkerip = false;
			wsourip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--norip"))
		{
			rmimrip = false;
			obimrip = false;
			akosrip = false;
			awizrip = false;
			charrip = false;
			digirip = false;
			talkrip = false;
			wsourip = false;
			extdmurip = false;
			tlkerip = false;
			metadatarip = false;
		}
		else if (quickstrcmp(ripset.argv[i], "--disablelog"))
		{
			disablelog = true;
		}
		else if (quickstrcmp(ripset.argv[i], "--force_lined_rle"))
		{
			rle_encoding_method_hack = rle_hack_always_use_lined;
			rle_encoding_method_hack_was_user_overriden = true;
		}
		else if (quickstrcmp(ripset.argv[i], "--force_unlined_rle"))
		{
			rle_encoding_method_hack = rle_hack_always_use_unlined;
			rle_encoding_method_hack_was_user_overriden = true;
		}
		else if (quickstrcmp(ripset.argv[i], "--force_akos2c_rle"))
		{
			akos_2color_decoding_hack = akos_2color_hack_always_use_rle;
			akos_2color_decoding_hack_was_user_overriden = true;
		}
		else if (quickstrcmp(ripset.argv[i], "--force_akos2c_bitmap"))
		{
			akos_2color_decoding_hack = akos_2color_hack_always_use_bitmap;
			akos_2color_decoding_hack_was_user_overriden = true;
		}
		if (i < ripset.argc - 1)	// parameterized settings
		{
			if (quickstrcmp(ripset.argv[i], "-alttrans"))
			{
				alttrans = true;
				transcol = from_string<int>(std::string(ripset.argv[i + 1]));
			}
		}
	}
}

void HERip::rip_lecf(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat,
	RipperFormats::RipResults& results)
{
	// clear any files we need to write to
	if (metadatarip)
	{
		std::ofstream((fprefix + "-metadata.txt").c_str(),
		    std::ios_base::trunc);
	}

	SputmChunkHead lecf_hd;
	SputmChunk loffc;
	read_sputm_chunkhead(stream, lecf_hd);
	read_chunk_if_exists(stream, loffc, loff);

	int rmnum = 0;
	while (stream.tellg() < lecf_hd.nextaddr())
	{
		if (roomend != not_set && rmnum > roomend)
		{
			logger.print("ending read at room " + to_string(rmnum));
			break;
		}
		else if (roomstart != not_set && rmnum < roomstart)
		{
			logger.print("skipping room " + to_string(rmnum));
			SputmChunkHead skiphd;
			read_sputm_chunkhead(stream, skiphd);
			stream.seekg(skiphd.nextaddr());
			++rmnum;
			continue;
		}

		int starttime = std::clock();

		std::string rmstr = "-room-" + to_string(rmnum);

		logger.print("reading room " + to_string(rmnum) + "...");

		LFLFChunk lflfc;
		read_lflf(stream, lflfc);

		logger.print("...finished read");
			
		starttime = std::clock() - starttime;
		logger.qprint("\tread time: " + to_string((double)starttime/CLOCKS_PER_SEC)
			+ " s");
		logger.qprint("\tread stats:");
		if (lflfc.rmim_chunk.images.size())
			logger.qprint("\t\tRMIM: " + to_string(lflfc.rmim_chunk.images.size()));
		if (lflfc.obim_chunks.size())
			logger.qprint("\t\tOBIM: " + to_string(lflfc.obim_chunks.size()));
		if (lflfc.akos_chunks.size())
			logger.qprint("\t\tAKOS: " + to_string(lflfc.akos_chunks.size()));
		if (lflfc.awiz_chunks.size())
			logger.qprint("\t\tAWIZ: " + to_string(lflfc.awiz_chunks.size()));
		if (lflfc.mult_chunks.size())
			logger.qprint("\t\tMULT: " + to_string(lflfc.mult_chunks.size()));
		if (lflfc.char_chunks.size())
			logger.qprint("\t\tCHAR: " + to_string(lflfc.char_chunks.size()));
		if (lflfc.digi_chunks.size())
			logger.qprint("\t\tDIGI: " + to_string(lflfc.digi_chunks.size()));
		if (lflfc.talk_chunks.size())
			logger.qprint("\t\tTALK: " + to_string(lflfc.talk_chunks.size()));
		if (lflfc.wsou_chunks.size())
			logger.qprint("\t\tWSOU: " + to_string(lflfc.wsou_chunks.size()));

		if (!stream.eof())
		{
			stream.seekg(lflfc.nextaddr());
		}
		else if (stream.eof() && lflfc.nextaddr() < lecf_hd.nextaddr())
		{
			logger.error("stream unexpectedly reached end of file -- "
				"probably hit a misaligned chunk, recovering to next room");
			stream.clear();
			stream.seekg(lflfc.nextaddr());
		}

		// set alternate transparency color, if requested
		if (!alttrans)
			transcol = lflfc.trns_chunk.trns_val;

		if (rmimrip && lflfc.rmim_chunk.type == rmim)
		{
			logger.print("\tripping RMIM");
			rip_rmim(lflfc, ripset, fprefix + rmstr, results, transcol);
		}

		if (obimrip && lflfc.obim_chunks.size())
		{
			logger.print("\tripping OBIM");
			rip_obim(lflfc, ripset, fprefix + rmstr, results, transcol);
		}

		if (akosrip && lflfc.akos_chunks.size())
		{
			logger.print("\tripping AKOS");
			rip_akos(lflfc, ripset, fprefix + rmstr, results, transcol);
		}

		if (awizrip && lflfc.awiz_chunks.size())
		{
			logger.print("\tripping AWIZ");
			rip_awiz(lflfc, ripset, fprefix + rmstr, results, transcol);
		}

		if (charrip && lflfc.char_chunks.size())
		{
			logger.print("\tripping CHAR");
			rip_char(lflfc, ripset, fprefix + rmstr, results, transcol);
		}

		if (digirip && lflfc.digi_chunks.size())
		{
			logger.print("\tripping DIGI");
			rip_sound(lflfc.digi_chunks, ripset, fprefix + rmstr + "-digi-",
				results);
		}

		if (talkrip && lflfc.talk_chunks.size())
		{
			logger.print("\tripping TALK");
			rip_sound(lflfc.talk_chunks, ripset, fprefix + rmstr + "-talk-",
				results);
		}

		if (wsourip && lflfc.wsou_chunks.size())
		{
			logger.print("\tripping WSOU");
			rip_wsou(lflfc, ripset, fprefix + rmstr, results,
				// override decoding settings if normaliziation requested
				ripset.normalize ? true : ripset.decode_audio);
		}

		if (extdmurip && lflfc.fmus_chunks.size())
		{
			rip_extdmu(lflfc, fprefix, ripset, fmtdat, results);
		}

		if (tlkerip && lflfc.tlke_chunks.size())
		{
			logger.print("\tripping TLKE");
			rip_tlke(lflfc, ripset, fprefix + "-tlke.txt",
				rmnum, results);
		}

		if (metadatarip)
		{
			logger.print("\tripping metadata");
			rip_metadata(lflfc, ripset, fprefix + "-metadata.txt", 
				rmnum, results);
		}

	++rmnum;

	}
}

void HERip::rip_tlkb(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat,
	RipperFormats::RipResults& results)
{
	logger.print("\tripping TLKB");

	SputmChunkHead tlkbhd;
	read_sputm_chunkhead(stream, tlkbhd);
	
	int sndnum = 0;
	SputmChunkHead hdcheck;
	while (stream.tellg() < tlkbhd.nextaddr())
	{
		bool seektonext = true;

		read_sputm_chunkhead(stream, hdcheck);
		stream.seekg(hdcheck.address);

		switch (hdcheck.type)
		{
		case talk:
		{
			SoundChunk soundc;
			read_digi_talk(stream, soundc);
			if (ripset.normalize)
				soundc.wave.normalize();

			write_pcmdata_wave(soundc.wave, fprefix
				+ "-tlkb-talk-" + to_string(sndnum)
				+ ".wav",
				ripset.ignorebytes,
				ripset.ignoreend);

			++results.audio_ripped;
			break;
		}
		case wsou:
		{
			WSOUChunk wsouc;
			read_wsou(stream, wsouc);
			RIFFEntry& riffe = wsouc.riff_entry;

			if (ripset.decode_audio)
			{
				PCMData wave;
				CommFor::riff::decode_riff(riffe.riffdat, riffe.riffdat_size,
					wave);

				if (ripset.normalize)
					wave.normalize();

				write_pcmdata_wave(wave, fprefix
					+ "-tlkb-wsou-" + to_string(sndnum)
					+ ".wav",
					ripset.ignorebytes,
					ripset.ignoreend);

				++results.audio_ripped;
			}
			else
			{
				std::ofstream ofs((fprefix
					+ "-tlkb-wsou-" + to_string(sndnum)
					+ ".wav").c_str(),
					    std::ios_base::binary);
				ofs.write(riffe.riffdat, riffe.riffdat_size);

				++results.audio_ripped;
			}
			break;
		}
		default:
			logger.error("unrecognized TLKB subcontainer "
				+ hdcheck.name + " (type " + to_string(hdcheck.type)
				+ ") at " + to_string(hdcheck.address) + '\n'
				+ "searching for next chunk");

			// crappy error recovery code
			// this isn't watertight and hopefully will only ever run
			// on the pajama sam demo
			while (!stream.eof())
			{
				int startaddress = hdcheck.address;

				stream.seekg(stream.seek_bytes(id_TALK, 4));
				if (!stream.eof())
				{
					read_sputm_chunkhead(stream, hdcheck);

					// check if this is actually a valid chunk
					if (hdcheck.nextaddr() > stream.get_fsize() + 1
						|| hdcheck.size <= 0)
					{
						// not valid: keep trying from new location,
						// potentially skipping WSOU chunks
						continue;
					}
					else
					{
						logger.print("found new TALK chunk, continuing");
						seektonext = false;
						stream.seekg(hdcheck.address);
						break;
					}
				}

				// the probably-theoretical case where we need to find a WSOU
				stream.clear();
				stream.seekg(startaddress);
				stream.seekg(stream.seek_bytes(id_WSOU, 4));
				if (!stream.eof())
				{
					read_sputm_chunkhead(stream, hdcheck);

					// check if this is actually a valid chunk
					if (hdcheck.nextaddr() > stream.get_fsize() + 1
						|| hdcheck.size <= 0)
					{
						// not valid: keep trying from new location,
						// potentially having skipped TALK chunks
						continue;
					}
					else
					{
						logger.print("found new WSOU chunk, continuing");
						seektonext = false;
						stream.seekg(hdcheck.address);
						break;
					}
				}
			}

			// found new chunk: proceed
			if (hdcheck.type == talk || hdcheck.type == wsou)
				break;

			logger.print("no next chunk found, giving up");
			return;
		}
		
		++sndnum;

		if (seektonext)
			stream.seekg(hdcheck.nextaddr());
	}
}

void HERip::rip_song_type12(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat,
	RipperFormats::RipResults& results)
{
	logger.print("\tripping SONG");

	SputmChunkHead songhd;
	read_sputm_chunkhead(stream, songhd);

	SputmChunkHead sghdhd;
	read_sputm_chunkhead(stream, sghdhd);
	stream.seekg(sghdhd.address);

	SONGHeader song_header;
	read_songhead_type1(stream, song_header);
	stream.seekg(sghdhd.nextaddr());

	rip_song_entries(stream, song_header, fprefix, ripset, results);
}

void HERip::rip_song_type34(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat,
	RipperFormats::RipResults& results)
{
	logger.print("\tripping SONG");

	SputmChunkHead songhd;
	read_sputm_chunkhead(stream, songhd);

	SONGHeader song_header;
	read_songhead_type234(stream, song_header);

	rip_song_entries(stream, song_header, fprefix, ripset, results);
}

void HERip::rip_song_entries(RipUtil::MembufStream& stream, const SONGHeader& song_header,
	const std::string& fprefix, const RipperFormats::RipperSettings& ripset, 
	RipperFormats::RipResults& results)
{
	for (std::vector<SONGEntry>::size_type i = 0;
		i < song_header.song_entries.size(); i++)
	{
		if (roomend != not_set && i > (unsigned int)roomend)
		{
			break;
		}
		else if (roomstart != not_set && i < (unsigned int)roomstart)
		{
			continue;
		}

		const SONGEntry& songe = song_header.song_entries[i];

		stream.seekg(songe.address);

		SputmChunkHead hdcheck;
		read_sputm_chunkhead(stream, hdcheck);
		stream.seekg(hdcheck.address);
		switch (hdcheck.type)
		{
		case digi:
		{
			SoundChunk soundc;
			read_digi_talk(stream, soundc);

			if (ripset.normalize)
				soundc.wave.normalize();

			write_pcmdata_wave(soundc.wave, fprefix
				+ "-song-digi-" + to_string(i)
				+ ".wav",
				ripset.ignorebytes,
				ripset.ignoreend);

			++results.audio_ripped;
			break;
		}
		case riff:
		{
			if (ripset.decode_audio)
			{
				RIFFEntry riffe;
				read_riff(stream, riffe);

				PCMData wave;
				CommFor::riff::decode_riff(riffe.riffdat, riffe.riffdat_size,
					wave);

				if (ripset.normalize)
					wave.normalize();

				write_pcmdata_wave(wave, fprefix
					+ "-song-riff-" + to_string(i)
					+ ".wav",
					ripset.ignorebytes,
					ripset.ignoreend);

				++results.audio_ripped;
			}
			else
			{
				// RIFF uses little-endian, noninclusive chunk sizes
				int sz = set_end(hdcheck.size, 4, DatManip::le) + 8;
				char* data = new char[sz];
				stream.read(data, sz);
				std::ofstream ofs((fprefix
					+ "-song-riff-" + to_string(i)
					+ ".wav").c_str(), std::ios_base::binary);
				ofs.write(data, sz);
				delete[] data;

				++results.audio_ripped;
			}

			break;
		}
		case chunk_unknown:		// assume to be unheadered PCM (SONG type 1)
		{
			PCMData wave(songe.length, 1, 11000, 8);
			wave.set_signed(DatManip::has_nosign);
			stream.read(wave.get_waveform(), songe.length);

			if (ripset.normalize)
				wave.normalize();

			write_pcmdata_wave(wave, fprefix
				+ "-song-unheadered-" + to_string(i)
				+ ".wav",
				ripset.ignorebytes,
				ripset.ignoreend);

			++results.audio_ripped;
			break;
		}
		case chunk_none:
			logger.error("\tcould not read SONG subchunk at " +  to_string(stream.tellg()));
			break;
		default:
			logger.error("\trecognized but invalid SONG subchunk " + hdcheck.name 
				+ " at " + to_string(hdcheck.address)
				+ " (type " + to_string(hdcheck.type) + ")");
			break;
		}
	}
}

void HERip::rip_song_dmu(RipUtil::MembufStream& stream, const std::string& fprefix,
	const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat,
	RipperFormats::RipResults& results)
{
	logger.print("\tripping DMU");

	stream.seekg(0);
	SoundChunk soundc;
	read_digi_talk(stream, soundc);

	// account for DMU signedness
	soundc.wave.set_signed(DatManip::has_sign);
	soundc.wave.convert_signedness(DatManip::has_nosign);

	if (ripset.normalize)
		soundc.wave.normalize();

	write_pcmdata_wave(soundc.wave, fprefix
		+ ".wav",
		ripset.ignorebytes,
		ripset.ignoreend);

	++results.audio_ripped;
}

void HERip::rip_extdmu(const LFLFChunk& lflfc, const std::string& fprefix, 
	const RipperFormats::RipperSettings& ripset, 
	const RipperFormats::FileFormatData& fmtdat, 
	RipperFormats::RipResults& results)
{
	// check if files in FMUS exist and rip them if possible
	for (std::vector<FMUSChunk>::size_type i = 0;
		i < lflfc.fmus_chunks.size(); i++)
	{
		const FMUSChunk& fmusc = lflfc.fmus_chunks[i];
		// strip nonstandard terminators from filename
		std::string checkchars;
		checkchars += 0x20;
		checkchars += 0xD;
		checkchars += 0xA;
		checkchars += 0x1A;
		std::string filename = strip_terminators(fmusc.fmussdat_chunk.filestring,
			checkchars);
		std::string filepath = get_lowest_directory(ripset.argv[1]) + filename;
		logger.print("checking for external sound file " + filename
			+ " at " + filepath);

		if (file_exists(filepath))
		{
			RipUtil::MembufStream dmustream(filepath, RipUtil::MembufStream::rb);
			rip_song_dmu(dmustream, fprefix + "-" + strip_extension(filename), 
				ripset, fmtdat, results);
		}
		else
		{
			logger.print("couldn't find file -- file is missing or source "
				"referenced nonexistent file");
		}
	}
}


};	// end of namespace Humongous
