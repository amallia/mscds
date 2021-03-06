#include "blkcomp.h"
#include "utils/utils.h"

#include <stdexcept>

namespace mscds {

void LineBlockBuilder::add(const std::string &line) {
	if (line.length() > 0 && line[line.length() - 1] == '\n')
		blkmem.append(line);
	else {
		blkmem.append(line);
		blkmem.append("\n");
	}
}

void LineBlockBuilder::build() {
}

void LineBlockBuilder::clear() {
	blkmem.clear();
}

std::string LineBlock::getline(unsigned int i) const {
	return std::string(blkmem.data() + lineptr[i], lineptr[i+1] - lineptr[i] - 1);
}

size_t LineBlock::size() const { return _size; }

void LineBlock::clear() { blkmem.clear(); lineptr.clear(); _size = 0; }


void LineBlock::post_load() {
	_buildptr();
}

void LineBlock::_buildptr() {
	lineptr.clear();
	lineptr.push_back(0);
	for (unsigned int i = 0; i < blkmem.size(); ++i)
		if ('\n' == blkmem[i]) lineptr.push_back(i+1);
	if (lineptr.back() != blkmem.size())
		lineptr.push_back((unsigned int) blkmem.size()+1);
	_size = lineptr.size() - 1;
}
//--------------------------------------------------------------

void BlkCompBuilder::build(mscds::OutArchive &ar) {
	BlkCompQuery out;
	build(&out);
	out.save(ar);
}

void BlkCompBuilder::init() {
	bd.clear();
	buff.clear();
	maxblksz = 128;
	curblksz = 0;
	entcnt = 0;
}

void BlkCompBuilder::add(const std::string &line) {
	bd.add(line);
	curblksz++;
	entcnt++;
	if (curblksz == maxblksz) {
		flush_data();
		curblksz = 0;
	}
}

void BlkCompBuilder::clear() { bd.clear(); curblksz = 0; blkcnt = 0; os.clear(); buff.clear(); pbd.clear(); }

void BlkCompBuilder::flush_data() {
	if (curblksz == 0) return;
	bd.build();
	blkcnt++;
	codec.compress(bd.blkmem, &buff);
	os.puts(buff);
	buff.clear();
	pbd.add_inc(os.length());
	curblksz = 0;
	bd.clear();
}

void BlkCompBuilder::build(BlkCompQuery *data) {
	flush_data();
	data->entcnt = entcnt;
	data->maxblksz = this->maxblksz;
	pbd.build(&(data->bptr));
	os.build(&(data->bits));
	data->prepare_ptr();
	entcnt = 0;
}


//--------------------------------------------------------------

void BlkCompQuery::init() { cache_mamager.resize_capacity(32); cache.resize(32); lastblk = -1; }

BlkCompQuery::BlkCompQuery() { init(); }

void BlkCompQuery::load(mscds::InpArchive &ar) {
	ar.loadclass("BlockCompressor");
	ar.var("n_entries").load(entcnt);
	ar.var("entry_per_block").load(maxblksz);
	bptr.load(ar.var("start_block_ptrs"));
	bits.load(ar.var("compressed_data"));
	ar.endclass();
	prepare_ptr();
}

void BlkCompQuery::save(mscds::OutArchive &ar) const {
	ar.startclass("BlockCompressor");
	ar.var("n_entries").save(entcnt);
	ar.var("entry_per_block").save(maxblksz);
	bptr.save(ar.var("start_block_ptrs"));
	bits.save(ar.var("compressed_data"));
	ar.endclass();
}

void BlkCompQuery::clear() {
	maxblksz = 0;
	entcnt = 0;
	len = 0;
	bptr.clear();
	bits.clear();
}

void BlkCompQuery::getEnum(unsigned int idx, BlkCompQuery::Enum *e) const {
	unsigned int blk = idx / maxblksz;
	e->bidx = idx % maxblksz;
	e->idx = idx;
	e->cblk = blk;
	load_blk(blk, e->blkdata);
	e->parent = this;
}

bool BlkCompQuery::Enum::hasNext() const {
	return idx < parent->len;
}

std::string BlkCompQuery::Enum::next() {
	std::string ret = blkdata.getline(bidx);
	idx++;
	bidx++;
	if (bidx >= parent->maxblksz && idx < parent->len) {
		cblk++;
		parent->load_blk(cblk, this->blkdata);
		bidx = 0;
	}
	return ret;
}

std::string BlkCompQuery::getline(unsigned int i) const {
	if (i >= entcnt) throw std::runtime_error("index out of range");
	unsigned int blk = i / maxblksz;
	const LineBlock& ref = getblk(blk);
	return ref.getline(i % maxblksz);
}

const LineBlock& BlkCompQuery::getblk(unsigned int b) const {
	if (b == lastblk) return cache[lastanswer]; // read only :)
	auto r = cache_mamager.access(b);
	if (r.type != utils::LRU_Policy::FOUND_ENTRY)
		load_blk(b, cache[r.index]);
	lastblk = b;
	lastanswer = r.index;
	return cache[lastanswer];
}

void BlkCompQuery::load_blk(unsigned int blk, LineBlock& lnblk) const {
	//assert();
	unsigned int st = bptr.prefixsum(blk);
	unsigned int ed = bptr.prefixsum(blk + 1);
	//codec.uncompress_c(ptr+st, ed-st, &(lnblk.blkmem));
	//UNDONE
	throw std::runtime_error("not implemented");
	lnblk.post_load();
}

void BlkCompQuery::prepare_ptr() {
	if (utils::ceildiv(entcnt, maxblksz) != bptr.length()) {
		throw std::runtime_error("data inconsistent");
	}
	//UNDONE
	//ptr = (const char*)bits.data_ptr();
	len = bits.length() / 8;
	lastblk = -1;
}


}//namespace
