#ifdef ENABLE_MOHAWK

#include "RipModule.h"
#include "../RipperFormats.h"
#include <vector>

namespace Mohawk
{


class MohawkRip : public RipModule
{
public:
	MohawkRip()
		: RipModule("Mohawk",
		"Extracts graphics, sound, and text from games using\n"
		"Broderbund's Mohawk engine\n"
		"Currently, only Where in Time is Carmen Sandiego is supported;\n"
		"other games will probably cause freezing or crashing",
		false, "",
		supported, supported, supported,
		supported, supported
	/*	RipSupport(supported, supported, supported, supported, unnecessary) */ ),
		ripseq(true) { };

	bool can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat);

	RipperFormats::RipResults rip(RipUtil::MembufStream& stream, const std::string& fprefix,
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat);

private:

	RipperFormats::FileFormatData check_file_format(const std::string& filename, int bufsize);

	void check_params(int argc, char* argv[]);

	bool ripseq;

	enum FormatSubtype
	{
		fmt_cstime = 1
	};

	FormatSubtype formatsubtype;



	typedef int FormatSetting;

	// Mohawk data types
	enum MHWKDatType
	{
		mhwk_dattype_none,
		cinf, conv, hots, invo, qars,
		regs, tbmh, tbmp, tcnt, tpal,
		tscr, stri, scen, twav, strl
	};

	// container for Mohawk 8-byte header chunk data
	struct MHWKHeadChunk
	{
		MHWKHeadChunk(MHWKDatType dat = mhwk_dattype_none,
			int start = 0, int end = 0)
			: dattype(dat), indstart(start), indend(end) { };

		MHWKDatType dattype;
		int indstart;
		int indend;
	};

	// container for Mohawk index table data
	struct MHWKIndexTableEntry
	{
		MHWKIndexTableEntry(MHWKDatType dat = mhwk_dattype_none,
			int unk = 0, int ind = 0)
			: dattype(dat), unknown(unk), index(ind) { };

		MHWKDatType dattype;
		int unknown;
		int index;
	};

	// container for Mohawk address table data
	struct MHWKAddrTableEntry
	{
		MHWKAddrTableEntry(MHWKDatType type, int addr = -1, int len = -1,
			int unk1 = -1, int unk2 = -1)
			: dattype(type), address(addr), length(len), unknown_1(unk1),
			unknown_2(unk2) { };

		MHWKDatType dattype;
		int address;
		int length;
		int unknown_1;
		int unknown_2;
	};

	typedef std::vector<int> REGSEntries;

	struct REGSEntriesList
	{
		REGSEntriesList()
			: id(0) { };

		int id;
		REGSEntries entries;
	};

	typedef std::vector<REGSEntriesList> REGSChunks;

	struct TBMHEntry
	{
		TBMHEntry()
			: xoffset(0), yoffset(0) { };

		int xoffset;
		int yoffset;
		RipUtil::BitmapData image;
	};

	typedef std::vector<TBMHEntry> TBMHEntryList;

	struct SequenceSizingInfo
	{
		SequenceSizingInfo()
			: width(0), height(0), centerx(0), centery(0) { };

		int width;
		int height;
		int centerx;
		int centery;
	};

	void rip_tbmh_from_stream(RipUtil::MembufStream& stream, RipperFormats::RipResults& results,
		const RipperFormats::RipperSettings& ripset,
		const RipperFormats::FileFormatData& fmtdat,
		REGSEntries& regs_x, REGSEntries& regs_y,
		std::vector<RipUtil::BitmapPalette>& palettes,
		int& palettenum,
		int& framenum, const std::string& fprefix, std::vector<MHWKIndexTableEntry>& identries,
		int i,
		int& entrynum, int& chunkstart, int& numentries,
		int& value1, int& value2, int& value3, int& value4, int& value5);

	SequenceSizingInfo compute_sequence_enclosing_dimensions(
		const TBMHEntryList& components)
	{
		SequenceSizingInfo seqsize;

		// find the topmost, rightmost, bottommost, and leftmost extent of
		// the components in the sequence
		int topbound = 0;
		int rightbound = 0;
		int leftbound = 0;
		int bottombound = 0;

		// check static frames
		for (TBMHEntryList::const_iterator sit = components.begin();
			sit != components.end(); sit++)
		{
			// is this component the leftmost so far?
			// sanity check: anything further left than -1024
			// is probably invalid
			if (sit->xoffset < leftbound)
			{
				leftbound = sit->xoffset;

				// update the centerpoint
				seqsize.centerx = -leftbound;
			}
			// is this component the topmost so far?
			if (sit->yoffset < topbound)
			{
				topbound = sit->yoffset;

				// update the centerpoint
				seqsize.centery = -topbound;
			}

			// is this component the rightmost so far?
			if (sit->xoffset + sit->image.get_width() > rightbound)
			{
				rightbound = sit->xoffset + sit->image.get_width();
			}
			// is this component the bottommost so far?
			if (sit->yoffset + sit->image.get_height() > bottombound)
			{
				bottombound = sit->yoffset + sit->image.get_height();
			}
		}

		seqsize.width = rightbound - leftbound;
		seqsize.height = bottombound - topbound;

		return seqsize;
	}

	// it's an array
	struct RawData
	{
		RawData(int sz)
		{
			data = new char[sz];
			size = sz;
		}
		RawData()
		{
			data = 0;
			size = 0;
		}
		~RawData()
		{
			delete data;
		}

		void resize(int sz)
		{
			delete data;
			data = new char[sz];
			size = sz;
		}

		char* data;
		int size;
	};

