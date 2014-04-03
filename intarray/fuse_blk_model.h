#pragma once

#include "blkgroup_array.h"
#include "bitarray/bitstream.h"
#include "bitarray/bitop.h"

#include "codec/deltacoder.h"

#include "huffarray.h"

namespace mscds {

typedef HuffmanModel Model;

//template<typename Model>
class CodeInterBlkBuilder: public InterBlockBuilderTp {
public:
	void start_model() { model.startBuild(); }
	void model_add(uint32_t val) { model.add(val); }

	void build_model() {
		model.endBuild();
		model.saveModel(&model_buffer);
	}

	void init_bd(BlockBuilder& bd_) {
		bd = &bd_;
	}

	void register_struct() {
		unsigned int bl = (model_buffer.length() + 7) / 8;
		sid = bd->register_summary(8 + bl, 1);
		did = bd->register_data_block();

		cnt = 0;
	}

	bool is_full() const {
		return cnt >= 512;
	}
	bool is_empty() const {
		return cnt == 0;
	}

	void add(uint32_t val) {
		assert(!is_full());
		if (cnt % 64 == 0)
			ptrs.push_back(data_buffer.length());
		model.encode(val, &data_buffer);
		cnt++;
	}

	void set_block_data(bool x = false) {
		if (is_empty()) return;
		//while (ptrs.size() < 9) ptrs.push_back(ptrs.back());
		unsigned char w = ceillog2(ptrs.back() + 1);
		unsigned int base = (ptrs.size())* w;
		if (base + ptrs.back() >= (1u << w)) {
			w += 1;
			base = (ptrs.size())* w;
		}
		bd->set_summary(sid, MemRange::wrap(w));
		data_buffer.close();
		OBitStream& data = bd->start_data(did);
		for (unsigned int i = 0; i < ptrs.size(); ++i) {
			data.puts(ptrs[i] + base, w);
		}
		assert(ptrs.back() + base < (1u << w));
		data.append(data_buffer);
		//debug_print();
		ptrs.clear();
		data_buffer.clear();
		bd->end_data();
		cnt = 0;
	}

	void debug_print_org(std::ostream& out = std::cout) const {
		out << "Shortcut pointers: " << '\n';
		for (auto v : ptrs)
			out << v << ", ";
		out << '\n';
		unsigned w = ceillog2(ptrs.back() + 1);
		out << "W = " << w << '\n';
		unsigned int base = (ptrs.size())* w;
		out << "Base_Shift: " << base << "\n\n";
	}

	void debug_print(std::ostream& out = std::cout) const {
		out << "Shortcut pointers: " << '\n';
		for (auto v : ptrs)
			out << v << ", ";
		out << '\n';
		unsigned w = ceillog2(ptrs.back() + 1);
		out << "W = " << w << '\n';
		unsigned int base = (ptrs.size())* w;
		out << "Base_Shift: " << base << "\n\n";

		for (unsigned int i = 1; i < ptrs.size(); ++i) {
			auto d = ptrs[i] - ptrs[i-1] - 475;
			out << d << " ";
		}
		out << "\n\n";
	}

	void build_struct() {
		if (!is_empty())
			set_block_data();
		model_buffer.puts(cnt);
		model_buffer.close();

		bd->set_global(sid, model_buffer);
		model_buffer.clear();
	}

	void deploy(StructIDList& lst) {
		lst.addId("CodeInterBlkBuilder");
		lst.add(sid);
		lst.add(did);
	}

private:
	unsigned int cnt;
	std::vector<uint32_t> ptrs;
	OBitStream data_buffer, model_buffer;
	Model model;
	BlockBuilder * bd;
	unsigned int sid, did;
};


//template<typename Model>
class CodeInterBlkQuery {
public:
	static const unsigned int elements_per_blk = 512;
	CodeInterBlkQuery(): mng(nullptr) {}
	void setup(BlockMemManager& mng_, StructIDList& lst) {
		mng = &mng_;
		lst.checkId("CodeInterBlkBuilder");
		sid = lst.get();
		did = lst.get();
		assert(sid > 0);
		assert(did > 0);
		load_model();
	}

	uint64_t get(unsigned int i) {
		Enum e;
		getEnum(i, &e);
		return e.next();
	}

	struct Enum: public EnumeratorInt<uint64_t> {
	public:
		Enum() {}
		bool hasNext() const { return pos <= data->len; }
		uint64_t next() {
			auto v = data->model.decode(&is);
			++pos;
			if (pos % elements_per_blk == 0) move_blk(pos / elements_per_blk);
			return v;
		}
	private:
		unsigned int pos;
		IWBitStream is;
		const CodeInterBlkQuery * data;
		friend class CodeInterBlkQuery;

		void move_blk(unsigned int blk) {
			if (!hasNext()) return;
			auto br = data->mng->getData(data->did, blk);
			unsigned int w = data->get_w(blk);
			unsigned int st = br.bits(0, w);
			is.init(*br.ba, br.start + st);
		}
	};

	void getEnum(unsigned int pos, Enum * e) const {
		e->data = this;
		
		unsigned int blk = pos / elements_per_blk;
		unsigned sbid = pos % elements_per_blk;
		unsigned int sblk = sbid / SSBLKSIZE;
		unsigned int px = sbid % SSBLKSIZE;
		e->pos = pos - px;

		auto br = mng->getData(did, blk);
		unsigned int w = get_w(blk);
		unsigned int st = br.bits(sblk * w, w);
		e->is.init(*br.ba, br.start + st);

		for (unsigned int i = 0; i < px; ++i)
			e->next();
	}

	/*void debug_print(unsigned int blk, unsigned sscnt = 8, std::ostream& out = std::cout) const {
		auto br = mng->getData(did, blk);
		unsigned int w = get_w(blk);
		out << "Shortcut pointers: " << '\n';
		for (unsigned sblk = 0; sblk < sscnt; ++sblk) {
			unsigned int st = br.bits(sblk * w, w);
			out << st << ", ";
		}
		out << "\n\n";
	}*/
private:
	static const unsigned int SSBLKSIZE = 64;

	unsigned char get_w(unsigned int blk) const {
		return mng->getSummary(sid, blk).byte(0);
	}

	void load_model() {
		auto br = mng->getGlobal(sid);
		IWBitStream is;
		is.init(*br.ba, br.start);
		model.loadModel(is, true);
		len = is.get();
	}
	Model model;
	uint64_t len;
	unsigned int sid, did;
	BlockMemManager* mng;
};


}//namespace
