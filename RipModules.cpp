#include "RipModules.h"
#include "modules/indian.h"
#include "modules/mohawk.h"
#include "modules/atlas.h"
#include "modules/candyadv.h"
#include "modules/humongous.h"
#include "modules/legoisland.h"

namespace Ripper
{


RipModules::RipModules()
{
	#ifdef ENABLE_INDIAN
	rippers.push_back(new IndCup::IndianRip());
	#endif
	#ifdef ENABLE_MOHAWK
	rippers.push_back(new Mohawk::MohawkRip());
	#endif
	#ifdef ENABLE_ATLAS
	rippers.push_back(new Atlas::AtlasRip());
	#endif
	#ifdef ENABLE_CANDYADV
	rippers.push_back(new CandyAdv::CandyAdvRip());
	#endif
	#ifdef ENABLE_HUMONGOUS
	rippers.push_back(new Humongous::HERip());
	#endif
	#ifdef ENABLE_LEGOISLAND
	rippers.push_back(new LegoIsland::LegoIslandRip());
	#endif
}

RipModules::~RipModules()
{
	for (int i = 0; i < rippers.size(); i++)
		delete rippers[i];
}

RipModule* RipModules::operator[](int n)
{
	return rippers[n];
}

int RipModules::num_mods()
{
	return rippers.size();
}



};	// end namespace Ripper