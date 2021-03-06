#include "nintv_fuse.h"

namespace app_ds {

void NIntvInterBlkBuilder::register_struct() {
	bd->begin_scope("nintv");
	start.register_struct();

	lgsid = bd->register_summary(8, 8);
	lgdid = bd->register_data_block();
	bd->end_scope();

	cnt = 0;
	lensum = 0;
	laststart = 0;
}

void NIntvInterBlkBuilder::add(unsigned int st, unsigned int ed) {
	assert(st < ed);
	assert(!is_full());
	data.emplace_back(st, ed);
	cnt++;
}

bool NIntvInterBlkBuilder::is_empty() const { return cnt == 0; }

bool NIntvInterBlkBuilder::is_full() const { return cnt >= 512; }

void NIntvInterBlkBuilder::set_block_data(bool lastblock) {
	if (cnt == 0) return ;
	_build_block();

	cnt = 0;
	data.clear();
}

void NIntvInterBlkBuilder::build_struct() {
	set_block_data();
	start.build_struct();
	uint64_t v = lensum;
	bd->set_global(lgsid, mscds::ByteMemRange::ref(v));
}

void NIntvInterBlkBuilder::deploy(mscds::StructIDList& lst) {
	lst.addId("nintv_fuse");
	start.deploy(lst);
	lst.add(lgsid);
	lst.add(lgdid);
}

void NIntvInterBlkBuilder::_build_block() {
	if (data.size() == 0) return;
	for (unsigned i = 0; i < data.size(); ++i) {
		start.add(data[i].first - laststart);
		laststart = data[i].first;
	}
	start.set_block_data();
	size_t tlen = 0, tgap = 0;
	tlen = data[0].second - data[0].first;
	tgap = 0;
	for (unsigned i = 1; i < data.size(); ++i) {
		tlen += data[i].second - data[i].first;
		if (data[i].first < data[i - 1].second)
			throw std::runtime_error("require non-overlap intv");
		tgap += data[i].first - data[i - 1].second;
	}
	bool store_len = (tlen < tgap);
	_build_block_type(store_len);
	//
	data.clear();
}

void NIntvInterBlkBuilder::_build_block_type(bool store_len) {
	if (store_len) {
		for (unsigned i = 0; i < data.size(); ++i)
			lgblk.add(data[i].second - data[i].first);
	} else {
		for (unsigned i = 1; i < data.size(); ++i)
			lgblk.add(data[i].first - data[i - 1].second);
		lgblk.add(0);
	}

	uint64_t v = lensum;
	if (!store_len) v |= (1ULL << 63);
	bd->set_summary(lgsid, mscds::ByteMemRange::ref(v));
	auto& a = bd->start_data(lgdid);
	lgblk.saveBlock(&a);
	for (unsigned i = 0; i < data.size(); ++i)
		lensum += data[i].second - data[i].first;
	bd->end_data();
}

NIntvQueryInt::PosType FuseNIntvInterBlock::int_start(NIntvQueryInt::PosType i) const { return start.prefixsum(i + 1); }

NIntvQueryInt::PosType FuseNIntvInterBlock::int_end(NIntvQueryInt::PosType i) const { return int_start(i) + int_len(i); }

std::pair<NIntvQueryInt::PosType, NIntvQueryInt::PosType> FuseNIntvInterBlock::int_startend(NIntvQueryInt::PosType i) const {
	PosType st = int_start(i);
	return std::pair<PosType, PosType>(st, st + int_len(i));
}

NIntvQueryInt::PosType FuseNIntvInterBlock::int_len(NIntvQueryInt::PosType i) const {
	assert(i < length());
	PosType ip = i % BLKSIZE;
	PosType blk = i / BLKSIZE;
	LGBlk_t sumc = loadLGSum(blk);
	loadblock(blk);
	if (sumc.store_len) {
		return lgblk.lookup(ip);
	}
	if (i + 1 == length()) {
		auto dx = int_start(i) - int_start(i - ip);
		return lensum + lgblk.prefixsum(ip + 1) - sumc.sum - dx;
	}
	if (ip != BLKSIZE - 1) {
		return start.lookup(i + 1) - lgblk.lookup(ip);
	} else {
		auto nx = loadLGSum(blk + 1).sum;
		auto dx = int_start(i) - int_start(i - ip);
		return nx + lgblk.prefixsum(BLKSIZE - 1) - sumc.sum - dx;
	}
}

std::pair<NIntvQueryInt::PosType, NIntvQueryInt::PosType> FuseNIntvInterBlock::find_cover(NIntvQueryInt::PosType pos) const {
	PosType p = rank_interval(pos);
	if (p == npos()) return std::pair<PosType, PosType>(0u, 0u);
	uint64_t sp = int_start(p);
	assert(sp <= pos);
	PosType kl = pos - sp + 1;
	PosType rangelen = int_len(p);
	if (kl <= rangelen) return std::pair<PosType, PosType>(p, kl);
	else return std::pair<PosType, PosType>(p+1, 0);
}

NIntvQueryInt::PosType FuseNIntvInterBlock::coverage(NIntvQueryInt::PosType pos) const {
	uint64_t p = rank_interval(pos);
	if (p == npos()) return 0;
	uint64_t sp = int_start(p);
	assert(sp <= pos);
	uint64_t ps = int_psrlen(p);
	PosType rangelen = int_len(p);
	return std::min<PosType>((pos - sp), rangelen) + ps;
}

NIntvQueryInt::PosType FuseNIntvInterBlock::rank_interval(NIntvQueryInt::PosType pos) const {
	uint64_t p = start.rank(pos+1);
	if (p == 0) return npos();
	return p-1;
}

NIntvQueryInt::PosType FuseNIntvInterBlock::int_psrlen(NIntvQueryInt::PosType i) const {
	if (i >= length())
		return lensum;
	PosType ip = i % BLKSIZE;
	PosType blk = i / BLKSIZE;
	LGBlk_t sumt = loadLGSum(blk);
	if (i == 0) return sumt.sum;
	loadblock(blk);
	if (sumt.store_len) {
		return sumt.sum + lgblk.prefixsum(ip);
	} else {
		return int_start(i) - int_start(i - ip) + sumt.sum - lgblk.prefixsum(ip);
	}
}

FuseNIntvInterBlock::FuseNIntvInterBlock() {
	glsid = 0;
	gldid = 0;
}

void FuseNIntvInterBlock::setup(mscds::BlockMemManager &mng_, mscds::StructIDList& lst) {
	mng = &mng_;
	lst.checkId("nintv_fuse");
	start.setup(mng_, lst);
	glsid = lst.get();
	gldid = lst.get();
	assert(glsid > 0 && gldid > 0);
	loadGlobal();
}

FuseNIntvInterBlock::LGBlk_t FuseNIntvInterBlock::loadLGSum(unsigned int blk) const {
	mscds::BitRange br = mng->getSummary(glsid, blk);
	uint64_t d = br.bits(0, 64);
	LGBlk_t ret;
	ret.sum = d & ((1ull << 63) - 1);
	ret.store_len = (d >> 63) == 0;
	return ret;
}

void FuseNIntvInterBlock::loadGlobal() {
	lensum = mng->getGlobal(glsid).word(0);
}

void FuseNIntvInterBlock::clear() {
	mng = nullptr;
	glsid = 0;
	gldid = 0;
	lensum = 0;
	start.clear();
	lgblk.clear();
}

void NIntvFuseBuilder::init() {
	iblk.register_struct();
	bd.init_data();
}

void NIntvFuseBuilder::add(unsigned int st, unsigned int ed) {
	iblk.add(st, ed);
	cnt++;
	if (cnt % 512 == 0)
		_end_block();
}

void NIntvFuseBuilder::_end_block() {
	iblk.set_block_data();
	bd.end_block();
}

void NIntvFuseBuilder::build(NIntvFuseQuery *out) {
	if (cnt % 512 != 0)
		_end_block();
	iblk.build_struct();
	bd.build(&out->mng);
	mscds::StructIDList lst;
	iblk.deploy(lst);
	out->init(lst);
}

void app_ds::FuseNIntvInterBlock::loadblock(unsigned int blk) const {
	auto a = mng->getData(gldid, blk);
	lgblk.loadBlock(a);
}

void NIntvFuseQuery::init(mscds::StructIDList& lst) {
	b.setup(mng, lst);
}

void NIntvFuseQuery::save(mscds::OutArchive &ar) const {
	ar.startclass("fused_non_overlapped_intervals", 1);
	mng.save(ar.var("block_data"));
	ar.endclass();
}

void NIntvFuseQuery::load(mscds::InpArchive &ar) {
	int class_version = ar.loadclass("fused_non_overlapped_intervals");
	mng.load(ar.var("block_data"));
	ar.endclass();
	//throw std::runtime_error("not implemented");
	std::cout << "warning: dangerous assignment" << std::endl;
	mscds::StructIDList lst;
	std::vector<int> x = {-1, -1, 1, 1, 2, 2};
	for (auto v : x) lst._lst.push_back(v);
	init(lst);
}


}//namespace


