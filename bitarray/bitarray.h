#pragma once

#ifndef __BITVECTOR_H_
#define __BITVECTOR_H_




#include "bitop.h"
#include "archive.h"

#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <memory>
#include <algorithm>


namespace mscds {

class BitArraySeqBuilder {
	OArchive & ar;
	size_t pos, wl;
public:
	BitArraySeqBuilder(size_t wordlen, OArchive& _ar): ar(_ar), pos(0) {
		ar.startclass("Bitvector", 1);
		ar.var("bit_len").save(wordlen * 64);
		wl = wordlen;
	}

	void addword(uint64_t v) {
		ar.save_bin(&v, sizeof(v));
		pos++;
	}

	void done() {
		ar.endclass();
		assert(wl == pos);
	}
};

class BitArray {
public:
	const static unsigned int WORDLEN = 64;

	uint64_t bits(size_t bitindex, unsigned int len) const {
		assert(len <= WORDLEN && len > 0);
		assert(bitindex + len <= bitlen);
		uint64_t i = bitindex / WORDLEN;
		unsigned int j = bitindex % WORDLEN; 
		uint64_t mask = (len < WORDLEN) ? ((1ull << len) - 1) : ~0ull;
		if (j + len <= WORDLEN) 
			return (data[i] >> j) & mask;
		else
			return (data[i] >> j) | ((data[i + 1] << (WORDLEN - j)) & mask);
	}

	void setbits(size_t bitindex, uint64_t value, unsigned int len) {
		assert(len <= WORDLEN && len > 0);
		assert(bitindex + len <= bitlen);
		uint64_t i = bitindex / WORDLEN;
		unsigned int j = bitindex % WORDLEN; 
		uint64_t mask = (len < WORDLEN) ? ((1ull << len) - 1) : ~0ull; // & (~0ull >> (WORDLEN - len))
		value = value & mask;
		data[i] = (data[i] & ~(mask << j)) | (value << j);
		if (j + len > WORDLEN)
			data[i+1] = (data[i+1] & ~ (mask >> (WORDLEN - j))) | (value >> (WORDLEN - j));
	}

	bool bit(size_t bitindex) const {
		assert(bitindex < bitlen);
		return (data[bitindex / WORDLEN] & (1ULL << (bitindex % WORDLEN))) != 0;
	}

	bool operator[](size_t i) const { return bit(i); }

	void setbit(size_t bitindex, bool value) {
		assert(bitindex < bitlen);
		if (value) data[bitindex / WORDLEN] |= (1ULL << (bitindex % WORDLEN));
		else data[bitindex / WORDLEN] &= ~(1ULL << (bitindex % WORDLEN));
	}

	uint8_t byte(size_t pos) const {
		assert(pos*8 < bitlen);
		return ((const uint8_t*) data)[pos];
		//return 0;
	}

	uint64_t count_one() const {
		if (bitlen == 0) return 0;
		uint64_t ret = 0;
		const uint64_t wc = bitlen / WORDLEN;
		for (size_t i = 0; i < wc; i++)
			ret += popcnt(word(i));
		for (size_t i = (bitlen / WORDLEN)*WORDLEN; i < bitlen; i++)
			if (bit(i)) ret++;
		return ret;
	}

	uint64_t& word(size_t pos) { assert(pos < word_count()); return data[pos]; }
	const uint64_t& word(size_t pos) const { assert(pos < word_count()); return data[pos]; }
	size_t length() const { return bitlen; }
	size_t word_count() const { return ceildiv(bitlen, WORDLEN); }

	void fillzero() {
		std::fill(data, data + word_count(), 0ull);
	}

	void fillone() {
		std::fill(data, data + word_count(), ~0ull);
	}

//--------------------------------------------------
	BitArray(): bitlen(0), data(NULL) {}

	BitArray(size_t bit_len): bitlen(0) {
		*this = create(bit_len);
	}

	BitArray(const BitArray& other) {
		data = other.data;
		bitlen = other.bitlen;
		ptr = other.ptr;
	}

	BitArray(SharedPtr p, size_t bit_len): ptr(p), bitlen(bit_len) {
		data = (uint64_t*) ptr.get();
	}

	BitArray(uint64_t * ptr, size_t bit_len): bitlen(bit_len), data(ptr) {
		this->ptr.reset();
	}

