#pragma once

#include <stdint.h>
#include "archive.h"
#include "bitarray/bitstream.h"
#include "intarray/sdarray.h"
#include "intarray/intarray.h"
#include "codec/deltacoder.h"

namespace mscds {

class DeltaCodeArr;

class DeltaCodeArrBuilder {
public:
	DeltaCodeArrBuilder();
	void init(unsigned int rate);
	void add(uint64_t val);
	void build(OArchive& ar);
	void build(DeltaCodeArr * out);
	void clear();
private:
	coder::DeltaCoder dc;
	OBitStream enc;
	SDArrayBuilder ptrbd;
	unsigned int sample_rate, i;
};



class DeltaCodeArr {
public:
	uint64_t lookup(uint64_t pos) const;
	uint64_t operator[](uint64_t pos) const { return lookup(pos); }
	
	void load(IArchive& ar);
	void save(OArchive& ar) const;
	void clear();

	class Iterator {
	public:
		Iterator(): cp(false) {}
		uint64_t operator*() const;
		Iterator& operator++();
	private:
		mscds::IWBitStream is;
		coder::DeltaCoder dc;
		mutable coder::CodePr c;
		mutable bool cp;
		friend class DeltaCodeArr;
	};
	Iterator getItr(uint64_t pos) const;

private:
	uint64_t len;
	unsigned int sample_rate;
	SDArrayQuery ptr;
	BitArray enc;
	friend class DeltaCodeArrBuilder;
};



//-----------------------------------------------------------------

class DiffDeltaArr;

class DiffDeltaArrBuilder {
public:
	DiffDeltaArrBuilder();
	void init(unsigned int rate);
	void add(uint64_t val);
	void build(OArchive& ar);
	void build(DiffDeltaArr * out);
	void clear();
private:
	coder::DeltaCoder dc;
	OBitStream enc;
	SDArrayBuilder ptrbd;
	unsigned int sample_rate, i;
	int64_t lastval;
};



class DiffDeltaArr {
public:
	uint64_t lookup(uint64_t pos) const;
	uint64_t operator[](uint64_t pos) const { return lookup(pos); }
	
	void load(IArchive& ar);
	void save(OArchive& ar) const;
	void clear();

	class Iterator {
	public:
		Iterator(): val(0) {}
		uint64_t operator*() const;
		Iterator& operator++();
	private:
		mscds::IWBitStream is;
		coder::DeltaCoder dc;
		mutable uint64_t val;
		friend class DiffDeltaArr;
	};
	Iterator getItr(uint64_t pos) const;

private:
	uint64_t len;
	unsigned int sample_rate;
	SDArrayQuery ptr;
	BitArray enc;
	friend class DiffDeltaArrBuilder;
};


}