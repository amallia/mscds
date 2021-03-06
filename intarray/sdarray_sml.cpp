#include "sdarray_sml.h"
#include "bitarray/bitop.h"

#include <cassert>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace mscds {

#define CACHE_SELECT_RANK

SDArraySmlBuilder::SDArraySmlBuilder() {
	vals.reserve(BLKSIZE);
	cnt = 0;
	p_sum = 0;
	pslast = 0;
}

void SDArraySmlBuilder::add(uint64_t val) {
	cnt++;
	vals.push_back(val);
	if (vals.size() == BLKSIZE)
		build_blk();
	pslast += val;
}

uint64_t SDArraySmlBuilder::current_sum() {
	return pslast;
}

void SDArraySmlBuilder::add_inc(uint64_t pos) {
	assert(pos >= pslast);
	add(pos - pslast);
}

void SDArraySmlBuilder::build(SDArraySml* out) {
	build_blk();
	bits.close();
	out->len = cnt;
	out->sum = p_sum;
	bits.build(&out->bits);
	if (cnt > 0) out->table = BitArrayBuilder::create(table.size() * 64, (char*) (table.data()));
	else out->table = BitArrayBuilder::create(0);
	cnt = 0;
	p_sum = 0;
	pslast = 0;
	clear();
}

void SDArraySmlBuilder::build(OutArchive& ar) {
	SDArraySml sda;
	build(&sda);
	sda.save(ar);
	sda.clear();
}

const uint64_t SDArraySmlBuilder::BLKSIZE = 512;
const uint16_t SDArraySmlBuilder::SUBB_PER_BLK = 7;
const uint64_t SDArraySml::BLKSIZE = 512;
const uint64_t SDArraySml::SUBB_SIZE = 74;//=(ceil(BLKSIZE/SUBB_PER_BLK))

void SDArraySmlBuilder::clear() {
	cnt = 0;
	p_sum = 0;
	pslast = 0;
	vals.clear();
	table.clear();
	bits.clear();
}

void SDArraySmlBuilder::build_blk(){
	assert(vals.size() <= BLKSIZE);
	if (vals.size() == 0) return;
	while (vals.size() < BLKSIZE) vals.push_back(0);

	for (size_t p = 1; p < vals.size(); ++p)
		vals[p] += vals[p - 1];

	uint64_t begPos  = bits.length();
	table.push_back(p_sum);
	p_sum += vals.back();

	// blkinfo
	assert(begPos < (1ULL << 50));
	uint64_t blkinfo = (uint64_t)begPos;

	//lower bits
	uint64_t width = ceillog2(1 + vals.back()/vals.size());
	assert(width < (1ULL << 7));

	for (size_t p = 0; p < vals.size(); ++p)
		bits.puts(vals[p], width); /* will be masked inside */
	blkinfo |= (width << 57);

	//higher bits' hints
	uint64_t select_hints = 0;
	const unsigned int step = (BLKSIZE + SUBB_PER_BLK - 1) / SUBB_PER_BLK;
	size_t i = step;
	for (size_t p = 0; p < SUBB_PER_BLK - 1; ++p) {
		uint64_t hp = ((vals[i-1] >> width) + i-1);
		assert(hp <= 1024);
		select_hints |= (hp << (p*10));
		assert(p*10 <= 64);
		i += step;
	}

	//higer bits
	size_t j = 0;
	for (size_t p = 0; p < vals.size(); p++) {
		size_t pos = (vals[p] >> width) + p;
		while (j < pos) { bits.put0(); ++j; }
		bits.put1(); ++j;
	}
	table.push_back(blkinfo);
	table.push_back(select_hints);
	vals.clear();
}

//----------------------------------------------------------------------------

struct BlkHintInfo {
	uint64_t hints;
	BlkHintInfo(){}
	BlkHintInfo(uint64_t v):hints(v){}
	uint32_t getHints(uint32_t p) const {
		assert(p < 6);
		if (p == 0) return 0;
		return getBits((p-1)*10, 10);
	}

