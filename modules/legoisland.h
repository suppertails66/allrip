#ifdef ENABLE_LEGOISLAND

/* a very, very crappy decoder for some Lego Island formats
   everything here except phonemes (face textures) should work
   incomplete, broken phoneme ripping can be enabled with 
   --enable_partial_phonemes
   models and animations aren't supported at all */

#include "RipModule.h"
#include "../RipperFormats.h"
#include "../utils/MembufStream.h"
#include "../utils/BitmapData.h"
#include "../utils/logger.h"
#include "legoisland_structs.h"

#include <string>

namespace LegoIsland
{


class LegoIslandRip : public RipModule
{
public:
	LegoIslandRip()
		: RipModule("LEGO Island", "Ripper for LEGO Island scripts",
				false, "",
				unsupported, unsupported,
				unsupported, unsupported,
				unsupported),
			logging(true), reporting(false), 
			rip_smackers(true), rip_flcs(true), rip_phonemes(false), 
			rip_bmaps(true), rip_waves(true),
			force_wave_loop(false),
			loopfadelen(5), loopendsilence(1) { };

	bool can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat);

	RipperFormats::RipResults rip(RipUtil::MembufStream& stream, const std::string& fprefix, 
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat);
private:
	void check_params(const RipperFormats::RipperSettings& ripset);

	void rip_typeid3(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid3_smacker(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid3_images(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid3_flcs(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid3_phonemes(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid4_wave(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void rip_typeid10_image(
		RipUtil::MembufStream& stream, int id, std::vector<MxChChunk*>& mxchs, 
		MxObChunk& header, const std::string& fprefix);

	void decode_lego_rle8(
		RipUtil::BitmapData& bmp, const char* source, int length);

	void decode_lego_type2_rle8(
		RipUtil::BitmapData& bmp, const char* source, int length);

	void decode_lego_skipblock(
		RipUtil::BitmapData& bmp, const char* source, int length, 
		int numrows, int yoffset);

	void decode_lego_type2_skipblock(
		RipUtil::BitmapData& bmp, const char* source, int length);

	void read_cstring_stream(RipUtil::MembufStream& stream, std::string& dest);

	void addMxObs(MxObChunk& mxobc, std::map<int, MxObChunk*>& MxObIDMap);

	bool MxChAddressOrder(MxChChunk* mxchc_p1, MxChChunk* mxchc_p2);

	void readRIFFOMNI(RipUtil::MembufStream& stream, RIFFOMNIChunk& dest);

	void readMxHd(RipUtil::MembufStream& stream, MxHdChunk& dest);

	void readMxOf(RipUtil::MembufStream& stream, MxOfChunk& dest);

	void readLISTMxSt(RipUtil::MembufStream& stream, LISTMxStChunk& dest);

	void readMxSt(RipUtil::MembufStream& stream, MxStChunk& dest);

	void readMxOb(RipUtil::MembufStream& stream, MxObChunk& dest);

	void readLISTMxCh(RipUtil::MembufStream& stream, LISTMxChChunk& dest);

	void readLISTMxDa(RipUtil::MembufStream& stream, LISTMxDaChunk& dest);

	void readpad(RipUtil::MembufStream& stream, padChunk& dest);

	void readMxCh(RipUtil::MembufStream& stream, MxChChunk& dest);

	void printMxObInfo(MxObChunk& mxobc, std::ofstream& ofs);

	void logRIFFOMNI(RIFFOMNIChunk& src, std::string filename);

	bool logging;
	bool reporting;
	bool rip_smackers;
	bool rip_flcs;
	bool rip_phonemes;
	bool rip_bmaps;
	bool rip_waves;
	bool force_wave_loop;
	double loopfadelen;
	double loopendsilence;
};


};	// end of namespace LegoIsland

#endif


#pragma once