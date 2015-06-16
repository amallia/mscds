#pragma once

#include "codec/rrr_codec.h"
#include "bitarray.h"
#include "rank25p.h"

#include "bitstream.h"

namespace mscds {
class RRR_WordAccessBuilder;

class RRR_WordAccess {
public:
    uint64_t getword(size_t i) const;
    uint8_t popcntw(size_t i) const;
	void load(InpArchive& ar);
	void save(OutArchive& ar) const;
    size_t count() const;
	typedef RRR_WordAccessBuilder BuilderTp;
private:
    uint64_t offset_loc(unsigned i) const;

	friend class RRR_WordAccessBuilder;
private:
	friend class RRR_WordAccessBuilder;
	static const unsigned OFFSET_BLK = 32;
	coder::RRR_Codec codec;
	
	FixedWArray opos;
	BitArray offset;

	static const unsigned OVERFLOW_BLK = 8;
	BitArray bitcnt;
	Rank25p pmark;
	BitArray overflow;
};

class RRR_WordAccessBuilder {
public:
    RRR_WordAccessBuilder();
    void init();
    void add(uint64_t word);

    void build(RRR_WordAccess* out);

    static void build_array(const BitArray& ba, RRR_WordAccess* out);
private:
    void _flush_overflow();
private:
	OBitStream offset, bitcnt, pmark, overflow;
	coder::RRR_Codec codec;
	unsigned overflow_mark;

	FixedWArrayBuilder opos;

	unsigned i, j;
};

}//namespace
