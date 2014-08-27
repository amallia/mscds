#pragma once

/** 
Implemented SDArray in fusion block.

*/

#include <vector>
#include <stdint.h>
#include "bitarray/bitstream.h"
#include "bitarray/bitarray.h"
#include "block_mem_mng.h"
#include "generic_struct.h"

namespace mscds {

//template<typename BlockManager>
class SDArrayFuse;

class SDArrayBlock {
public:
	typedef uint64_t ValueType;
	typedef unsigned int IndexType;
	void add(ValueType v);
	void saveBlock(OBitStream* bits);
	void loadBlock(const BitRange& br);
	void loadBlock(const BitArray & ba, size_t pt, size_t len);

	ValueType prefixsum(unsigned int  p) const;
	ValueType lookup(unsigned int p) const;
	ValueType lookup(unsigned int p, ValueType& prev_sum) const;
	unsigned int rank(ValueType val) const;

	void clear() { lastpt = ~0ULL; vals.clear(); bits.clear(); width = 0; select_hints = 0; blkptr = 0; }
	SDArrayBlock() { clear(); }
	static const unsigned int BLKSIZE = 512;
private:
	unsigned int select_hi(uint64_t hints, uint64_t start, uint32_t off) const;
	static uint64_t getBits(uint64_t x, uint64_t beg, uint64_t num);
	unsigned int select_zerohi(uint64_t hints, uint64_t start, uint32_t off) const;
private:
	static const unsigned int SUBB_PER_BLK = 7;
	static const unsigned int SUBB_SIZE = 74;

	std::vector<ValueType> vals;

	uint16_t width;
	uint64_t select_hints;

	size_t lastpt;

	size_t blkptr;
	BitArray bits;
	//template<typename>
	friend class SDArrayFuse;
};

class SDArrayFuse;

class SDArrayFuseBuilder: public InterBlockBuilderTp {
public:
	SDArrayFuseBuilder(BlockBuilder& _bd);
	SDArrayFuseBuilder();

	void init_bd(BlockBuilder& bd_);
	void register_struct();

	void add(uint64_t val);
	void add_incnum(uint64_t val);
	bool is_empty() const;
	bool is_full() const;
	void set_block_data(bool lastblock = false);
	void build_struct();
	void deploy(StructIDList& lst);

	unsigned int blk_size() const { return 512; }
private:
	uint64_t last;
	SDArrayBlock blk;
	unsigned int sid, did;
	BlockBuilder * bd;
	uint64_t sum, cnt, lastsum;
	int blkcnt;
};

//template<typename BlockManager>
class SDArrayFuse : public InterBlockQueryTp {
public:
	typedef SDArrayBlock BlockType;
	typedef SDArrayBlock::ValueType ValueType;
	SDArrayFuse(): mng(nullptr), len(0) {}
	void setup(BlockMemManager& mng_, StructIDList& lst);
	SDArrayFuse(BlockMemManager& mng_, unsigned sid_, unsigned did_);

	ValueType prefixsum(unsigned int  p) const;
	ValueType lookup(unsigned int p) const;
	ValueType lookup(unsigned int p, ValueType& prev_sum) const;
	unsigned int rank(ValueType val) const;
	uint64_t length() const { return len; }

	void clear();
	void inspect(const std::string &cmd, std::ostream &out);

	uint64_t getBlkSum(size_t blk) const;

	uint64_t total_sum() const { return sum; }
	
private:
	friend class SDArrayFuseBuilder;

	unsigned int sid, did;
	BlockMemManager* mng;
	uint64_t len, sum;

	void load_global();

	void loadBlk(size_t i) const;

	mutable SDArrayBlock blk;
	friend class SDArrayFuseHints;
	unsigned int _rank(ValueType val, unsigned int begin, unsigned int end) const;
};

}
