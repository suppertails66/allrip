#include "launch.h"
#include "utils/DatManip.h"
#include "utils/PCMData.h"

#include <iostream>

using std::cout;
using namespace RipUtil;

// print all non-module dependent usage info
void print_base_usage_text()
{

}

// configure RipperSettings from command line params
void configure_parameters(int argc, char** argv, RipperFormats::RipperSettings& ripset)
{
	// first pass: single-flag params
	for (int i = 2; i < argc; i++)
	{
		if (quickstrcmp(argv[i], "--ripallraw"))
			ripset.ripallraw = true;
		else if (quickstrcmp(argv[i], "--copycommon"))
			ripset.copycommon = false;
		else if (quickstrcmp(argv[i], "--nographics"))
			ripset.ripgraphics = false;
		else if (quickstrcmp(argv[i], "--noanimations"))
			ripset.ripanimations = false;
		else if (quickstrcmp(argv[i], "--noaudio"))
			ripset.ripaudio = false;
		else if (quickstrcmp(argv[i], "--nostrings"))
			ripset.ripstrings = false;
		else if (quickstrcmp(argv[i], "--ripdata"))
			ripset.ripdata = true;
		else if (quickstrcmp(argv[i], "--palettesoff"))
			ripset.guesspalettes = false;
		else if (quickstrcmp(argv[i], "--normalize"))
			ripset.normalize = true;
		else if (quickstrcmp(argv[i], "--decode_audio"))
			ripset.decode_audio = true;
	}

	// second pass: two-flag params
	for (int i = 2; i < argc - 1; i++)
	{		
		if (quickstrcmp(argv[i], "-output")
			|| quickstrcmp(argv[i], "-o"))
			ripset.outpath = argv[i + 1];
		else if (quickstrcmp(argv[i], "-encoding")
			|| quickstrcmp(argv[i], "-e"))
			ripset.encoding = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-bufsize")
			|| quickstrcmp(argv[i], "-b"))
			ripset.bufsize = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-start")
			|| quickstrcmp(argv[i], "-st"))
			ripset.startentry = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-end")
			|| quickstrcmp(argv[i], "-en"))
			ripset.endentry = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-palettenum")
			|| quickstrcmp(argv[i], "-pn"))
			ripset.palettenum = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-backgroundcolor")
			|| quickstrcmp(argv[i], "-bg"))
		{
			/* not implemented */
		}
		else if (quickstrcmp(argv[i], "-audsign")
			|| quickstrcmp(argv[i], "-as"))
		{
			if (quickstrcmp(argv[i + 1], "signed"))
				ripset.audsign = DatManip::has_sign;
			else if (quickstrcmp(argv[i + 1], "unsigned"))
				ripset.audsign = DatManip::has_nosign;
		}
		else if (quickstrcmp(argv[i], "-audend")
			|| quickstrcmp(argv[i], "-ae"))
		{
			if (quickstrcmp(argv[i + 1], "big"))
				ripset.audend = DatManip::be;
			else if (quickstrcmp(argv[i + 1], "little"))
				ripset.audend = DatManip::le;
		}
		else if (quickstrcmp(argv[i], "-channels")
			|| quickstrcmp(argv[i], "-ch"))
			ripset.channels = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-samprate")
			|| quickstrcmp(argv[i], "-sr"))
			ripset.samprate = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-sampwidth")
			|| quickstrcmp(argv[i], "-sw"))
			ripset.sampwidth = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-ignorebytes")
			|| quickstrcmp(argv[i], "-ib"))
			ripset.ignorebytes = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-ignoreend")
			|| quickstrcmp(argv[i], "-ie"))
			ripset.ignoreend = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-numloops")
			|| quickstrcmp(argv[i], "-nl"))
			ripset.numloops = from_string<int>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-loopstyle")
			|| quickstrcmp(argv[i], "-ls"))
		{
			if (quickstrcmp(argv[i + 1], "fade"))
				ripset.loopstyle = PCMData::fadeloop;
			else if (quickstrcmp(argv[i + 1], "tail"))
				ripset.loopstyle = PCMData::tailloop;
		}
		else if (quickstrcmp(argv[i], "-loopfadelen")
			|| quickstrcmp(argv[i], "-lf"))
			ripset.loopfadelen = from_string<double>(argv[i + 1]);
		else if (quickstrcmp(argv[i], "-loopfadesil")
			|| quickstrcmp(argv[i], "-ls"))
			ripset.loopfadesil = from_string<double>(argv[i + 1]);
	}
}