	void clear() {
		bitlen = 0;
		data = NULL;
		ptr.reset();
	}

	static BitArray create(size_t bitlen) {
		BitArray v;
		if (bitlen == 0) return v;
		assert(bitlen > 0);
		size_t arrlen = (size_t) ceildiv(bitlen, WORDLEN);
		v.data = new uint64_t[arrlen];
		v.ptr = SharedPtr(v.data);
		v.bitlen = bitlen;
		v.data[arrlen-1] = 0;
		return v;
	}

	static BitArray create(uint64_t * ptr, size_t bitlen) {
		BitArray v = create(bitlen);
		size_t arrlen = (size_t) ceildiv(bitlen, WORDLEN);
		std::copy(ptr, ptr + arrlen, v.data);
		return v;
	}

	BitArray clone_mem() const {
		BitArray v(this->bitlen);
		std::copy(data, data + word_count(), v.data);
		return v;
	}

	~BitArray() { clear(); }

	inline static uint64_t ceildiv(uint64_t a, uint64_t b) {
		//return (a != 0 ? ((a - 1) / b) + 1 : 0);
		return (a + b - 1) / b;
	}

	IArchive& loadraw(IArchive& ar) {
		ar.var("bit_len").load(bitlen);
		ptr = ar.var("bits").load_mem(0, sizeof(uint64_t) * word_count());
		data = (uint64_t*) ptr.get();
		return ar;
	}
	
	IArchive& load(IArchive& ar) {
		ar.loadclass("Bitvector");
		loadraw(ar);
		ar.endclass();
		return ar;
	}

	OArchive& saveraw(OArchive& ar) const {
		ar.var("bit_len").save(bitlen);
		ar.var("bits").save_bin(data, sizeof(uint64_t) * word_count());
		return ar;
	}

	OArchive& save(OArchive& ar) const {
		ar.startclass("Bitvector", 1);
		saveraw(ar);
		ar.endclass();
		return ar;
	}

	std::string to_str() const {
		assert(length() < (1UL << 16));
		std::string s;
		for (unsigned int i = 0; i < bitlen; ++i)
			if (bit(i)) s += '1';
			else s += '0';
		return s;
	}

private:
	size_t bitlen;
	uint64_t * data;
private:
	SharedPtr ptr;
};

class FixedWArray {
private:
	BitArray b;
	unsigned int width;
public:
	FixedWArray(): width(0) {}
	FixedWArray(const FixedWArray& other): b(other.b), width(other.width) {}
	FixedWArray(const BitArray& bits, unsigned int width_): b(bits), width(width_) {}
	static FixedWArray create(size_t len, unsigned int width) {
		return FixedWArray(BitArray::create(len*width), width);
	}
	uint64_t operator[](size_t i) const { return b.bits(i*width, width); }
	void set(size_t i, uint64_t v) { b.setbits(i*width, v, width); }

	void fillzero() { b.fillzero(); }
	void clear() { b.clear(); width = 0; }
	IArchive& load(IArchive& ar) {
		ar.loadclass("FixedWArray");
		ar.var("bitwidth").load(width);
		b.loadraw(ar);
		ar.endclass();
		return ar;
	}
	OArchive& save(OArchive& ar) const {
		ar.startclass("FixedWArray", 1);
		ar.var("bitwidth").save(width);
		b.saveraw(ar);
		ar.endclass();
		return ar;
	}
	size_t length() const { return b.length() / width; }
	unsigned int getWidth() const { return width; }
	const BitArray getArray() const { return b; }
};


template<typename Iterator>
FixedWArray bsearch_hints(Iterator start, size_t arrlen, size_t rangelen, unsigned int lrate) {
	FixedWArray hints = FixedWArray::create((rangelen >> lrate) + 2, ceillog2(arrlen + 1));
	uint64_t i = 0, j = 0, p = 0;
	do {
		hints.set(i, p);
		//assert(j == (1 << ranklrate)*i);
		//assert(A[p] >= j);
		++i;  j += (1<<lrate);
		while (p < arrlen && *start < j) { ++p; ++start; }
	} while (j < rangelen);
	hints.set(hints.length() - 1, arrlen);
	return hints;
}

} //namespace
#endif // __BITVECTOR_H_
