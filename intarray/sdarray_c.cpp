#include "sdarray_c.h"



namespace mscds {

SDArrayCompressBuilder::SDArrayCompressBuilder() { init(); }

void SDArrayCompressBuilder::init() { i = 0; sum = 0; }

void SDArrayCompressBuilder::add(unsigned int v) {
    sum += v;
    ++i;
    if (i % BLKSIZE == 0) {
        // skip the BLKSIZE-th value in the block
        //each block has (BLKSIZE-1) = 511 values
        _build_blk();
    } else {
        blk.add(v);
    }
}

void SDArrayCompressBuilder::_build_blk() {
    blk.saveBlock(&obits);
    blkpos.push_back(obits.length());
    csum.push_back(sum);
}

void SDArrayCompressBuilder::_finalize() {
    if (i % BLKSIZE != 0) {
        blk.saveBlock(&obits);
    }
    if (i > 0) {
        blkpos.push_back(obits.length());
        csum.push_back(sum);
    }
    if (!blkpos.empty()) {
        w1 = val_bit_len(csum.back());
        w2 = val_bit_len(blkpos.back());
    } else {
        w1 = w2 = 0;
    }
    assert(csum.size() == blkpos.size());
    auto it = csum.begin();
    auto jt = blkpos.begin();
    while (it != csum.end()) {
        header.puts(*it,w1);
        header.puts(*jt,w2);
        ++it;
		++jt;
    }
    w2 += w1;

    blkpos.clear();
    csum.clear();
}

void SDArrayCompressBuilder::add_inc(unsigned int s) {
    assert(s >= sum);
    add(s - sum);
}

void SDArrayCompressBuilder::build(SDArrayCompress *out) {
    _finalize();
    header.build(&out->header);
    BitArray ba;
    obits.build(&ba);
    RRR_BitArrayBuilder::build_array(ba, &(out->bits));
    out->w1 = w1;
    out->w2 = w2;
	out->sum = sum;
	out->len = i;
}

SDArrayCompress::ValueTp SDArrayCompress::_getBlkSum(unsigned blk) const {
    if (blk == 0) return 0;
    else blk-=1;
    return header.bits(w2*blk, w1);
}

SDArrayCompress::ValueTp SDArrayCompress::_getBlkStartPos(unsigned blk) const {
    if (blk == 0) return 0;
    else blk-=1;
    return header.bits(w2*blk + w1, w2-w1);
}

void SDArrayCompress::_loadBlk(unsigned bp) const {
    auto p1 = _getBlkStartPos(bp);
    auto p2 = _getBlkStartPos(bp+1);
    blk.loadBlock(&bits, p1, p2-p1);
}


SDArrayCompress::ValueTp SDArrayCompress::prefixsum(unsigned int p) const {
    if (p == this->len) return this->sum;
    uint64_t bpos = p / BLKSIZE;
    uint32_t off = p % BLKSIZE;
    auto sum = _getBlkSum(bpos);
    if (off == 0) return sum;
    else {
        _loadBlk(bpos);
        return sum + blk.prefixsum(off);
    }
}

SDArrayCompress::ValueTp SDArrayCompress::lookup(unsigned int p) const {
    uint64_t bpos = p / BLKSIZE;
    uint32_t off = p % BLKSIZE;
    if (off + 1 < BLKSIZE) {
        _loadBlk(bpos);
        return blk.lookup(off);
    }else {
        auto sum0 = _getBlkSum(bpos);
        auto sum1 = _getBlkSum(bpos + 1);
        _loadBlk(bpos);
		return sum1 - sum0 - blk.prefixsum(BLKSIZE-1);
    }
}



}//namespace 
