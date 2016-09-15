#include "launch.h"
#include "utils/DatManip.h"
#include "RipperFormats.h"
#include "RipModules.h"
#include <ctime>
#include <iostream>
#include <string>

using std::cout;
using std::cerr;
using namespace Ripper;
using namespace RipperFormats;
using namespace RipUtil;

char wait_for_key()
{
	char c;
	std::cin >> c;
	return c;
}


int main(int argc, char** argv)
{
	try {

	// load modules
	RipModules mods;

	if (argc < 2)
	{
		print_base_usage_text();
		// print mod-defined usage info
		// print header only if at least one mod uses it
		int paramcheck = mods.num_mods();
		while (--paramcheck + 1)
		{
			if (mods[paramcheck]->has_extra_parameters)
			{
//				cout << "Game specific:" << '\n'
//					<< '\n';
				for (int i = 0; i < mods.num_mods(); i++)
				{
					if (mods[i]->has_extra_parameters)
					{
//						cout << mods[i]->module_name << '\n';
						cout << mods[i]->extra_parameters_text;
//						cout << '\n';
					}
				}
				break;
			}
		}
//		wait_for_key();
		return 1;
	}
	
	RipperSettings ripset;
	configure_parameters(argc, argv, ripset);
	ripset.argc = argc;
	ripset.argv = argv;

	int timer = std::clock();
	
//	for (int i = 0; i < mods.num_mods(); i++)
//	{
//		cout << mods[i]->module_name << '\n';
//	}
//	cout << mods.num_mods() << " mods loaded" << '\n';

	std::string filename = argv[1];
	std::string shortfname = get_short_filename(filename);
	std::string fprefix = shortfname;
	if (ripset.outpath != "")
		fprefix = ripset.outpath;

	MembufStream stream(filename, MembufStream::rb);

	FileFormatData fmtdat;
	RipResults results;
	bool ripped = false;

	if (mods.num_mods() == 0)
	{
//		cout << "Ripper was built with no modules!" << '\n';
		return 5;
	}
	for (int i = 0; i < mods.num_mods(); i++)
	{
		if (mods[i]->can_rip(stream, ripset, fmtdat))
		{
			cout << "Input file: " << shortfname << '\n';
//			cout << "mod " << i << " (" << mods[i]->module_name << ") " <<  "can rip" << '\n';
			results = mods[i]->rip(stream, fprefix, ripset, fmtdat);
			ripped = true;
//			cout << "Rip results:" << '\n'
//				<< '\n';
//			cout << "Graphics ripped: " << results.graphics_ripped << '\n'
//				<< "Animations ripped: " << results.animations_ripped << '\n'
//				<< "Animation frames ripped: " << results.animation_frames_ripped << '\n'
//				<< "Audio files ripped: " << results.audio_ripped << '\n'
//				<< "Strings ripped: " << results.strings_ripped << '\n'
//				<< "Raw files ripped: " << results.data_ripped << '\n'
//				<< "Total: " << results.graphics_ripped + results.animation_frames_ripped 
//					+ results.audio_ripped + results.strings_ripped
//					+ results.data_ripped << '\n';
		}
		stream.reset();
	}

	if (!ripped)
	{
		cout << "No module could recognize this file: no output" << '\n';
	}

	timer = clock() - timer;
	cout << "Time elapsed: " << (double)timer/CLOCKS_PER_SEC << " secs" << '\n';

//	wait_for_key();
	return 0;

	}
//	catch(...) { wait_for_key(); }
	catch (FileOpenException e)
	{
		cerr << "Error opening file " << e.fname << " for reading " << '\n';
		return 4;
	}
	catch (std::exception& e)
	{
		cerr << "Error: " << e.what() << '\n';
		return 3;
	}
	catch (...)
	{
		cerr << "Unhandled exception: aborting" << '\n';
		return 2;
	}
}
