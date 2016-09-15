#ifdef ENABLE_CANDYADV

#include "RipModule.h"
#include "../RipperFormats.h"
#include "../utils/MembufStream.h"
#include <string>
#include <vector>

namespace CandyAdv
{


class CandyAdvRip : public RipModule
{
public:
	CandyAdvRip()
		: RipModule("Candy Land Adventure", 
		"", 
		true, 
		"candyadv_rip: Candy Land Adventure resource extractor and decompressor\n"
		"Usage: candyadv_rip <infile> <parameters>\n"
		"<infile> should be the GAME.RFS resource file\n"
		"Parameters:\n"
		"-o <outfile>\tSet the output file prefix (default: <infile>)\n"
		"-nl <number>\tSet number of loops for looped sounds (default: 0)\n"
		"--ripallraw\tExtract only raw (compressed) data files\n"
		"--palettesoff\tDon't guess palettes (use greyscale)\n"
		"--noanimations\tDon't rip animations\n"
		"--nographics\tDon't rip static graphics\n"
		"--noaudio\tDon't rip audio\n"
		,
		RipModule::unsupported,
		RipModule::unsupported,
		RipModule::unsupported,
		RipModule::unsupported,
		RipModule::unsupported) { };

	bool can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat);

	RipperFormats::RipResults rip(RipUtil::MembufStream& stream, const std::string& fprefix,
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat);


private:
	
	// offset table entry
	struct CandyAdvOfftabEntry
	{
		CandyAdvOfftabEntry()
			: offset(0), length(0) { };

		int offset;
		int length;
	};

	// graphics table entry
	struct CandyAdvGrptabEntry
	{
		CandyAdvGrptabEntry()
			: compression(0), datalen(0), unknown1(0),
			width(0), height(0), unknown2(0), unknown3(0) { };

		int compression;
		int datalen;
		int unknown1;
		int width;
		int height;
		int unknown2;
		int unknown3;
	};

	// graphics table
	struct CandyAdvGrptab
	{
		CandyAdvGrptab()
			: unknown1(0), unknown2(0), unknown3(0),
			unknown4(0), unknown5(0), unknown6(0),
			datoffset(0), numimages(0), unknown7(0) { };

		int unknown1;
		int unknown2;
		int unknown3;
		int unknown4;
		int unknown5;
		int unknown6;
		int datoffset;
		int numimages;
		int unknown7;
		std::vector<CandyAdvGrptabEntry> entries;
	};

	// starting from chunk beginning, read an offset table
	void cndadv_read_offtab(RipUtil::MembufStream& stream, 
		std::vector<CandyAdvOfftabEntry>& entries);

	// read a graphics header table
	void cndadv_read_grptab(RipUtil::MembufStream& stream,
		CandyAdvGrptab& grptab);

	// decode an image
	void cndadv_decode_image(RipUtil::MembufStream& stream,
		const CandyAdvGrptabEntry& entry, RipUtil::BitmapData& bmap);

	// read and arrange block-based bitmap data
	void cndadv_read_blockdata(RipUtil::MembufStream& stream,
		RipUtil::BitmapData& bmap, int width, int height, int blockw, int blockh);

	// starting at chunk for room subchunks, read all palettes
	void cndadv_read_palettes(RipUtil::MembufStream& stream,
		const CandyAdvOfftabEntry& entry, std::vector<RipUtil::BitmapPalette>& palettes);

};

	const static char cndadv_chunkid[4]
		= { 0x43, 0x65, (unsigned)0x87, 0x09 };


};	// end namespace CandyAdv

#endif


#pragma once