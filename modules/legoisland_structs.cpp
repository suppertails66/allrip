#include "legoisland_structs.h"
#include "../utils/MembufStream.h"

namespace LegoIsland
{


ChunkID getChunkID(char* s, int n)
{
	if (n < 4) return chunkid_unknown;

	if (RipUtil::quickcmp(s, RIFF_hd, 4)) return RIFF;
	else if (RipUtil::quickcmp(s, OMNI_hd, 4)) return OMNI;
	else if (RipUtil::quickcmp(s, LIST_hd, 4)) return LIST;
	else if (RipUtil::quickcmp(s, MxHd_hd, 4)) return MxHd;
	else if (RipUtil::quickcmp(s, MxOf_hd, 4)) return MxOf;
	else if (RipUtil::quickcmp(s, MxSt_hd, 4)) return MxSt;
	else if (RipUtil::quickcmp(s, MxOb_hd, 4)) return MxOb;
	else if (RipUtil::quickcmp(s, MxDa_hd, 4)) return MxDa;
	else if (RipUtil::quickcmp(s, MxCh_hd, 4)) return MxCh;
	else if (RipUtil::quickcmp(s, pad_hd, 4)) return pad;
	else return chunkid_unknown;
}

void roundChunkLength(int& length)
{
	if (length % 2)
		length += 1;
}

void readChunkHead(RipUtil::MembufStream& stream, ChunkBase& chunk)
{
	chunk.set_address(stream.tellg());

	char hdcheck[4];
	stream.read(hdcheck, 4);
	chunk.set_type(getChunkID(hdcheck, 4));
	chunk.set_typestring(std::string(hdcheck, 4));

	int sz = stream.read_int(4, DatManip::le);
	chunk.set_data_nopad_size(sz);
	roundChunkLength(sz);
	chunk.set_size(sz + 8);

	stream.seekg(chunk.address());
}

void readLISTChunkHead(RipUtil::MembufStream& stream, LISTChunkBase& chunk)
{
	readChunkHead(stream, chunk);
	
	char hdcheck[4];
	stream.seek_off(8);
	stream.read(hdcheck, 4);
	chunk.set_listtype(getChunkID(hdcheck, 4));
	chunk.set_liststr(std::string(hdcheck, 4));
	chunk.set_data_nopad_size(chunk.data_nopad_size() - 4);
	stream.seekg(chunk.address());
}


}	// end of namespace LegoIsland