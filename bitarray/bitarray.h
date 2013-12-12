#pragma once

#ifndef __BITVECTOR_H_
#define __BITVECTOR_H_


#include "bitop.h"
#include "framework/archive.h"


#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <memory>
#include <vector>
#include <cstring>

#include "mem/local_mem.h"

namespace mscds {

class BitArray;

class BitArrayBuilder {
public:
	static BitArray create(size_t bitlen);
	static BitArray create(size_t bitlen, const char * ptr);
	static BitArray adopt(size_t bitlen, StaticMemRegionPtr p);
};

class BitArray {
public:
	const static unsigned int WORDLEN = 64;

	void setbits(size_t bitindex, uint64_t value, unsigned int len);
	void setbit(size_t bitindex, bool value);

	uint64_t bits(size_t bitindex, unsigned int len) const;
	bool bit(size_t bitindex) const;
	bool operator[](size_t i) const { return bit(i); }
	uint8_t byte(size_t pos) const;

	uint64_t count_one() const;
	void fillzero();
	void fillone();

	void setword(size_t pos, uint64_t val);
	uint64_t word(size_t pos) const;
	size_t length() const { return bitlen; }
	size_t word_count() const;

	//--------------------------------------------------
	BitArray();
	BitArray(const BitArray& other) = default;
	BitArray& operator=(const BitArray& other) = default;
	BitArray(BitArray&& mE) : bitlen(mE.bitlen), data(std::move(mE.data)) {}
	BitArray& operator=(BitArray&& mE) { bitlen = mE.bitlen; data = std::move(mE.data); return *this; }

	StaticMemRegionPtr data_ptr() const { return data; }

	void clear();
	~BitArray();

	InpArchive& load(InpArchive& ar);
	OutArchive& save(OutArchive& ar) const;
	OutArchive& save_nocls(OutArchive& ar) const;
	InpArchive& load_nocls(InpArchive& ar);
	std::string to_str() const;

