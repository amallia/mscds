
#include "gamma_arr.h"


namespace mscds {

void GammaArrayBuilder::add(uint64_t val) {
	auto c = coder::GammaCoder::encode_raw(val + 1);
	upper.add(c.second);
	lower.puts(c.first, c.second);
}

void GammaArrayBuilder::build(GammaArray * out) {
	lower.close();
	upper.build(&out->upper);
	lower.build(&out->lower);
}

void GammaArrayBuilder::build(OutArchive& ar) {
	GammaArray temp;
	build(&temp);
	temp.save(ar);
}

uint64_t GammaArray::lookup(uint64_t p) const {
	uint64_t ps = 0;
	uint16_t len = upper.lookup(p, ps);
	uint64_t val = lower.bits(ps, len);
	return (val | (1ULL << len)) - 1;
}

void GammaArray::save(OutArchive& ar) const {
	ar.startclass("gamma_code", 1);
	upper.save(ar.var("upper"));
	lower.save(ar.var("lower"));
	ar.endclass();
}

void GammaArray::load(InpArchive& ar) {
	ar.loadclass("gamma_code");
	upper.load(ar.var("upper"));
	lower.load(ar.var("lower"));
	ar.endclass();
}

void GammaArray::clear() {
	upper.clear(); lower.clear();
}

uint64_t GammaArray::length() const {
	return upper.length();
}

void GammaArray::getEnum(unsigned int pos, Enum * e) const {
	e->lpos = upper.prefixsum(pos);
	upper.getEnum(pos, &(e->e));
	e->lower = &lower;
}

void GammaArray::inspect(const std::string& cmd, std::ostream& out) const
{

}


uint64_t GammaArray::Enum::next()
{
	unsigned int len = e.next();
	uint64_t val = lower->bits(lpos, len);
	lpos += len;
	return (val | (1ULL << len)) - 1;
}

bool GammaArray::Enum::hasNext() const {
	return e.hasNext();
}


}
