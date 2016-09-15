#ifdef ENABLE_ATLAS

#include "RipModule.h"
#include "../RipperFormats.h"
#include "../utils/MembufStream.h"

#include <string>

namespace Atlas
{


class AtlasRip : public RipModule
{
public:
	AtlasRip()
		: RipModule("Atlas", "Ripper for Microsoft Atlas Game System resource files",
				true, "--atlas_copybmp\t"
				"Copies full bitmaps instead of splitting into frames\n",
				unsupported, unsupported,
				unsupported, unsupported,
				unsupported),
		copybmp(true) { };

	bool can_rip(RipUtil::MembufStream& stream, const RipperFormats::RipperSettings& ripset,
		RipperFormats::FileFormatData& fmtdat);

	RipperFormats::RipResults rip(RipUtil::MembufStream& stream, const std::string& fprefix, 
		const RipperFormats::RipperSettings& ripset, const RipperFormats::FileFormatData& fmtdat);
private:

	enum FormatSubtype
	{
		fmt_res_type2 = 2,		// address of address table at 0x14
		fmt_res_type3 = 3,		// address of address table at 0x3C
		fmt_res_type4 = 4,		// address of address table at 0x14
		fmt_unknown_addr14,
		fmt_unknown_addr3C
	};

	struct AtlasAddrTableEntry
	{
		AtlasAddrTableEntry()
			: address(0), length(0) { };

		int address;
		int length;
	};

	FormatSubtype formatsubtype;

	int addrtableaddr;

	bool copybmp;

	// check command line parameters and configure accordingly
	void check_params(const RipperFormats::RipperSettings& ripset);

};


};	// end of namespace Atlas

#endif


#pragma once