	uint64_t getBits(uint64_t beg, uint64_t num) const {
		return (hints >> beg) & ((1ULL << num) - 1);
	}
};

uint64_t SDArraySml::getBits(uint64_t x, uint64_t beg, uint64_t num) {
	return (x >> beg) & ((1ULL << num) - 1);
}

uint64_t SDArraySml::prefixsum(size_t p) const {
	if (p >= len) return this->sum;
	uint64_t bpos = p / BLKSIZE;
	uint32_t off  = p % BLKSIZE;
	uint64_t sum  = table.word(bpos * 3);
	if (off == 0) return sum;
	uint64_t info   = table.word(bpos * 3 + 1);
	uint64_t blkptr = info & 0x01FFFFFFFFFFFFFFull;
	uint32_t width  = info >> 57;
	uint64_t lo = (width > 0) ? bits.bits(blkptr + width * (off - 1), width) : 0;
	uint64_t hi = select_hi(table.word(bpos * 3 + 2), blkptr + width*BLKSIZE, off - 1) + 1 - off;
	return sum + ((hi << width) | lo);
}

uint64_t SDArraySml::select_hi(uint64_t hints, uint64_t start, uint32_t off) const {
	uint64_t subblkpos = off / SUBB_SIZE;
	uint32_t res       = off % SUBB_SIZE;
	if (res == SUBB_SIZE - 1)
		return getBits(hints, subblkpos*10, 10);
	uint64_t gb = subblkpos > 0 ? getBits(hints, (subblkpos-1)*10, 10) + 1 : 0;
	return bits.scan_bits(start + gb, res) + gb;
}

uint64_t SDArraySml::lookup(uint64_t p) const {
	assert(p < length());
	uint64_t bpos = p / BLKSIZE;
	uint32_t off  = p % BLKSIZE;
	uint64_t info   = table.word(bpos * 3 + 1);
	uint64_t blkptr = info & 0x01FFFFFFFFFFFFFFull;
	uint32_t width  = info >> 57;
	uint64_t prev = 0;
	int64_t prehi = 0;
	if (off > 0) {
		uint64_t prelo = bits.bits(blkptr + width * (off - 1), width);
		prehi = select_hi(table.word(bpos * 3 + 2), blkptr + width*BLKSIZE, off - 1) + 1 - off;
		prev = ((prehi << width) | prelo);
	}
	uint64_t lo = bits.bits(blkptr + width * off, width);
	//uint64_t hi = prehi + scan_hi_bits(blkptr + width*BLKSIZE + prehi + off, 0);
	uint64_t hi = prehi + bits.scan_next(blkptr + width*BLKSIZE + prehi + off);
	uint64_t cur = ((hi << width) | lo);
	return cur - prev;
	//return sum + prev;
}

uint64_t SDArraySml::lookup(uint64_t p, uint64_t& prev_sum) const {
	assert(p < length());
	uint64_t bpos = p / BLKSIZE;
	uint32_t off  = p % BLKSIZE;
	uint64_t info   = table.word(bpos * 3 + 1);
	uint64_t blkptr = info & 0x01FFFFFFFFFFFFFFull;
	uint32_t width  = info >> 57;
	uint64_t prev = 0;
	int64_t prehi = 0;
	if (off > 0) {
		uint64_t prelo = bits.bits(blkptr + width * (off - 1), width);
		prehi = select_hi(table.word(bpos * 3 + 2), blkptr + width*BLKSIZE, off - 1) + 1 - off;
		prev = ((prehi << width) | prelo);
	}
	uint64_t lo = bits.bits(blkptr + width * off, width);
	//uint64_t hi = prehi + scan_hi_bits(blkptr + width*BLKSIZE + prehi + off, 0);
	uint64_t hi = prehi + bits.scan_next(blkptr + width*BLKSIZE + prehi + off);
	uint64_t cur = ((hi << width) | lo);
	uint64_t sum  = table.word(bpos * 3);
	prev_sum = sum + prev;
	return cur - prev;
}

