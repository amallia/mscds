#pragma once

#include <string>
#include "RLSum4.h"
#include "tree/RMQ_sct.h"
#include "stringarr.h"
#include "archive.h"

namespace app_ds {

struct ValRange {
	unsigned int st, ed;
	int val;
	std::string annotation;
	ValRange() {}
	ValRange(unsigned int s, unsigned int e, int v):st(s), ed(e), val(v) {}
	ValRange(unsigned int s, unsigned int e, int v, const std::string& ann):st(s), ed(e), val(v), annotation(ann) {}
	bool operator<(const ValRange& e) const { return st < e.st; }
	bool operator==(const ValRange& e) const {
		return st == e.st && ed == e.ed && val == e.val;
	}
};

typedef RunLenSumArray4 ChrNumValType;
typedef ChrNumValType::BuilderTp ChrNumValBuilderType;

class ChrNumThread;

enum minmaxop_t {NO_MINMAX= 0, MIN_OP=1, MAX_OP=2, ALL_OP=3};

class ChrNumThreadBuilder {
public:
	ChrNumThreadBuilder();
	void init(minmaxop_t option=NO_MINMAX, unsigned int factor=100, bool range_annotations = false);
	void add(unsigned int st, unsigned int ed, int val, const std::string& s = "");
	void build(mscds::OArchive& ar);
	void build(ChrNumThread* out);
private:
	std::deque<ValRange> ranges;
	bool setup_;
	unsigned int factor;
	minmaxop_t minmax_opt;
	int find_min();

	bool has_annotation;
	StringArrBuilder annbd;
};

class ChrNumThread {
public:
	/** \brief returns the i-th range's annotation (if available) */
	const std::string range_annotation(unsigned int i) const;

	/** \brief returns the position of the next non-zero value */
	unsigned int next_nz(unsigned int) const;

	/** \brief returns the position of the previous non-zero value */
	unsigned int prev_nz(unsigned int) const;

	/** \brief returns the last position */
	unsigned int last_position() const { return vals.last_position(); }

	/** \brief return the sum of the position from 0 to p */
	long long sum(size_t p) const;

	std::vector<long long> sum_batch(size_t st, size_t ed, size_t n) const;

	/** \brief counts the number of non-zero ranges that start from 0 to i (inclusive) */
	unsigned int count_range(unsigned int i) const;

	std::vector<unsigned int> count_range_batch(size_t st, size_t ed, size_t n) const;

	/** \brief counts the number of non-zero position from 0 to i */
	unsigned int count_nz(unsigned int) const;

	std::vector<unsigned int> count_nz_batch(unsigned int st, size_t ed, size_t n) const;

	/** \brief returns the minimum value in [st..ed) */
	unsigned int min_value(unsigned int st, unsigned int ed) const;

	std::vector<unsigned int> min_value_batch(unsigned int st, size_t ed, size_t n) const;

	/** \brief returns the minimum value in [st..ed) */
	unsigned int max_value(unsigned int st, unsigned int ed) const;

	std::vector<unsigned int> max_value_batch(unsigned int st, size_t ed, size_t n) const;

	void clear();
	void load(mscds::IArchive& ar);
	void save(mscds::OArchive& ar) const;
	void dump_bedgraph(std::ostream& fo) const;
	std::string name;
private:
	ChrNumValType vals;
	StringArr annotations;
	mscds::RMQ_sct min, max;

	bool has_annotation;
	minmaxop_t minmax_opt;
	unsigned int factor;
	int64_t delta;
	friend class ChrNumThreadBuilder;
};


}//namespace

namespace app_ds {

inline long long ChrNumThread::sum(size_t p) const {
	return vals.sum_delta(p, delta);
}

inline const std::string ChrNumThread::range_annotation(unsigned int i) const {
	if (has_annotation)
		return annotations.get(i);
	else return "";
}

inline unsigned int ChrNumThread::count_range(unsigned int i) const {
	return vals.count_range(i);
}

inline unsigned int ChrNumThread::min_value(unsigned int st, unsigned int ed) const {
	if (minmax_opt & MIN_OP) {
		unsigned int i = (unsigned int) min.m_idx(st, ed);
		return (int)(vals.range_value(i) + delta);
	}else return 0;
}

inline unsigned int ChrNumThread::max_value(unsigned int st, unsigned int ed) const {
	if (minmax_opt & MAX_OP) {
		unsigned int i = (unsigned int) min.m_idx(st, ed);
		return (int)(vals.range_value(i) + delta);
	}else return 0;
}

inline unsigned int ChrNumThread::next_nz(unsigned int p) const {
	return vals.next(p);
}

inline unsigned int ChrNumThread::prev_nz(unsigned int p) const {
	return vals.prev(p);
}

inline unsigned int ChrNumThread::count_nz(unsigned int p) const {
	return vals.countnz(p);
}


}//namespace
