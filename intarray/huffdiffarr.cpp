
#include "huffdiffarr.h"
#include "codec/deltacoder.h"

namespace mscds {

void HuffDiffArrBuilder::build(HuffDiffArray * outds) {
	if (buf.size() > 0) {
		blk.build(&buf, rate1, &out, &opos);
		for (unsigned int i = 0; i < opos.size(); ++i) 
			bd.add(opos[i]);
		buf.clear();
		opos.clear();
	}
	out.close();
	bd.build(&(outds->ptr));
	outds->bits = BitArray::create(out.data_ptr(), out.length());
	outds->len = cnt;
	outds->rate1 = rate1;
	outds->rate2 = rate2;
}

void HuffDiffArrBuilder::build( OArchive& ar ) {
	HuffDiffArray tmp;
	tmp.save(ar);
}

void HuffDiffArrBuilder::build_huffman(HuffDiffArray * out) {

}

void HuffDiffArrBuilder::add(uint32_t val) {
	assert(rate1 > 0 && rate2 > 0);
	buf.push_back(coder::absmap(((uint64_t)val) - last));
	cnt++;
	if (buf.size() % (rate1 * rate2) == 0) {
		blk.build(&buf, rate1, &out, &opos);
		for (unsigned int i = 0; i < opos.size(); ++i) 
			bd.add(opos[i]);
		buf.clear();
		opos.clear();
	}
}

void HuffDiffArrBuilder::clear() {
	buf.clear(); bd.clear(); rate1 = 0; rate2 = 0; opos.clear();
	last = 0;
}

void HuffDiffArrBuilder::init(unsigned int rate /*= 64*/, unsigned int secondrate/*=512*/) {
	rate1 = rate; rate2 = secondrate;
	cnt = 0; last = 0;
}

HuffDiffArrBuilder::HuffDiffArrBuilder() {
	init(64, 512);
}

static const unsigned int MIN_RATE = 64;

void HuffDiffBlk::buildModel(std::vector<uint32_t> * data) {
	std::map<uint32_t, unsigned int> cnt;
	for (unsigned int i = 0; i < data->size(); ++i)
		++cnt[(*data)[i]];

	unsigned int n = data->size();

	for (auto it = cnt.begin(); it != cnt.end(); ++it) {
		if (it->second * MIN_RATE > n) {
			freq.push_back(it->first);
			freqset[it->first] = freq.size();
		}
	}
	std::vector<uint32_t> W;
	hc.clear();
	tc.clear();
	if (freq.size() == 0) return ;
	W.reserve(freq.size() + 1);
	W.push_back(0);
	unsigned int sum = 0;
	for (unsigned int i = 0; i < freq.size(); ++i) {
		auto cc = cnt[freq[i]];
		sum += cc;
		W.push_back(cc);
	}
	W[0] = n - sum;
	hc.build(W);
	tc.build(hc);
}

void HuffDiffBlk::saveModel(OBitStream * out) {
	out->puts(freq.size(), 32);
	if (freq.size() > 0) {
		for (int i = 0; i < freq.size(); ++i)
			out->puts(freq[i], 32);
		for (int i = 0; i <= freq.size(); ++i)
			out->puts(hc.codelen(i), 16);
	}
}

void HuffDiffBlk::encode(uint32_t val, OBitStream * out) {
	coder::DeltaCoder dc;
	if (freq.size() > 0) {
		auto it = freqset.find(val);
		if (it != freqset.end()) {
			auto cd = hc.encode(it->second);
			out->puts(cd);
		} else {
			out->puts(hc.encode(0));
			auto cd = dc.encode(val+1);
			out->puts(cd);
		}
	}else {
		auto cd = dc.encode(val+1);
		out->puts(cd);
	}
}

void HuffDiffBlk::build(std::vector<uint32_t> * data, unsigned int subsize, OBitStream * out, std::vector<uint32_t> * opos) {
	clear();
	buildModel(data);
	auto cpos = out->length();
	saveModel(out);
	this->subsize = subsize;
	out->puts(subsize, 32);
	out->puts(data->size(), 32);
	
	for (int i = 0; i < data->size(); ++i) {
		if (i % subsize == 0) {
			opos->push_back(out->length() - cpos);
			cpos = out->length();
		}
		uint32_t val = (*data)[i];
		encode(val, out);
	}
	if (cpos != out->length())
		opos->push_back(out->length() - cpos);
}
//------------------------------------------------
void HuffDiffBlk::loadModel(IWBitStream & is) {
	unsigned int m = is.get(32);
	freq.clear();
	freqset.clear();
	tc.clear();
	hc.clear();
	if (m > 0) {
		for (int i = 0; i < m; ++i) {
			freq.push_back(is.get(32));
			freqset[freq.back()] = freq.size();
		}
		std::vector<uint16_t> L;
		for (int i = 0; i <= m; ++i)
			L.push_back(is.get(16));
		hc.loadCode(m+1, L);
		tc.build(hc);
	}
}

void HuffDiffBlk::mload(const BitArray * enc, const SDArraySml * ptr, unsigned pos) {
	auto st = ptr->prefixsum(pos);
	IWBitStream is(enc->data_ptr(), enc->length(), st);
	loadModel(is);
	subsize = is.get(32);
	len = is.get(32);
	this->enc = enc;
	this->ptr = ptr;
	this->pos = pos;
}

uint32_t HuffDiffBlk::decode(IWBitStream * is) const {
	coder::DeltaCoder dc;
	unsigned int val = 0;
	if (freq.size() > 0) {
		auto a = tc.decode2(is->peek());
		is->skipw(a.second);
		if (a.first > 0)
			val = freq[a.first - 1];
		else {
			a = dc.decode2(is->peek());
			val = a.first - 1;
			is->skipw(a.second);
		}
	} else {
		auto a = dc.decode2(is->peek());
		val = a.first - 1;
		is->skipw(a.second);
	}
	return val;
}

uint32_t HuffDiffBlk::lookup(unsigned int i) const {
	assert(i < len);
	auto r = i % subsize;
	auto p = i / subsize;
	auto epos = ptr->prefixsum(p + 1 + pos);
	IWBitStream is(enc->data_ptr(), enc->length(), epos);
	
	unsigned int val = 0;
	for (unsigned int j = 0; j <= r; ++j) {
		val = decode(&is);
	}
	return val;
}

void HuffDiffBlk::clear() {
	enc = NULL;
	ptr = NULL;
	subsize = 0;
	len = 0;
	pos = 0;
	freq.clear();
	freqset.clear();
	hc.clear();
	tc.clear();
}

uint32_t HuffDiffArray::lookup(unsigned int i) const {
	assert(i < len);
	auto r = i % (rate1 * rate2);
	auto b = i / (rate1*rate2);
	if (curblk != b) {
		blk.mload(&bits, &ptr, i/rate1 + b);
		curblk = b;
	}
	return blk.lookup(r);
}

void HuffDiffArray::save(OArchive& ar) const {
	ar.startclass("huffman_code", 1);
	ar.var("length").save(len);
	ar.var("sample_rate").save(rate1);
	ar.var("bigblock_rate").save(rate2);
	ptr.save(ar.var("pointers"));
	bits.save(ar.var("bits"));
	ar.endclass();
}

void HuffDiffArray::load(IArchive& ar) {
	ar.loadclass("huffman_code");
	ar.var("length").load(len);
	ar.var("sample_rate").load(rate1);
	ar.var("bigblock_rate").load(rate2);
	ptr.load(ar.var("pointers"));
	bits.load(ar.var("bits"));
	ar.endclass();
	curblk = -1;
}

void HuffDiffArray::clear() {
	curblk = -1;
	bits.clear();
	ptr.clear();
	len = 0;
	rate1 = 0;
	rate2 = 0;
}



}