uint64_t SDArraySml::rank2(uint64_t p, uint64_t& select) const {
	uint64_t v = rank(p);
	//TODO: optimize this
	select = prefixsum(v);
	return v;
}


//number of 1 that is less than val
uint64_t SDArraySml::rank(uint64_t val) const {
	if (val > sum) return len;
	uint64_t lo = 0;
	uint64_t hi = table.word_count() / 3;
	while (lo < hi) {
		uint64_t mid = lo + (hi - lo) / 2;
		if (table.word(mid*3) < val) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return 0;
	lo--;
	assert(val > table.word(lo*3));
	assert(lo < table.word_count()/3 || val <= table.word((lo+1)*3));
	uint64_t ret = lo * BLKSIZE + rankBlk(lo, val - table.word(lo*3));
	return ret;
}

uint64_t SDArraySml::rank(uint64_t val, uint64_t lo, uint64_t hi) const {
	assert(lo <= hi);
	assert(hi <= table.word_count() / 3);
	if (val > sum) return len;
	while (lo < hi) {
		uint64_t mid = lo + (hi - lo) / 2;
		if (table.word(mid*3) < val) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return 0;
	lo--;
	assert(val > table.word(lo*3));
	assert(lo < table.word_count()/3 || val <= table.word((lo+1)*3));
	uint64_t ret = lo * BLKSIZE + rankBlk(lo, val - table.word(lo*3));
	return ret;
}

uint64_t SDArraySml::rankBlk(uint64_t blk, uint64_t val) const {
	uint64_t info   = table.word(blk * 3 + 1);
	uint64_t blkptr = info & 0x01FFFFFFFFFFFFFFull;
	uint32_t width  = info >> 57;
	
	uint64_t vlo = val & ((1ull << width) - 1);
	uint64_t vhi = val >> width;
	uint32_t hipos = 0, rank = 0;
	if (vhi > 0) {
		hipos = select_zerohi(table.word(blk * 3 + 2), blkptr + width*BLKSIZE, vhi-1)+1;
		//assert(scan_zerohi_bitslow(blkptr + width*BLKSIZE, vhi-1) + 1 == hipos);
		rank = hipos - vhi;
	}
	uint64_t curlo = 0;
	while (rank < BLKSIZE && bits.bit(blkptr + width*BLKSIZE + hipos)) {
		curlo =  bits.bits(blkptr + width * rank, width);
		if (curlo >= vlo)
			return rank+1;
		++rank;
		++hipos;
	}
	return rank+1;
}

uint64_t SDArraySml::select_zerohi(uint64_t hints, uint64_t start, uint32_t off) const {
	uint64_t sblk = 0;
	for (; sblk < 6; ++sblk) {
		uint64_t sbpos = getBits(hints, sblk*10, 10);
		if (sbpos - (sblk+1) * SUBB_SIZE + 1 >= off) break;
	}
	uint64_t res = off, sbpos = 0;
	if (sblk > 0) {
		sbpos = getBits(hints, (sblk-1)*10, 10) + 1;
		res -= sbpos - sblk * SUBB_SIZE; 
	}
	return sbpos + bits.scan_zeros(start + sbpos, res);
}

void SDArraySml::dump_text(std::ostream& fo) const {
	//fo << "#sd_array\n";
	fo << length() << ' ';
	for (size_t i = 0; i < length(); ++i)
		fo << lookup(i) << ' ';
	fo << '\n';
}

std::string SDArraySml::to_str(bool psum) const {
	std::ostringstream ss;
	if (psum) {
		ss << '<';
		if (length() > 0)
			ss << prefixsum(1);
		for (unsigned int i = 2; i <= length(); ++i)
			ss << ',' << prefixsum(i);
		ss << '>';
	}else {
		ss << '{';
		if (length() > 0)
			ss << lookup(0);
		for (unsigned int i = 1; i < length(); ++i)
			ss << ',' << lookup(i);
		ss << '}';
	}
	return ss.str();
}


void SDArraySml::clear() {
	sum = 0;
	len = 0;
	bits.clear();
	table.clear();
}

void SDArraySml::save(OutArchive& ar) const {
	ar.startclass("SDArraySml", 1);
	ar.var("length").save(len);
	ar.var("sum").save(sum);
	bits.save(ar.var("bits"));
	table.save(ar.var("table"));
	ar.endclass();
}

void SDArraySml::load(InpArchive& ar) {
	ar.loadclass("SDArraySml");
	ar.var("length").load(len);
	ar.var("sum").load(sum);
	bits.load(ar.var("bits"));
	table.load(ar.var("table"));
	ar.endclass();
}

SDArraySml::PSEnum::PSEnum(const SDArraySml * p, uint64_t blk): ptr(p) {
	moveblk(blk);
}

void SDArraySml::PSEnum::moveblk(uint64_t blk) {
	basesum = ptr->table.word(blk*3);
	uint64_t info   = ptr->table.word(blk * 3 + 1);
	loptr = info & 0x01FFFFFFFFFFFFFFull;
	blkwidth  = info >> 57;
	baseptr = loptr + blkwidth*BLKSIZE;
	hiptr = 0;
	idx = blk * BLKSIZE;
}

bool SDArraySml::PSEnum::hasNext() const {
	return idx < ptr->length();
}

uint64_t SDArraySml::PSEnum::next() {
	//uint64_t d = ptr->scan_hi_bits(baseptr + hiptr, 0);
	uint64_t d = ptr->bits.scan_next(baseptr + hiptr);
	hiptr += d;
	// extract here
	uint64_t lo = ptr->bits.bits(loptr, blkwidth);
	uint64_t hi = (hiptr - (idx % BLKSIZE));
	uint64_t val = basesum + lo + (hi << blkwidth);

	hiptr += 1;
	loptr += blkwidth;
	idx++;
	if (idx % BLKSIZE == 0 && hasNext()) {
		moveblk(idx / BLKSIZE);
	}
	return val;
}

void SDArraySml::getPSEnum(size_t idx, PSEnum * e) const {
	e->ptr = this;
	e->moveblk(idx / BLKSIZE);
	//Enum e(this, idx / BLKSIZE);
	uint64_t r = idx % BLKSIZE;
	for (size_t i = 0; i < r; ++i)
		e->next();
}

void SDArraySml::getEnum(size_t idx, Enum* e) const {
	if (idx > 0) {
		getPSEnum(idx-1,&(e->e));		
		uint64_t last = e->e.next();
		e->last = last;
	} else {
		getPSEnum(0, &(e->e));
		e->last = 0;
	}
}

void SDArraySml::inspect(const std::string& cmd, std::ostream& out) const {
	if (cmd == "comp_size") {
		out << "sdarray_block_512" << std::endl;
		out << "length: " << len << std::endl;
		out << "sum: " << sum << std::endl;
		out << "header_size_(overhead): " << (table.length() + 7) / 8 << std::endl;
		auto blkcnt = table.word_count()/ 3;
		size_t lower_size = 0;
		for (size_t i = 0; i < blkcnt; ++i) {
			uint64_t info = table.word(i * 3 + 1);
			uint32_t width = info >> 57;
			lower_size += width * BLKSIZE;
		}
		size_t upper_size = bits.length() - lower_size;
		out << "upper_size: " << (upper_size + 7) / 8 << std::endl;
		out << "lower_size: " << (lower_size + 7) / 8 << std::endl;
	}
}

//---------------------------------------------------------------------------------------

void SDRankSelectSml::build(const std::vector<uint64_t>& inc_pos) {
	clear();
	bool b = std::is_sorted(inc_pos.begin(), inc_pos.end());
	if (!b) throw std::logic_error("required sorted array");
	for (size_t i = 1; i < inc_pos.size(); i++) 
		if (inc_pos[i] == inc_pos[i-1]) throw std::logic_error("required non-duplicated elements");
	if (inc_pos.size() == 0) return;
	SDArraySmlBuilder bd;
	if (inc_pos[0] == 0) bd.add(0);
	else bd.add(inc_pos[0]);
	for (size_t i = 1; i < inc_pos.size(); i++) 
		bd.add(inc_pos[i] - inc_pos[i-1]);
	bd.build(&qs);
	initrank();
}

void SDRankSelectSml::build(const std::vector<unsigned int>& inc_pos) {
	clear();
	bool b = std::is_sorted(inc_pos.begin(), inc_pos.end());
	if (!b) throw std::logic_error("required sorted array");
	for (size_t i = 1; i < inc_pos.size(); i++) 
		if (inc_pos[i] == inc_pos[i-1]) throw std::logic_error("required non-duplicated elements");
	if (inc_pos.size() == 0) return;
	SDArraySmlBuilder bd;
	if (inc_pos[0] == 0) bd.add(0);
	else bd.add(inc_pos[0]);
	for (size_t i = 1; i < inc_pos.size(); i++) 
		bd.add(inc_pos[i] - inc_pos[i-1]);
	bd.build(&qs);
	initrank();
}

void SDRankSelectSml::build(BitArray& ba) {
	clear();
	SDArraySmlBuilder bd;
	uint64_t last = 0;
	for (size_t i = 0; i < ba.length(); i++)
		if (ba[i]) {
			bd.add(i-last);
			last = i;
		}
	bd.build(&qs);
	initrank();
}

struct SDASIIterator {
	const SDArraySml& q;
	uint64_t p;
	SDASIIterator(const SDArraySml& _q): q(_q), p(0) {}
	uint64_t operator*() const { return q.table.word(3*p); }
	void operator++() { ++p; }
};

void SDRankSelectSml::initrank() {
	if (qs.length() == 0) return;
	ranklrate = ceillog2(qs.total() / qs.length() + 1) + 7;
	SDASIIterator it(qs);
	rankhints = bsearch_hints(it, qs.table.word_count() / 3, qs.total(), ranklrate);
}

uint64_t SDRankSelectSml::rank(uint64_t p) const {
	if (p == 0) return 0;
	if (p > qs.total()) return qs.length();
	uint64_t i = rankhints[p>>ranklrate], j = rankhints[(p>>ranklrate)+1];

	/*uint64_t kt = qs.find(p);
	kt = (qs.prefixsum(kt) != p) ? kt : kt - 1;*/

	uint64_t k = qs.rank(p, i, j);
	//assert(k == kt);
	if (k == 0) return 0;
	else return k - 1;
}

bool SDRankSelectSml::bit(uint64_t p) const {
	uint64_t r = rank(p);
	return select(r) == p;
}

void SDRankSelectSml::load(InpArchive& ar) {
	ar.loadclass("sd_rank_select_sml");
	qs.load(ar);
	ar.load(ranklrate);
	rankhints.load(ar);
	ar.endclass();
}

void SDRankSelectSml::save(OutArchive& ar) const {
	ar.startclass("sd_rank_select_sml", 1);
	qs.save(ar.var("sdarray"));
	ar.var("log_sample_rate").save(ranklrate);
	rankhints.save(ar.var("rank_hints"));
	ar.endclass();
}

std::string SDRankSelectSml::to_str() const {
	std::ostringstream ss;
	ss << '{';
	if (qs.length() > 0) {
		ss << qs.prefixsum(1);
		for (size_t i = 2; i <= qs.length(); i++) 
			ss << ',' << qs.prefixsum(i);
	}
	ss << '}';
	return ss.str();
}


}//namespace
