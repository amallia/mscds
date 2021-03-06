#include "remap_dt.h"

#include <algorithm>
#include <functional>

namespace mscds {

static const unsigned int MAX_REMAP = 127;

void RemapTransform::buildModel(const std::vector<uint32_t> * data, const Config* conf) {
	std::unordered_map<uint32_t, unsigned int> cnt;
	for (unsigned int i = 0; i < data->size(); ++i)
		++cnt[(*data)[i]];

	unsigned int n = data->size();

	std::vector<std::pair<unsigned int, uint32_t> > sfreq;
	for (auto it = cnt.begin(); it != cnt.end(); ++it) 
		sfreq.push_back(std::make_pair(it->second, it->first));
	std::sort(sfreq.begin(), sfreq.end(), std::greater<std::pair<unsigned int, uint32_t> >());

	std::map<uint32_t, uint32_t> remapt;
	unsigned rmsize = std::min<unsigned int>(MAX_REMAP, sfreq.size());
	for (unsigned int i = 0; i < rmsize; ++i)
		if (cnt.find(i) != cnt.end())
			remapt[i] = i;
	for (unsigned int i = 0; i < rmsize; ++i) {
		auto val = sfreq[i].second;
		if (remapt.find(val) == remapt.end())
			remapt[val] = val;
	}
	for (unsigned int i = 0; i < rmsize; ++i) {
		auto val = sfreq[i].second;
		auto it = remapt.find(val);
		if (it->second != i) {
			auto jt = remapt.find(i);
			if (jt != remapt.end()) {
				std::swap(it->second, jt->second);
			}else it->second = i;
		}
	}
	for (auto & p : remapt) {
		if (p.first != p.second) remap[p.first] = p.second;
	}
	buildRev();
}

void RemapTransform::saveModel(OBitStream * out) const {
	out->puts(remap.size(), 32);
	for (auto & p : remap) {
		out->puts(p.first, 32);
		out->puts(p.second, 32);
	}
}

void RemapTransform::loadModel( IWBitStream & is, bool decode_only /*= false*/ ) {
	unsigned int m = is.get(32);
	remap.clear();
	for (int i = 0; i < m; ++i) {
		uint32_t k = is.get(32);
		uint32_t v = is.get(32);
		remap[k] = v;
	}
	buildRev();
}

void RemapTransform::clear() {
	remap.clear();
	rev.clear();
}

uint32_t RemapTransform::map(uint32_t val) const {
	auto it = remap.find(val);
	if (it != remap.end())
		return it->second;
	else return val;
}

uint32_t RemapTransform::unmap(uint32_t val) const {
	auto it = rev.find(val);
	if (it != rev.end()) return it->second;
	else return val;
}

void RemapTransform::buildRev() {
	rev.clear();
	for (auto& p: remap) {
		rev[p.second] = p.first;
	}
}

void RemapTransform::inspect(const std::string& cmd, std::ostream& out) const {

}



}//namespace

