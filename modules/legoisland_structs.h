#include <vector>
#include "../utils/MembufStream.h"
#include "../utils/DatManip.h"

namespace LegoIsland
{


/* Forward declarations of chunk types due to interdependency */
struct ChunkBase;
struct LISTChunkBase;
struct ChunkHead;
struct LISTChunkHead;
struct MxHdChunk;
struct MxOfChunk;
struct MxStChunk;
struct MxObChunk;
struct MxDaChunk;
struct MxChChunk;
struct padChunk;
struct LISTMxStChunk;
struct LISTMxChChunk;
struct LISTMxDaChunk;
struct RIFFOMNIChunk;


/* Chunk 4CCs */
const static char RIFF_hd[4] = { 'R', 'I', 'F', 'F' };
const static char OMNI_hd[4] = { 'O', 'M', 'N', 'I' };
const static char LIST_hd[4] = { 'L', 'I', 'S', 'T' };
const static char MxHd_hd[4] = { 'M', 'x', 'H', 'd' };
const static char MxOf_hd[4] = { 'M', 'x', 'O', 'f' };
const static char MxSt_hd[4] = { 'M', 'x', 'S', 't' };
const static char MxOb_hd[4] = { 'M', 'x', 'O', 'b' };
const static char MxDa_hd[4] = { 'M', 'x', 'D', 'a' };
const static char MxCh_hd[4] = { 'M', 'x', 'C', 'h' };
const static char pad_hd[4] = { 'p', 'a', 'd', 0x20 };

/* Chunk IDs for internal use */
enum ChunkID
{
	chunkid_none, chunkid_unknown,
	RIFF, OMNI, LIST, MxHd, MxOf, MxSt, MxOb, MxDa, MxCh, pad
};

// Given a chunk 4CC, return its internal identifier
ChunkID getChunkID(char* s, int n);

// Given a chunk length, round it upwards to account for padding
// currently rounds to 16-bit boundary
void roundChunkLength(int& length);

// Read a chunk header from stream into internal chunk
void readChunkHead(RipUtil::MembufStream& stream, ChunkBase& chunk);

// Read a LIST chunk header from stream into internal chunk
void readLISTChunkHead(RipUtil::MembufStream& stream, LISTChunkBase& chunk);


/* Data containers */


/* Address table */
typedef std::vector<unsigned int> MxOfTable;

/* Abstract base class for all chunks */
struct ChunkBase
{
public:
	ChunkID type() { return typeident; }
	std::string typestring() { return typestr; }
	unsigned int address() { return addressnum; }
	unsigned int size() { return sizenum; }

	void set_type(ChunkID newtype) { typeident = newtype; }
	void set_typestring(std::string newtypestring) { typestr = newtypestring; }
	void set_address(unsigned int newaddress) { addressnum = newaddress; }
	void set_size(unsigned int newsize) { sizenum = newsize; }
	void set_data_nopad_size(unsigned int newnopad) { data_nopad_num = newnopad; }

	unsigned int nextaddress() { return addressnum + sizenum; }
	unsigned int data_nopad_size() { return data_nopad_num; }
	unsigned int dataend() { return addressnum + data_nopad_num; }
	virtual unsigned int datasize() { return sizenum - 8; }
	virtual unsigned int datastart() { return addressnum + 8; }
protected:
	ChunkBase()
		: typeident(chunkid_none), addressnum(-1), sizenum(-1), data_nopad_num(-1) { };
	ChunkBase(RipUtil::MembufStream& stream)
		: typeident(chunkid_none)
	{
		readChunkHead(stream, *this);
	}
private:
	ChunkID typeident;
	std::string typestr;
	unsigned int addressnum;
	unsigned int sizenum;
	unsigned int data_nopad_num;
};

/* Abstract base class for LIST chunks */
struct LISTChunkBase : public ChunkBase
{
	LISTChunkBase()
		: ChunkBase(), listid(chunkid_none) { };
	LISTChunkBase(RipUtil::MembufStream& stream)
		: ChunkBase()
	{ 
		readLISTChunkHead(stream, *this);
	};