	inline static uint64_t ceildiv(uint64_t a, uint64_t b) {
		/* return (a != 0 ? ((a - 1) / b) + 1 : 0); // overflow free version */
		return (a + b - 1) / b;
	}
private:
	friend class BitArrayBuilder;
	size_t bitlen;
	StaticMemRegionPtr data;
};

//---------------------------------------------------------------------


inline BitArray::BitArray(): bitlen(0) {}

inline void BitArray::clear() { bitlen = 0; data = StaticMemRegionPtr(); }

inline void BitArray::setword(size_t pos, uint64_t val) { assert(pos < word_count()); data.setword(pos, val); }
inline uint64_t BitArray::word(size_t pos) const { assert(pos < word_count()); return data.getword(pos); }
inline size_t BitArray::word_count() const { return ceildiv(bitlen, WORDLEN); }

//inline const uint64_t* BitArray::data_ptr() const { return data; }

template<typename T>
struct CppArrDeleter {
	void operator()(void* p) const { delete[]((T*)p); }
};


inline BitArray BitArrayBuilder::create(size_t bitlen) {
	BitArray v;
	if (bitlen == 0) return v;
	assert(bitlen > 0);
	size_t arrlen = (size_t)BitArray::ceildiv(bitlen, BitArray::WORDLEN);
	LocalMemModel alloc;
	v.data = alloc.allocStaticMem(arrlen * sizeof(uint64_t));
	v.bitlen = bitlen;
	if (arrlen > 0) v.data.setword(arrlen - 1, 0);
	return v;
}

inline BitArray BitArrayBuilder::create(size_t bitlen, const char * ptr) {
	BitArray v = create(bitlen);
	size_t bytelen = (size_t)BitArray::ceildiv(bitlen, 8);
	v.data.write(0, bytelen, (const void*) ptr);
	return v;
}

inline BitArray BitArrayBuilder::adopt(size_t bitlen, StaticMemRegionPtr p) {
	BitArray v;
	v.data = p;
	v.bitlen = bitlen;
	return v;
}

/*
inline BitArray BitArray::clone_mem() const {
	BitArray v(this->bitlen);
	std::copy(data, data + word_count(), v.data);
	return v;
}*/

inline BitArray::~BitArray() { }

inline uint64_t BitArray::bits(size_t bitindex, unsigned int len) const {
	assert(len <= WORDLEN); // len > 0
	assert(bitindex + len <= bitlen);
	if (len==0) return 0;
	uint64_t i = bitindex / WORDLEN;
	unsigned int j = bitindex % WORDLEN;
	//uint64_t mask = (len < WORDLEN) ? ((1ull << len) - 1) : ~0ull;
	uint64_t mask = ((~0ull) >> (WORDLEN - len));
	if (j + len <= WORDLEN)
		return (word(i) >> j) & mask;
	else
		return (word(i) >> j) | ((word(i + 1) << (WORDLEN - j)) & mask);
}

inline void BitArray::setbits(size_t bitindex, uint64_t value, unsigned int len) {
	assert(len <= WORDLEN && len > 0);
	assert(bitindex + len <= bitlen);
	uint64_t i = bitindex / WORDLEN;
	unsigned int j = bitindex % WORDLEN;
	//uint64_t mask = (len < WORDLEN) ? ((1ull << len) - 1) : ~0ull; // & (~0ull >> (WORDLEN - len))
	uint64_t mask = ((~0ull) >> (WORDLEN - len));
	value = value & mask;
	uint64_t v = (word(i) & ~(mask << j)) | (value << j);
	data.setword(i, v);
	if (j + len > WORDLEN)
		setword(i+1, (word(i+1) & ~ (mask >> (WORDLEN - j))) | (value >> (WORDLEN - j)));
}

inline bool BitArray::bit(size_t bitindex) const {
	assert(bitindex < bitlen);
	return (word(bitindex / WORDLEN) & (1ULL << (bitindex % WORDLEN))) != 0;
}

inline void BitArray::setbit(size_t bitindex, bool value) {
	assert(bitindex < bitlen);
	uint64_t v = word(bitindex / WORDLEN);
	if (value) v |= (1ULL << (bitindex % WORDLEN));
	else v &= ~(1ULL << (bitindex % WORDLEN));
	setword(bitindex / WORDLEN, v);
}

inline uint8_t BitArray::byte(size_t pos) const {
	assert(pos * 8 < bitlen);
	return data.getchar(pos);
}


//------------------------------------------------------------------------
class FixedWArray {
private:
	BitArray b;
	unsigned int width;
public:
	FixedWArray(): width(0) {}
	FixedWArray(const FixedWArray& other): b(other.b), width(other.width) {}
	FixedWArray(const BitArray& bits, unsigned int width_): b(bits), width(width_) {}
	static FixedWArray create(size_t len, unsigned int width) {
		 return FixedWArray(BitArrayBuilder::create(len*width), width);
	}

	static FixedWArray build(const std::vector<unsigned int>& values);

	uint64_t operator[](size_t i) const { return b.bits(i*width, width); }
	void set(size_t i, uint64_t v) { b.setbits(i*width, v, width); }

	void fillzero() { b.fillzero(); }
	void clear() { b.clear(); width = 0; }
	InpArchive& load(InpArchive& ar);
	OutArchive& save(OutArchive& ar) const;
	size_t length() const { return b.length() / width; }
	unsigned int getWidth() const { return width; }
	const BitArray getArray() const { return b; }
	std::string to_str() const;
};


template<typename Iterator>
FixedWArray bsearch_hints(Iterator start, size_t arrlen, size_t rangelen, unsigned int lrate) {
	FixedWArray hints = FixedWArray::create((rangelen >> lrate) + 2, ceillog2(arrlen + 1));
	uint64_t i = 0, j = 0, p = 0;
	do {
		hints.set(i, p);
		//assert(j == (1 << ranklrate)*i);
		//assert(A[p] >= j);
		++i;  j += (1ULL<<lrate);
		while (p < arrlen && *start < j) { ++p; ++start; }
	} while (j < rangelen);
	for (unsigned int k = i; k < hints.length(); k++)
		hints.set(k, arrlen);
	return hints;
}

} //namespace
#endif // __BITVECTOR_H_