	static bool IDsByIndexNum(const MHWKIndexTableEntry& fir, const MHWKIndexTableEntry& sec);

	static bool IDsByDatType(const MHWKIndexTableEntry& fir, const MHWKIndexTableEntry& sec);

	// read Mohawk header chunk
	void mhwk_read_header(RipUtil::MembufStream& stream, MHWKDatType dat,
		std::vector<MHWKHeadChunk>& entries);

	// skip Mohawk type 1 table (starting from 2b initial chunklen)
	void mhwk_skip_type1_table(RipUtil::MembufStream& stream);

	// read Mohawk type 1 table (starting from 2b initial chunklen)
	void mhwk_read_type1_table(RipUtil::MembufStream& stream,
		std::vector<MHWKIndexTableEntry>& entries);

	// read Mohawk type 1 chunk (starting from 2b initial chunklen)
	void mhwk_read_type1_chunk(RipUtil::MembufStream& stream, MHWKDatType dat,
		std::vector<MHWKIndexTableEntry>& entries);

	// skip Mohawk type 2 table (starting from 2b initial chunklen)
	void mhwk_skip_type2_table(RipUtil::MembufStream& stream);

	// add data from Mohawk type 2 table to a vector of table entries
	void mhwk_read_type2_table(RipUtil::MembufStream& stream, MHWKDatType type,
		 std::vector<MHWKAddrTableEntry>& entries);

	// given a MHWKDatType, return its name as a string
	std::string mhwk_get_dattype_name(MHWKDatType dattype);

	// read from a Mohawk WAVE chunk into a PCMData object
	void mhwk_read_wave(RipUtil::MembufStream& stream, RipUtil::PCMData& dat,
		const RipperFormats::RipperSettings& ripset, RipperFormats::FileFormatData fmtdat, int len = -1);

	// read from a Mohawk tBMP chunk into a BitmapData object
	void mhwk_read_tbmp(RipUtil::MembufStream& stream, RipUtil::BitmapData& dat,
		const RipperFormats::RipperSettings& ripset, RipperFormats::FileFormatData fmtdat, int len = -1);

	void extract_tbmp(RipUtil::MembufStream& stream, RipUtil::BitmapData& dat,
		int width, int height, int bytesperrow, int format);

	// read an RIFF chunk into dat
	void read_riff(RipUtil::MembufStream& stream, RipUtil::PCMData& dat, RipperFormats::FileFormatData fmtdat, int len = -1);

	// decompress standard LZ format data pointed to by stream
	void decompress_lz(RipUtil::MembufStream& stream, RawData& outdat);

};

	// constant header chunk IDs


	// RIFF file identifier
	const static char riff_id[4] 
		= { 'R', 'I', 'F', 'F' };
	// RIFF subchunk2 identifier
	const static char riff_sbchnk2_id[4] 
		= { 'd', 'a', 't', 'a' };

	// Mohawk
	
	// file id
	const static char mhwk_id[4]
		= { 'M', 'H', 'W', 'K' };
	// file id 2
	const static char mhwk_rsrc_id[4]
		= { 'R', 'S', 'R', 'C' };
	// header IDs
	// CINF chunkid
	const static char mhwk_cinf_id[4]
		= { 'C', 'I', 'N', 'F' };
	// CONV chunkid
	const static char mhwk_conv_id[4]
		= { 'C', 'O', 'N', 'V' };
	// HOTS chunkid
	const static char mhwk_hots_id[4]
		= { 'H', 'O', 'T', 'S' };
	// INVO chunkid
	const static char mhwk_invo_id[4]
		= { 'I', 'N', 'V', 'O' };
	// QARS chunkid
	const static char mhwk_qars_id[4]
		= { 'Q', 'A', 'R', 'S' };
	// REGS chunkid
	const static char mhwk_regs_id[4]
		= { 'R', 'E', 'G', 'S' };
	// tBMH chunkid
	const static char mhwk_tbmh_id[4]
		= { 't', 'B', 'M', 'H' };
	// tBMP chunkid
	const static char mhwk_tbmp_id[4]
		= { 't', 'B', 'M', 'P' };
	// tCNT chunkid
	const static char mhwk_tcnt_id[4]
		= { 't', 'C', 'N', 'T' };
	// tPAL chunkid
	const static char mhwk_tpal_id[4]
		= { 't', 'P', 'A', 'L' };
	// tSCR chunkid
	const static char mhwk_tscr_id[4]
		= { 't', 'S', 'C', 'R' };
	// data address chunks
	// STRI chunkid
	const static char mhwk_stri_id[4]
		= { 'S', 'T', 'R', 'I' };
	// SCEN chunkid
	const static char mhwk_scen_id[4]
		= { 'S', 'C', 'E', 'N' };
	// tWAV chunkid
	const static char mhwk_twav_id[4]
		= { 't', 'W', 'A', 'V' };
	// data chunks
	// STRI optional chunk2 id
	const static char mhwk_strl_id[4]
		= { 'S', 'T', 'R', 'L' };
	// sound data id
	const static char mhwk_wave_id[4]
		= { 'W', 'A', 'V', 'E' };
	// optional WAVE Cue# chunk
	const static char mhwk_cuenum_id[4]
		= { 'C', 'u', 'e', '#' };
	// WAVE ADPC chunk
	const static char mhwk_adpc_id[4]
		= { 'A', 'D', 'P', 'C' };
	// WAVE Data chunk
	const static char mhwk_wavedata_id[4]
		= { 'D', 'a', 't', 'a' };

	// Mohawk sound format identifier: IMA ADPCM
	const static int mhwk_imaadpcm_fmtid = 1;


};	// end of namespace Mohawk

#endif


#pragma once
