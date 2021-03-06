

#include "utils/utest.h"
#include "extcodec/mem_codec.h"

#include <string>

using namespace std;
using namespace mscds;
using namespace tests;

template<typename Codec>
void testcd(const string& inp) {
	Codec cd;
	string out;
	auto sx = cd.compress(inp, &out);
	string uc;
	auto b = cd.uncompress(out, &uc);
	ASSERT_EQ(out.size(), sx);
	ASSERT_EQ(true, b);
	ASSERT_EQ(inp, uc);
}

TEST(extcodec, snappy) {
	testcd<SnappyCodec>("Hello world");
	testcd<SnappyCodec>(generate_str(10));
	testcd<SnappyCodec>(generate_str(100));
	for (unsigned int i = 0; i < 100; i++)
		testcd<SnappyCodec>(generate_str(512));
}

TEST(extcodec, zlib) {
	testcd<ZlibCodec>("Hello world");
	testcd<ZlibCodec>(generate_str(10));
	testcd<ZlibCodec>(generate_str(100));
	for (unsigned int i = 0; i < 100; i++)
		testcd<ZlibCodec>(generate_str(512));
}


int main(int argc, char* argv[]) {
	//::testing::GTEST_FLAG(filter) = "";
	::testing::InitGoogleTest(&argc, argv);
	int rs = RUN_ALL_TESTS();
	return rs;
}