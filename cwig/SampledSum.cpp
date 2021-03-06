#include "SampledSum.h"
#include <cmath>
#include "utils/param.h"

#include "float_precision.h"

namespace app_ds {



void SampledSumBuilder::init(unsigned int sample_rate) {
	clear();
	this->sample_rate = sample_rate;
}

void SampledSumBuilder::clear() {
	psbd.clear();
	spsbd.clear();
	lastst = 0;
	psum = 0;
	sqpsum = 0;
	lastv = 0;
	svals.clear();
	ptr = &svals;
	factor = 1;
	delta = 0;
	cnt = 0;
	method = 0;
}

void SampledSumBuilder::add(unsigned int st, unsigned int ed, double val) {
	if (ed - st == 0) throw std::runtime_error("zero length range");
	if (st < lastst) throw std::runtime_error("overlapping intervals");

	svals.push_back(ValRange(st, ed, val));
	lastst = st;
}

void SampledSumBuilder::build(mscds::OutArchive& ar) {
	SampledSumQuery a;
	build(&a, NULL);
	a.save(ar);
}

void SampledSumBuilder::add_all(const std::deque<ValRange>* vals ) {
	ptr = vals;
}

void SampledSumBuilder::build(SampledSumQuery * out, NIntvQueryInt * posquery) {
	out->clear();
	out->pq = posquery;
	auto cf = Config::getInst();
	unsigned int storemed = cf->getInt("CWIG.VALUE_STORAGE", 0);
	if (storemed > 9) throw std::runtime_error("invalid method");

	if (method == 0) {
		method = cf->getInt("CWIG.INT_STORAGE", 0);
		if (method > 2) throw std::runtime_error("invalid method");
	}
	comp_transform();
	assert(method == 1 || method == 2);
	if (method == 1) vdir.init(storemed, sample_rate);
	else vrank.init(storemed, sample_rate);
	lastst = 0;
	psum = 0;
	sqpsum = 0;
	lastv = 0;
	cnt = 0;
	for (auto it = ptr->begin(); it != ptr->end(); ++it)
		addint(it->st, it->ed, ((int)(it->val * factor)) - delta);
	if (cnt % sample_rate == 0) {
		psbd.add_inc(psum);
		spsbd.add_inc(sqpsum);
	}
	out->len = ptr->size();
	
	psbd.build(&(out->psum));
	spsbd.build(&(out->psqrsum));
	if (method == 1) vdir.build(&(out->vdir));
	else vrank.build(&(out->vrank));
	out->factor = factor;
	out->delta = delta;
	out->rate = sample_rate;
	out->method = method;
	clear();

}

void SampledSumBuilder::comp_transform() {
	unsigned int pc = 0;
	for (auto it = ptr->begin(); it != ptr->end(); ++it)
		pc = std::max<unsigned int>(fprecision(it->val), pc);
	factor = 1;
	if (pc > 6) pc = 6;
	for (unsigned int i = 0; i < pc; ++i) factor *= 10;
	int64_t minr = std::numeric_limits<int64_t>::max();
	for (auto it = ptr->begin(); it != ptr->end(); ++it)
		minr = std::min<int64_t>(minr, it->val*factor);
	delta = minr; // 1 - minr
	if (method == 0) {
		if (factor == 1) method = 1;
		else method = 2;
	}
}

void SampledSumBuilder::addint(unsigned int st, unsigned int ed, unsigned int v) {
	unsigned int llen = ed - st;
	if (st < lastst) throw std::runtime_error("overlapping intervals");
	//psbd.add(llen * v);
	
	lastst = st;
	if (cnt % sample_rate == 0) {
		psbd.add_inc(psum);
		spsbd.add_inc(sqpsum);
		//
	}
	//
	if (method == 1) vdir.add(v);
	else vrank.add(v);
	lastv = v;

	psum += llen * v;
	sqpsum += llen * (v*v);
	cnt++;
}

SampledSumBuilder::SampledSumBuilder() {
	auto cf = Config::getInst();
	unsigned int blksize = cf->getInt("CWIG.VALUE_SMALLBLOCK_SIZE", 64);
	init(blksize);
}

double SampledSumQuery::access(unsigned int idx) const {
	double x = 0;
	if (method == 1) x = vdir.access(idx);
	else x = vrank.access(idx);
	return (x + delta) / (double) factor;
}


void SampledSumQuery::Enum::init(unsigned char etype) {
	if (type != etype || ex == NULL) {
		type = etype;
		if (ex != NULL) delete ex;
		if (etype == 1) ex = new PRValArr::Enum();
		else if (etype == 2) ex =  new RankValArr::Enum();
		else throw std::runtime_error("unknown type");
	}
}

void SampledSumQuery::getEnum(unsigned int idx, Enum * e) const {
	e->init(method);
	assert(e->type == method);
	e->factor = this->factor;
	e->delta = this->delta;
	if (method == 1) {
		vdir.getEnum(idx, (PRValArr::Enum*)(e->ex));
	} else if (method == 2) {
		vrank.getEnum(idx, (RankValArr::Enum*)(e->ex));
	}
}

double SampledSumQuery::sum(unsigned int idx, unsigned int lefpos) const {
	size_t r = idx % rate;
	size_t p = idx / rate;
	int64_t tlen = pq->int_psrlen(idx - r);
	int64_t cpsum = psum.prefixsum(p + 1) + tlen * delta;
	Enum g;
	if (r > 0 || lefpos > 0) {
		getEnum(p*rate, &g);
		for (size_t j = 0; j < r; ++j)
			cpsum += (g.next_int() + delta) * pq->int_len(p*rate + j);
	} 
	if (lefpos > 0) cpsum += (g.next_int() + delta) * lefpos;
	return cpsum/(double)factor;
}

double SampledSumQuery::sqrSum(unsigned int idx, unsigned int lefpos) const {
	size_t r = idx % rate;
	size_t p = idx / rate;
	int64_t tlen = pq->int_psrlen(idx - r) ;
	double sqrps = psqrsum.prefixsum(p + 1);
	sqrps += 2*delta*psum.prefixsum(p + 1);;
	sqrps += tlen * (delta * delta);

	Enum g;
	if (r > 0 || lefpos > 0) {
		getEnum(p*rate, &g);
		for (size_t j = 0; j < r; ++j) {
			int64_t v = (g.next_int() + delta);
			sqrps += v*v * pq->int_len(p*rate + j);
		}
	}
	if (lefpos > 0) {
		int64_t v = (g.next_int() + delta);
		sqrps += v * v * lefpos;
	}
	double fx = (double)factor;
	return sqrps / (fx*fx);
}

void SampledSumQuery::save(mscds::OutArchive& ar) const {
	ar.startclass("sampledsum");
	ar.var("int_method_type").save(method);
	ar.var("rate").save(rate);
	uint32_t udelta = delta;
	ar.var("delta").save(udelta);
	ar.var("factor").save(factor);
	psum.save(ar.var("psum"));
	psqrsum.save(ar.var("sqrsum"));
	if (method == 1) {
		vdir.save(ar.var("direct_values"));
	}else 
	if (method == 2) {
		vrank.save(ar.var("rank_values"));
	}else throw mscds::ioerror("wrong method range");
	ar.endclass();
}

void SampledSumQuery::load(mscds::InpArchive& ar, NIntvQueryInt * posquery) {
	this->pq = posquery;
	ar.loadclass("sampledsum");
	ar.var("int_method_type").load(method);
	ar.var("rate").load(rate);
	uint32_t udelta;
	ar.var("delta").load(udelta);
	delta = udelta;
	ar.var("factor").load(factor);
	psum.load(ar.var("psum"));
	psqrsum.load(ar.var("sqrsum"));
	if (method == 1) {
		vdir.load(ar.var("direct_values"));
	}else 
		if (method == 2) {
			vrank.load(ar.var("rank_values"));
		}else throw mscds::ioerror("wrong method range");
	ar.endclass();	
}

void SampledSumQuery::clear() {
	pq = NULL;
	psum.clear();
	psqrsum.clear();
	len = 0;
	rate = 0;
	factor = 0;
	delta = 0;
	method = 0;
}

SampledSumQuery::SampledSumQuery() {
	clear();
}

bool SampledSumQuery::Enum::hasNext() const {
	return ex->hasNext();
}

SampledSumQuery::Enum::~Enum() {
	if (ex != NULL) {
		delete ex;
		ex = NULL;
	}
}

double SampledSumQuery::Enum::next() {
	return (double)(next_int() + delta) / factor;
}

int64_t SampledSumQuery::Enum::next_int() {
	return (int64_t) ex->next();
}




}
