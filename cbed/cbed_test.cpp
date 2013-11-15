#include <iostream>
#include "utils/utest.h"
#include "blkcomp.h"

#include <string>
#include <vector>

using namespace std;
using namespace mscds;

TEST(compressblk, test1) {
	const int n = 1000, len = 100;
	vector<string> inp;
	for (unsigned int i = 0; i < n; ++i)
		inp.push_back(generate_str(len));
	//--------------------------------------
	BlkCompBuilder bd;
	for (unsigned int i = 0; i < n; ++i)
		bd.add(inp[i]);

	BlkCompQuery qs;
	bd.build(&qs);
	for (unsigned int i = 0; i < n; ++i) {
		ASSERT_EQ(inp[i], qs.getline(i)) << "i = " << i << endl;
	}
}

using namespace std;

int main(int argc, char* argv[]) {
	//::testing::GTEST_FLAG(filter) = "";
	::testing::InitGoogleTest(&argc, argv);
	int rs = RUN_ALL_TESTS();
	return rs;
}