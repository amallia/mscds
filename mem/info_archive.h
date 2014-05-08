#pragma once

/** 
Implement some utility archives:

  Memory archive for testing

  Archive to estimate size

*/

#include "framework/archive.h"
#include "file_archive1.h"

#include <memory>
#include <sstream>

namespace mscds {

class IMemArchive;

class OMemArchive : public OFileArchive1 {
public:
	OMemArchive() : OFileArchive1() {
		ss = std::make_shared<std::stringstream>(std::ios::binary | std::ios::in | std::ios::out);
		assign_write(ss.get());
	}
private:
	std::shared_ptr<std::stringstream> ss;
	OFileArchive1 ar;
	friend class IMemArchive;
};

class IMemArchive : public IFileArchive1 {
public:
	IMemArchive(OMemArchive& in) : IFileArchive1() {
		this->ss = in.ss;
		assign_read(ss.get());
	}
private:
	std::shared_ptr<std::stringstream> ss;
};

class OSizeEstArchive: public OutArchive {
public:
	OSizeEstArchive(): pos(0) {}
	OutArchive& startclass(const std::string&, unsigned char version=1) {return *this;}
	OutArchive& endclass() {return *this;}
	OutArchive& save_bin(const void*, size_t size) { pos += size; return *this;}
	size_t opos() const {return pos;}

	OutArchive& start_mem_region(size_t size, MemoryAlignmentType = A4) { return *this; }
	OutArchive& add_mem_region(const void* ptr, size_t size)  { pos += size; return *this; }
	OutArchive& end_mem_region() { return *this; }
	void close() {}
private:
	uint64_t pos;
};

class OClassInfoArchive: public OutArchive {
public:
	OClassInfoArchive();
	~OClassInfoArchive();
	OutArchive& var(const std::string& name);
	OutArchive& var(const char* name);
	OutArchive& save_bin(const void* ptr, size_t size);
	OutArchive& startclass(const std::string& name, unsigned char version=1);
	OutArchive& endclass();
	OutArchive& start_mem_region(size_t size, MemoryAlignmentType = A4);
	OutArchive& add_mem_region(const void* ptr, size_t size);
	OutArchive& end_mem_region();
	void close();
	size_t opos() const {return pos;}
	std::string printxml();
private:
	uint64_t pos;
	bool finalized;
	void* impl;
};

class OBindArchive : public OutArchive {
public:
	OBindArchive(OutArchive& first, OutArchive& second): a1(first), a2(second) {}
	OutArchive& var(const std::string& name) { a1.var(name); a2.var(name); return *this; }
	OutArchive& var(const char* name)  { a1.var(name); a2.var(name); return *this; }
	OutArchive& startclass(const std::string& name, unsigned char version = 1) { a1.startclass(name); a2.startclass(name); return *this; }
	OutArchive& endclass() { a1.endclass(); a2.endclass(); return *this; }
	OutArchive& save_bin(const void* ptr, size_t size) { a1.save_bin(ptr, size); a2.save_bin(ptr, size); return *this; }
	size_t opos() const { return a1.opos(); }

	OutArchive& start_mem_region(size_t size, MemoryAlignmentType mt = A4) { a1.start_mem_region(size, mt);  a2.start_mem_region(size, mt);  return *this; }
	OutArchive& add_mem_region(const void* ptr, size_t size)  { a1.add_mem_region(ptr, size); a2.add_mem_region(ptr, size); return *this; }
	OutArchive& end_mem_region() { a1.end_mem_region(); a2.end_mem_region(); return *this; }
	void close() { a1.close(); a2.close(); }
private:
	OutArchive & a1;
	OutArchive & a2;
};

template<typename T>
inline size_t estimate_data_size(const T& a) {
	OSizeEstArchive ar;
	a.save(ar);
	ar.close();
	return ar.opos();
}

void save_str(OutArchive& ar, const std::string& st);
std::string load_str(InpArchive& ar);

}//namespace