	ChunkID listtype() { return listid; }
	std::string liststring() { return liststr; }

	void set_listtype(ChunkID newlistid) { listid = newlistid; }
	void set_liststr(std::string newliststr) { liststr = newliststr; }
	
	unsigned int datasize() { return size() - 12; }
	unsigned int datastart() { return address() + 12; }

private:
	ChunkID listid;
	std::string liststr;
};

/* Generic container for header of any chunk type */
struct ChunkHead : public ChunkBase
{
	ChunkHead()
		: ChunkBase() { };
	ChunkHead(RipUtil::MembufStream& stream)
		: ChunkBase(stream) { };
};

/* Generic container for header of LIST chunks */
struct LISTChunkHead : public LISTChunkBase
{
	LISTChunkHead()
		: LISTChunkBase() { };
	LISTChunkHead(RipUtil::MembufStream& stream)
		: LISTChunkBase(stream) { };
};

/* Data representations of contents of LIST chunks */

typedef std::vector<MxStChunk> MxStList;

typedef std::vector<MxObChunk> MxObList;

typedef std::vector<MxChChunk> MxChList;

/* MxHd chunk */
struct MxHdChunk : public ChunkBase
{
	MxHdChunk()
		: ChunkBase() { };

	int unk1;
	int unk2;
	int unk3;
	int unk4;
	int unk5;
};

/* MxOf chunk */
struct MxOfChunk : public ChunkBase
{
	MxOfChunk()
		: ChunkBase() { };

	unsigned int numentries;
	MxOfTable entries;
};

/* "pad " chunk */
struct padChunk : public ChunkBase
{
	padChunk()
		: ChunkBase() { };
};

/* LIST/MxDa chunk */
struct LISTMxDaChunk : public LISTChunkBase
{
	LISTMxDaChunk()
		: LISTChunkBase() { };


	padChunk padc;
	MxChList entries;
};

/* LIST/MxCh chunk */
struct LISTMxChChunk : public LISTChunkBase
{
	LISTMxChChunk()
		: LISTChunkBase() { };

	unsigned int mxobcount;
	MxObList entries;
};

/* MxOb chunk */
struct MxObChunk : public ChunkBase
{
	MxObChunk()
		: ChunkBase() { };

	int mxobid;			// object ID number (probably)
	std::string typecstr;// C-string something
	int unk1;			// ?
	std::string name;	// C-string name
	int thingid;
	int unk5;
	int unk6;
	int unk7;
	int unk8;
	char unk2[72];		// block of unknown stuff, probably ints
	int objinf_runlen;	// 16-bit byte length of obinf string
	std::string objinf;	// C-string (?) with some kind of attribute info
	std::string filename;// C-string original filename
	LISTMxChChunk listmxchc;
	char unk3[24];		// more stuff
};

/* MxCh chunk */
struct MxChChunk : public ChunkBase
{
	MxChChunk()
		: ChunkBase() { };

	int unk1;
	int thingid;
};

/* MxSt chunk */
struct MxStChunk : public ChunkBase
{
	MxStChunk()
		: ChunkBase() { };

	MxObChunk mxobc;
	LISTMxChChunk listmxchc;
	LISTMxDaChunk listmxdac;
};

/* LIST/MxSt chunk */
struct LISTMxStChunk : public LISTChunkBase
{
	LISTMxStChunk()
		: LISTChunkBase() { };

	MxStList entries;
	padChunk padc;
};

/* RIFF/OMNI master container */
struct RIFFOMNIChunk : public ChunkBase
{
	RIFFOMNIChunk()
		: ChunkBase() { };

	MxHdChunk mxhdc;
	MxOfChunk mxofc;
	LISTMxStChunk listmxstc;
};



};	// end of namespace LegoIsland


#pragma once
