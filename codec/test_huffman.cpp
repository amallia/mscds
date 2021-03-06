#include "utils/utest.h"
#include <vector>
#include "huffman_code.h"
#include "huffman_adp.hpp"

namespace tests {

using namespace std;
using namespace coder;


TEST(huffman, test1) {
	const int n = 5;
	int arr[n] = {6, 5, 3, 3, 1};
	vector<unsigned int> v;
	for (int i = 0; i < n; i++) v.push_back(arr[i]);
	HuffmanCode code;
	int tt = code.build(v);
	ASSERT_EQ(tt, 6*2 + 5*2 + 3*2 + 3*3 + 1*3);
}

TEST(huffman, test2) {
	const int n = 4;
	int arr[n] = {1, 2, 1, 4};//
	vector<unsigned int> v;
	for (int i = 0; i < n; i++) v.push_back(arr[i]);
	HuffmanCode code;
	int tt = code.build(v);
	ASSERT_EQ(tt, 14);
	ASSERT_EQ(1, code[3].second);
	ASSERT_EQ(2, code[1].second);
	ASSERT_EQ(3, code[0].second);
	ASSERT_EQ(3, code[2].second);
}

TEST(huffman, encode_decode1) {
	const int n = 256;
	adp_huffman_enc enc(n);
	adp_huffman_dec dec(n);
	for (int i = 0; i < 100000; i++) {
		unsigned int v = (rand() % n);
		CodePr a = enc.encode(v);
		CodePr b = dec.decode(a.first);
		ASSERT_EQ(v, b.first);
		ASSERT_EQ(a.second, b.second);
	}
}

TEST(huffman, encode_decode2) {
	const int n = 256;
	adp_huffman_enc enc(n);
	adp_huffman_dec dec(n);
	for (int i = 0; i < 100000; i++) {
		unsigned int v = (rand() % 128);
		if (rand() % 3 < 2) v += (rand() % 128);
		CodePr a = enc.encode(v);
		CodePr b = dec.decode(a.first);
		ASSERT_EQ(v, b.first);
		ASSERT_EQ(a.second, b.second);
	}
}

TEST(huffman, canonical_code1) {
	const int n = 4;
	int arr[n] = {2, 1, 3, 3};
	vector<uint16_t> L;
	for (int i = 0; i < n; i++) L.push_back(arr[i]);
	vector<unsigned int> out = HuffmanCode::canonical_code(n, L);
	ASSERT_EQ(1, out[0]); // 11
	ASSERT_EQ(0, out[1]); // 0
	ASSERT_EQ(3, out[2]); // 101
	ASSERT_EQ(7, out[3]); // 100
}

TEST(huffman, canonical_code2) {
	const int n = 9;
	int arr[n] = {2, 3, 3, 3, 4, 4, 4, 5, 5};
	vector<uint16_t> L;
	for (int i = 0; i < n; i++) L.push_back(arr[i]);
	vector<unsigned int> out = HuffmanCode::canonical_code(n, L);
	ASSERT_EQ(0, out[0]);  // 00
	ASSERT_EQ(2, out[1]);  // 010
	ASSERT_EQ(6, out[2]);  // 011
	ASSERT_EQ(1, out[3]);  // 100
	ASSERT_EQ(5, out[4]);  // 1010
	ASSERT_EQ(13, out[5]); // 1011
	ASSERT_EQ(3, out[6]);  // 1100
	ASSERT_EQ(11, out[7]); // 11010
	ASSERT_EQ(27, out[8]); // 11011
}

TEST(huffman, canonical_code3) {
	const int n = 9;
	int arr[n] = {2, 2, 3, 3, 4, 4, 5, 5, 4};
	vector<uint16_t> L;
	for (int i = 0; i < n; i++) L.push_back(arr[i]);
	vector<unsigned int> out = HuffmanCode::canonical_code(n, L);
	ASSERT_EQ(0, out[0]);  // 00
	ASSERT_EQ(2, out[1]);  // 01
	ASSERT_EQ(1, out[2]);  // 100
	ASSERT_EQ(5, out[3]);  // 101

	ASSERT_EQ(3, out[4]);  // 1100
	ASSERT_EQ(11, out[5]); // 1101
	ASSERT_EQ(7, out[8]);  // 1110

	ASSERT_EQ(15, out[6]); // 11110
	ASSERT_EQ(31, out[7]); // 11111
}


template<typename HuffmanDec>
void test_huffdec(unsigned int n, unsigned int syncnt) {
	vector<unsigned int> vals;
	for (unsigned int i = 0; i < n; ++i) {
		vals.push_back(rand() % syncnt);
	}
	HuffmanCode hc;
	HuffmanDec dec;
	hc.build(HuffmanCode::comp_weight(syncnt, vals.begin(), vals.end()));
	dec.build(hc);
	for (unsigned int i = 0; i < n; ++i) {
		auto v = vals[i];
		auto x = hc.encode(v);
		auto y = dec.decode2(x.first);
		if (v != y.first) {
			cout << v << endl;
			cout << i << endl;
			dec.decode2(x.first);
		}
		ASSERT_EQ(v, y.first);
		ASSERT_EQ(x.second, y.second);
	}
}

TEST(huffman, fastdecode1) {
	const unsigned int SYMCNT = 128;
	unsigned int n = 10000;
	test_huffdec<HuffmanByteDec>(n, SYMCNT);
}

TEST(huffman, fastdecode2) {
	const unsigned int SYMCNT = 512;
	unsigned int n = 10000;
	test_huffdec<HuffmanByteDec>(n, SYMCNT);
}

TEST(huffman, fastdecode3) {
	for (unsigned int i = 128; i < 512; ++i) {
		test_huffdec<HuffmanByteDec>(10000, i);
	}
}

TEST(huffman, decode4) {
	for (unsigned int i = 128; i < 512; ++i) {
		test_huffdec<HuffmanTree>(10000, i);
	}
}

TEST(huffman, fastdecode4) {
	for (unsigned int i = 128; i < 512; ++i) {
		test_huffdec<HuffmanByteDec>(10000, 64);
	}
}

TEST(huffman, decode5) {
	for (unsigned int i = 128; i < 512; ++i) {
		test_huffdec<HuffmanTree>(10000, 64);
	}
}

}//namespace
