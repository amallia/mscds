#include "chrfmt.h"
#include "mem/filearchive.h"
#include <stdexcept>
using namespace std;
using namespace mscds;

namespace app_ds {

ChrNumThreadBuilder::ChrNumThreadBuilder():setup_(false){}

void ChrNumThreadBuilder::init(minmaxop_t option, unsigned int factor) {
	this->factor = factor;
	this->minmax_opt = option;
	setup_ = true;
}

void ChrNumThreadBuilder::add(unsigned int st, unsigned int ed, int val) {
	if (!setup_) throw std::runtime_error("need setup");
	if (val == 0) throw std::runtime_error("zero value");
	ranges.push_back(ValRange(st, ed, val));
}

void ChrNumThreadBuilder::build(mscds::OArchive& ar) {
	ChrNumThread t;
	build(&t);
	t.save(ar);
}

int ChrNumThreadBuilder::find_min() {
	int minv = std::numeric_limits<int>::min();
	for (auto it = ranges.begin(); it != ranges.end(); ++it)
		if (minv > it->val)
			minv = it->val;
	return minv;
}

void ChrNumThreadBuilder::build(ChrNumThread* out) {
	out->clear();
	if(!is_sorted(ranges.begin(), ranges.end()))
		std::sort(ranges.begin(), ranges.end());
	out->factor = factor;
	int64_t delta = 1 - find_min();	
	out->delta = -delta;
	out->minmax_opt = minmax_opt;
	std::vector<int> minmaxr(ranges.size());
	RunLenSumArrayBuilder bd;
	for (auto it = ranges.begin(); it != ranges.end(); ++it) {
		bd.add(it->st, it->ed, (unsigned int) (it->val + delta));
		minmaxr.push_back(it->val);
	}
	bd.build(&out->vals);
	ranges.clear();

	if (out->minmax_opt & MIN_OP != 0) {
		BitArray b = build_supercartisian_tree(true, minmaxr.begin(), minmaxr.end());
		out->min.build(b, 256);
	}
	if (out->minmax_opt & MAX_OP != 0) {
		BitArray b = build_supercartisian_tree(false, minmaxr.begin(), minmaxr.end());
		out->max.build(b, 256);
	}
	setup_ = false;
}

//-----------------------------------------------------------------------------

int64_t ChrNumThread::sum(size_t p) const {
	return vals.sum_delta(p, delta);
}

unsigned int ChrNumThread::min_value(unsigned int st, unsigned int ed) const {
	if (minmax_opt & MIN_OP) {
		unsigned int i = min.m_idx(st, ed);
		return (int)(vals.range_value(i) + delta);
	}else return 0;
}

unsigned int ChrNumThread::max_value(unsigned int st, unsigned int ed) const {
	if (minmax_opt & MAX_OP) {
		unsigned int i = min.m_idx(st, ed);
		return (int)(vals.range_value(i) + delta);
	}else return 0;
}

void ChrNumThread::clear() {
	vals.clear();
	min.clear();
	max.clear();
	name.clear();
}

void ChrNumThread::load(mscds::IArchive& ar) {
	clear();
	ar.loadclass("chromosome_number_thread");
	name = load_str(ar.var("chr_name"));
	ar.var("factor").load(factor);
	ar.var("delta").load(delta);
	uint32_t o = minmax_opt;
	ar.var("minmax_opt").load(o);
	minmax_opt = (minmaxop_t) o;
	vals.load(ar.var("values"));
	min.load(ar.var("min"));
	min.load(ar.var("max"));
	ar.endclass();
}

void ChrNumThread::save(mscds::OArchive& ar) const {
	ar.startclass("chromosome_number_thread", 1);
	save_str(ar.var("chr_name"), name);
	ar.var("factor").save(factor);
	ar.var("delta").save(delta);
	uint32_t o = minmax_opt;
	ar.var("minmax_opt").save(o);
	vals.save(ar.var("values"));
	min.save(ar.var("min"));
	min.save(ar.var("max"));
	ar.endclass();
}

void ChrNumThread::dump_bedgraph(std::ostream& fo) const {
	for (size_t i = 0; i < vals.length(); ++i) {
		unsigned int st = vals.range_start(i);
		unsigned int ed = st + vals.range_len(i);
		int val = vals.range_value(i);
		fo << name << " " << st
			<< ed << " " << (val + delta) << '\n';
	}
}

}//namespace