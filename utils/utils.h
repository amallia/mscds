#pragma once

#ifndef __COMMON_UTILS_H_
#define __COMMON_UTILS_H_

/** \file

Collection of misc utilities that are not classified into other categories.

*/

#include <stdint.h>
#include <cassert>
#include <string>
#include <ctime>

template<typename T>
struct TypeParseTraits;

#define REGISTER_PARSE_TYPE(X) template <> struct TypeParseTraits<X> \
{ static const char* name() { return #X ; } }

/** \brief ultility functions

provides commonly used functions. There are 3 major groups of functions provided
(1) group of functions to manage time
(2) group of functions to manipulate string
(3) group of function to handle file
 */
namespace utils {
	/** \brief returns the ceiling of "a" over "b" */
	inline uint64_t ceildiv(uint64_t a, uint64_t b) {
		return (a + b - 1) / b;
	}

	/** \brief time in milli-seconds */
	uint64_t getTimeMs64();
		
	/** \brief Stopwatch class to measure running time */
	class Stopwatch {
	public:
		Stopwatch();
		void start();
		void stop();
		/** \brief return the time from the last start() call in seconds */
		double seconds();
		uint64_t start_time, stop_time;
	};
	
}//namespace


#include <stdint.h>
#include <cstdlib>

namespace utils {

#if defined(_MSC_VER)

	/* MS VC's RAND_MAX is only 32767 */
	/** returns random unsigned 32-bit integer */
	inline uint32_t rand32() {
		return (((((uint32_t)rand()) << 15) ^ ((uint32_t)rand())) << 2) | (rand() & 3);
	}
#else
	inline uint32_t rand32() {
		return (uint32_t) mrand48();
	}
#endif
	/// generates a random 64-bit integer
	inline uint64_t rand64() {
		return 
			(((uint64_t) rand32() <<  0) & 0x00000000FFFFFFFFull) | 
			(((uint64_t) rand32() << 32) & 0xFFFFFFFF00000000ull);
	}

	// fast random generator from
	// http://www.jstatsoft.org/v08/i14/paper
	// http://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c/
	// http://www.codeproject.com/Articles/9187/A-fast-equivalent-for-System-Random
	// by George Marsaglia
	/// Fast XorShift Random generator
	struct XorShiftRng {
		XorShiftRng() {
			init();
		}

		void init() {
			init96();
		}

		uint32_t operator()(void) {
			return xorshf96();
		}

		void init96() {
			x=123456789;
			y=362436069; z=521288629;
		}
		uint32_t xorshf96() { //period 2^96-1
			unsigned long t;
			x ^= x << 16;
			x ^= x >> 5;
			x ^= x << 1;
			t = x; x = y; y = z;
			z = t ^ x ^ y;
			return z;
		}

		uint32_t x, y, z, w;

		void srand(uint32_t seed) {
			init();
			x = (uint32_t)((seed * 1431655781) 
				+ (seed * 1183186591)
				+ (seed * 622729787)
				+ (seed * 338294347));
			if (x == 0) x = 1;
		}


		void init128_1() {
			x = 123456789; 
			y = 362436069; z = 521288629; w = 88675123;
		}
		void init128_2() {
			x = 123456789;
			y = 842502087, z=3579807591, w=273326509;
		}
		// for other options of [a, b, c] see the paper
		uint32_t xorshf128() {
			uint32_t t = x ^ (x << 11);
			x = y; y = z; z = w;
			return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
		}
	};

	/// Timer class
	struct Timer {
		std::clock_t last, start;
		Timer() { reset(); }

		void reset() {
			start = std::clock();
			last = start;
		}
		double current() {
			clock_t c = std::clock();
			double t = (double)(c - last)  / CLOCKS_PER_SEC;
			last = c;
			return t;
		}

		double total() {
			return (double)(std::clock() - start) / CLOCKS_PER_SEC;
		}
	};


}//namespace


#endif //__COMMON_UTILS_H_
