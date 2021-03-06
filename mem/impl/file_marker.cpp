#include "file_marker.h"
#include "utils/hash_utils.h"
#include <cstring>

namespace mscds {

using utils::FNV_hash;

void FileMarker::class_start(OutArchive &out, const std::string &name, unsigned char version) {
	//uint32_t v = FNV_hash::hash24(name) | (((uint32_t)version) << 24);
	//out.save_bin((char*)&v, sizeof(v));
	uint32_t v = FNV_hash::hash32(name);
	uint16_t h = (v >> 16) ^ (v & 0xFFFF);
	out.save_bin((char*)&h, sizeof(h));
}

void FileMarker::class_end(OutArchive &out) {}

unsigned char FileMarker::check_class_start(InpArchive &inp, const std::string &name) {
	/*uint32_t hash = FNV_hash::hash24(name);
	uint32_t v;
	inp.load_bin(&v, sizeof(v));
	if ((v & 0xFFFFFF) != hash) throw ioerror(std::string("Wrong hash tag: ") + name);
	return v >> 24;*/
	uint32_t v = FNV_hash::hash32(name);
	uint16_t h = (v >> 16) ^ (v & 0xFFFF);
	uint16_t x;
	inp.load_bin(&x, sizeof(x));
	if (x != h) throw ioerror(std::string("Wrong hash tag: ") + name);
	return 0;
}

bool FileMarker::check_class_end(InpArchive &inp) {
	return true;
}

void FileMarker::file_header(FileMarker::HeaderBlock& fh) {
	memcpy(&(fh.magic), "axc2", 4);
	fh.reserve = 0;
	fh.control_ptr = 0;
}

void FileMarker::check_file_header(FileMarker::HeaderBlock &fh, size_t &data, size_t &control) {
	if (memcmp(&(fh.magic), "axc2", 4) != 0)
		throw ioerror("File header mismatch");
	control = fh.control_ptr;
	data = sizeof(fh);
}

void FileMarker::control_start(OutArchive &o) {
	o.save_bin("ctrl", 4);
}

void FileMarker::check_control_start(InpArchive &inp) {
	char buff[5];
	inp.load_bin(buff, 4);
	buff[4] = '\0';
	if (strcmp(buff, "ctrl") != 0)  throw ioerror("Control block mismatch");
}

void FileMarker::mem_start(OutArchive &out, MemoryAlignmentType align) {
	uint32_t header = 0x92492400u | align;
	out.save_bin((char*)(&header), sizeof(header));
	//uint32_t nsz = (uint32_t)size;
	//out.save_bin((char*)(&nsz), sizeof(nsz));
}


void FileMarker::check_mem_start(InpArchive &inp, MemoryAlignmentType &t) {
	uint32_t header=0;// = 0x92492400u | align;
	inp.load_bin((char*)(&header), sizeof(header));

	if ((header >> 8) != 0x924924)
		throw ioerror("Wrong mem_region start or corrupted data");
	MemoryAlignmentType align = (MemoryAlignmentType) (header & 0xFF);
	uint32_t nsz = 0;
	//inp.load_bin((char*)(&nsz), sizeof(nsz));
	//return nsz;
}




}
