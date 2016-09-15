#ifdef ENABLE_INDIAN

#include "RipModule.h"
#include "../RipperFormats.h"
#include "../utils/BitmapData.h"
#include <vector>

namespace IndCup
{


class IndianRip : public RipModule
{
public:
	IndianRip()
		: RipModule("Indian in the Cupboard",
		"Extracts graphics and audio from Viacom New Media's\n"
		"The Indian in the Cupboard",
		false, "",
		supported, supported, supported,
		supported, unnecessary),
		ripseq(true) { };

	// our usual check_format; set formatsubtype before returning true
	bool can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat);

	// our usual ripping procedure
	RipperFormats::RipResults rip(RipUtil::MembufStream& stream, const std::string& fprefix,
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat);

private:

	void check_params(int argc, char* argv[]);

	bool ripseq;

	enum FormatSubtype
	{
		fmt_indian = 1
	};

	FormatSubtype formatsubtype;

	enum DataType
	{
		datatype_none,
		datatype_unknown,
		bitmap, pal_bitmap, animation,
		palette, aiff
	};

	struct AddrTabEnt
	{
		AddrTabEnt()
			: address(0), length(0),
		dattype(datatype_unknown), id(0) { };

		int address;
		int length;
		DataType dattype;
		int id;
	};

	struct AnimationFrame
	{
		AnimationFrame()
			: fulllen(0),
			unk1(0), unk2(0), unk3(0),
			unk4(0), unk5(0), xoffset(0),
			yoffset(0), unk8(0), unk9(0),
			bytesperrow(0), width(0), height(0),
			unk14(0) { };

		int fulllen;
		int unk1;
		int unk2;
		int unk3;
		int unk4;
		int unk5;
		int xoffset;
		int yoffset;
		int unk8;
		int unk9;
		int bytesperrow;
		int width;
		int height;
		int unk14;

		RipUtil::BitmapData image;
	};

	typedef std::vector<AnimationFrame> AnimationFrameList;

	struct SequenceSizingInfo
	{
		SequenceSizingInfo()
			: width(0), height(0), centerx(0), centery(0) { };

		int width;
		int height;
		int centerx;
		int centery;
	};

	SequenceSizingInfo compute_sequence_enclosing_dimensions(
		const AnimationFrameList& components)
	{
		SequenceSizingInfo seqsize;

		// find the topmost, rightmost, bottommost, and leftmost extent of
		// the components in the sequence
		int topbound = 0;
		int rightbound = 0;
		int leftbound = 0;
		int bottombound = 0;

		// check static frames
		for (AnimationFrameList::const_iterator sit = components.begin();
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
			if (sit->xoffset + sit->width > rightbound)
			{
				rightbound = sit->xoffset + sit->width;
			}
			// is this component the bottommost so far?
			if (sit->yoffset + sit->height > bottombound)
			{
				bottombound = sit->yoffset + sit->height;
			}
		}

		seqsize.width = rightbound - leftbound;
		seqsize.height = bottombound - topbound;

		return seqsize;
	}


	// read a color from an Indian in the Cupboard palette
	// return color in 24-bit BGR form
	int indcup_read_color(RipUtil::MembufStream& stream);

	// read Indian in the Cupboard bitmap data into a BitmapData object,
	// starting from row byte count
	void indcup_read_bitmap(RipUtil::MembufStream& stream, RipUtil::BitmapData& dat, const RipperFormats::RipperSettings& ripset);

	// read Indian in the Cupboard animation frame data into a BitmapData object,
	// starting from row byte count
	void indcup_read_aniframe(RipUtil::MembufStream& stream, RipUtil::BitmapData& dat,
		int bytesperrow, int width, int height, const RipperFormats::RipperSettings& ripset);

	// read a stupidly malformed Indian in the Cupboard AIFF file into a PCMData object
	void indcup_read_aiff(RipUtil::MembufStream& stream, RipUtil::PCMData& dat, const RipperFormats::RipperSettings& ripset);

};


};	// end of namespace IndCup

#endif


#pragma once