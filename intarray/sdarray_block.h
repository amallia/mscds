#pragma once

#include <vector>
#include <stdint.h>
#include "bitarray/bitstream.h"
#include "bitarray/bitarray.h"
#include "blkgroup_array.h"

namespace mscds {

//template<typename BlockManager>
class SDArrayFuse;

class SDArrayBlock {
public:
	typedef uint64_t ValueType;
	typedef unsigned int IndexType;
	void add(ValueType v);
	void saveBlock(OBitStream* bits);
	void loadBlock(BitArray & ba, size_t pt);

	ValueType prefixsum(unsigned int  p) const;
	ValueType lookup(unsigned int p) const;
	ValueType lookup(unsigned int p, ValueType& prev_sum) const;
	unsigned int rank(ValueType val) const;

	void clear() { vals.clear(); bits.clear(); width = 0; select_hints = 0; blkptr = 0; }
private:
	unsigned int select_hi(uint64_t hints, uint64_t start, uint32_t off) const;
	unsigned int scan_hi_bits(uint64_t start, uint32_t res) const;
	static uint64_t getBits(uint64_t x, uint64_t beg, uint64_t num);
	unsigned int scan_hi_next(unsigned int start) const;
	unsigned int select_zerohi(uint64_t hints, uint64_t start, uint32_t off) const;
	unsigned int scan_hi_zeros(unsigned int start, uint32_t res) const;
private:
	static const unsigned int BLKSIZE = 512;
	static const unsigned int SUBB_PER_BLK = 7;
	static const unsigned int SUBB_SIZE = 74;

	std::vector<ValueType> vals;

	uint16_t width;
	uint64_t select_hints;

	size_t blkptr;
	BitArray bits;
	//template<typename>
	friend class SDArrayFuse;
};


class SDArrayFuseBuilder {
public:
	SDArrayFuseBuilder(BlockBuilder& _bd) : bd(_bd) {}
	void register_struct() {
		id = bd.register_struct(16, 8);
		cnt = 0;
		sum = 0;
		blkcnt = 0;
	}

	void add(uint64_t val) {
		blk.add(val);
		sum += val;
		cnt++;
		blkcnt++;
	}

	void finish_block() {
		if (blkcnt > 0) {
			uint64_t v = sum;
			OBitStream& d1 = bd.start_struct(id, MemRange::wrap(v));
			blk.saveBlock(&d1);
			bd.finish_struct();
			blkcnt = 0;
		}
	}

	void build() {
		finish_block();
		auto & a = bd.globalStructData();
		a.puts(cnt, 64);
		a.puts(sum, 64);
	}

	unsigned int blk_size() const { return 512; }
private:
	SDArrayBlock blk;
	unsigned int id;
	BlockBuilder & bd;
	uint64_t sum, cnt;
	int blkcnt;
};

//template<typename BlockManager>
class SDArrayFuse {
public:
	typedef BlockMemManager BlockMemManager;
	typedef SDArrayBlock BlockType;
	typedef SDArrayBlock::ValueType ValueType;

	ValueType prefixsum(unsigned int  p) const {
		uint64_t bpos = p / SDArrayBlock::BLKSIZE;
		uint32_t off = p % SDArrayBlock::BLKSIZE;
		auto sum = getSum(bpos);
		if (off == 0) return sum;
		else {
			loadBlk(bpos);
			return sum + blk.prefixsum(off);
		}
	}

	ValueType lookup(unsigned int p) const {
		uint64_t bpos = p / SDArrayBlock::BLKSIZE;
		uint32_t off = p % SDArrayBlock::BLKSIZE;
		loadBlk(bpos);
		return blk.lookup(off);
	}

	ValueType lookup(unsigned int p, ValueType& prev_sum) const {
		uint64_t bpos = p / SDArrayBlock::BLKSIZE;
		uint32_t off = p % SDArrayBlock::BLKSIZE;
		auto sum = getSum(bpos);
		loadBlk(bpos);
		auto v = blk.lookup(p, prev_sum);
		prev_sum += sum;
		return v;
	}

	unsigned int rank(ValueType val) const {
		if (val > total_sum()) return length();
		uint64_t lo = 0;
		uint64_t hi = mng.blkCount();
		while (lo < hi) {
			uint64_t mid = lo + (hi - lo) / 2;
			if (getSum(mid) < val) lo = mid + 1;
			else hi = mid;
		}
		if (lo == 0) return 0;
		lo--;
		assert(val > getSum(lo));
		assert(lo < mng.blkCount() || val <= getSum(lo + 1));
		loadBlk(lo);
		ValueType ret = lo * SDArrayBlock::BLKSIZE + blk.rank(val - getSum(lo));
		return ret;
	}

	SDArrayFuse(BlockMemManager& mng_, unsigned id_) : mng(mng_), id(id_) {}
	uint64_t length() const { return len; }

private:
	unsigned int id;
	BlockMemManager& mng;
	uint64_t len, sum;

	void load_global() {
		auto br = mng.getGlobal(id);
		len = br.bits(0, 64);
		sum = br.bits(64, 64);
	}

	uint64_t total_sum() const { return sum; }

	uint64_t getSum(size_t i) const {
		auto br = mng.getSummary(i, id);
		return br.bits(0, 64);
	}

	void loadBlk(size_t i) const {
		auto br = mng.getData(i, id);
		blk.loadBlock(*br.ba, br.start);
	}

	mutable SDArrayBlock blk;
};

}
