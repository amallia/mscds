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
	typedef DeltaCodeArr QueryTp;
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
	typedef DeltaCodeArrBuilder BuilderTp;

	struct Enumerator: public EnumeratorInt<uint64_t> {
	public:
		Enumerator() {}
		Enumerator(const Enumerator& o): is(o.is), c(o.c) {}
		bool hasNext() const;
		uint64_t next();
	private:
		mscds::IWBitStream is;
		coder::DeltaCoder dc;
		mutable coder::CodePr c;
		friend class DeltaCodeArr;
	};
	void getEnum(uint64_t pos, Enumerator * e) const;

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
	typedef DiffDeltaArr QueryTp;
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
	typedef DiffDeltaArrBuilder BuilderTp;
	struct Enumerator: public EnumeratorInt<uint64_t> {
	public:
		Enumerator() {}
		bool hasNext() const;
		uint64_t next();
		Enumerator(const Enumerator& o): is(o.is), val(o.val) {}
	private:
		mscds::IWBitStream is;
		coder::DeltaCoder dc;
		mutable uint64_t val;
		friend class DiffDeltaArr;
	};
	void getEnum(uint64_t pos, Enumerator * e) const;

private:
	uint64_t len;
	unsigned int sample_rate;
	SDArrayQuery ptr;
	BitArray enc;
	friend class DiffDeltaArrBuilder;
};


}