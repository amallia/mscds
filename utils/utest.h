#pragma once
#ifndef __UNIT_TEST_H_
#define __UNIT_TEST_H_

/** \file

Unit testing wrapper (avoid missing of gtest library).

*/

#define PRT(x) cout<< #x <<"="<<(x) << endl

#include <iostream>
#include <type_traits>
#include <stdexcept>

#if defined(_USE_OWN_TEST_LIB_)

// http://stackoverflow.com/questions/5252375/custom-c-assert-macro
// http://stackoverflow.com/questions/37473/how-can-i-assert-without-using-abort
// http://en.wikipedia.org/wiki/Assert.h


namespace _has_insertion_operator_impl_ {
	typedef char no;
	typedef char yes[2];

	struct any_t { template<typename T> any_t( T const& ); };

	no operator<<( std::ostream const&, any_t const& );

	yes& test( std::ostream& );
	no test( no );

	template<typename T>
	struct has_insertion_operator {
		static std::ostream &s;
		static T const &t;
		static bool const value = sizeof( test(s << t) ) == sizeof( yes );
	};

	template<typename T> inline
		typename std::enable_if<has_insertion_operator<T>::value,
		void>::type
		_print_cerr(const T &t) {
		std::cerr << t << "\n";
	}
	template<typename T> inline
		typename std::enable_if<!has_insertion_operator<T>::value,
		void>::type
		_print_cerr(const T &t) {}

	template<typename T1, typename T2>
	inline void _assert_cmp(const T1& exp, const T2& val, const char* expstr, const char* valstr, const char* file, int line, const char* func) {
		if (exp != val) {
			std::cerr << "Assertion failed: " << expstr << " == " << valstr << ", file: "<<file<<", func: "<< func <<", line: "<< line << "\n";
			std::cerr << "Expected value = ";
			_print_cerr<T1>(exp);
			std::cerr << "Actual value   = ";
			_print_cerr<T2>(val);
			abort();
		}
	}
}

struct _mscds_assertion_fail_: public std::exception {
	virtual const char* what() const throw() { return "assertion_failed"; }
};

//#define _OWNTEST_THROW_EXCEPTION_

inline void _abort() {
#ifndef  _OWNTEST_THROW_EXCEPTION_
	abort();
#else
	throw _mscds_assertion_fail_();
#endif
}

#define __FAILED_MSG_(err, file, line, func) \
	((void)(std::cerr << "Assertion failed: `" << err << "`, file: "<<file<<", func: "<< func <<", line: "<< line << "\n"), _abort(), 0)

#define _ASSERT_(cond) \
	(void)( (!!(cond)) || __FAILED_MSG_(#cond, __FILE__, __LINE__, __FUNCTION__))

//#define ASSERT_EQ(exp,val) ASSERT((exp)==(val))
#define _ASSERT_EQ_(exp,val) _has_insertion_operator_impl_::_assert_cmp(exp, val, #exp, #val, __FILE__, __LINE__, __FUNCTION__)


#ifndef ASSERT

#define ASSERT _ASSERT_
#define ASSERT_EQ _ASSERT_EQ_

#endif //ASSERT

#define TEST(tcase, test) void tcase ## test ()
#define SCOPED_TRACE(msg)
#define TESTALL_MAIN() int main(int argc, char **argv) { return 0; }

#else //USE_OWN_TEST_LIB

#include <gtest/gtest.h>
// #include "ext_libs/gtest/include/gtest/gtest.h"

#define ASSERT ASSERT_TRUE
#define TESTALL_MAIN() int main(int argc, char **argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }

#endif //USE_OWN_TEST_LIB

#include <vector>
#include <string>

#include "rand_data.h"

#endif //__UNIT_TEST_H_
