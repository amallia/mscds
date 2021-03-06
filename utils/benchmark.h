#pragma once

/**  \file

Function and classes for speed benchmarking.

*/

#include <functional>
#include <chrono>
#include <list>
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include <ctime>

#include "utils/str_utils.h"


namespace tests {

/// Class to add and run benchmark
class BenchmarkRegister {
public:
	typedef std::function<void()> VoidFunc;
	static BenchmarkRegister *getInst();

	void* add(const std::string& name, VoidFunc func);

	static void run_all_bm();
private:
	BenchmarkRegister() {}
	static BenchmarkRegister* _inst;
	std::list<std::pair<std::string, VoidFunc> > _list;
};

#define BENCHMARK_SET(name) \
	void name(); \
	void * _ ## name ## _reg = ::tests::BenchmarkRegister::getInst()->add(#name, name); \
	void name()

#define BENCHMARK_SET_DISABLE(name) \
	void name()


class SharedFixtureItf {
public:
	virtual void SetUp() = 0;
	virtual void TearDown() = 0;
	virtual ~SharedFixtureItf(){}
};

/// Class for one benchmark set
template<typename SharedFixture>
class Benchmarker {
public:
	typedef void(*FuncType)(SharedFixture*);
	typedef std::function<void(SharedFixture*)> StdFuncType;

	Benchmarker() : n_samples(1), verbose(false) {}

	unsigned int n_samples;
	bool verbose;

	void add(const std::string& name, FuncType fc, unsigned int nrun = 1);
	void run_all(SharedFixture* preset = NULL) { _run_methods(results, preset); }
	void report(int baseline = -1);
	void add_remark(const std::string& remark) { remark_ = remark; }
private:
	struct FuncInfo {
		std::string name;
		FuncType func;
		unsigned int nrun;
		FuncInfo() : func(NULL), nrun(0) {}
		FuncInfo(const std::string& _name, FuncType f, unsigned int _nrun) : name(_name), func(f), nrun(_nrun) {}
	};

	std::list<FuncInfo> lst; //list of benchmark functions

	typedef std::vector<std::pair<std::string, double> > RESVector;
	RESVector results;
	std::vector<RESVector> allres;
	std::string remark_;
private:
	void _run_methods(RESVector& results, SharedFixture* preset);
	void _report_methods(const RESVector& results, int baseline = -1);
};

//--------------------------------------------------------------------


template<typename SharedFixture>
void Benchmarker<SharedFixture>::add(const std::string &name, typename Benchmarker<SharedFixture>::FuncType fc, unsigned int nrun){
	lst.emplace_back(name, fc, nrun);
}

/// Timer using ctime
struct CTimer {
	CTimer() { reset(); }
	void start() { start_time = clock(); }
	void end() { duration += clock() - start_time; }
	void reset() { duration = 0; }
	double milisec() { return duration * 1000.0 / CLOCKS_PER_SEC; }

	clock_t duration;
	clock_t start_time;
};

/// high resolution timer
struct HiResTimer {
	HiResTimer() { reset(); }
	void start() { start_time = Clock::now(); }
	void end() { duration += Clock::now() - start_time; }
	void reset() { duration = Clock::duration::zero(); }
	double milisec() { return std::chrono::duration_cast<millisecs_t>(duration).count(); }
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::duration<double, std::milli> millisecs_t;

	Clock::duration duration;
	Clock::time_point start_time;
};

template<typename SharedFixture>
void Benchmarker<SharedFixture>::_run_methods(typename Benchmarker<SharedFixture>::RESVector &results, SharedFixture* preset) {
	results.resize(lst.size());
	unsigned int idx = 0;
	for (auto& fc : lst) {
		results[idx].first = fc.name;
		idx++;
	}

	typedef std::chrono::high_resolution_clock Clock;
	//typedef CppClock Clock;
	typedef std::chrono::duration<double, std::milli> millisecs_t;

	bool has_preset = (preset != NULL);

	for (unsigned int sample = 0; sample < n_samples; ++sample) {
		SharedFixture *qfx;
		if (has_preset) qfx = preset;
		else qfx = new SharedFixture();
		unsigned int idx = 0;
		if (verbose) std::cout << "<";
		qfx->SetUp();
		for (FuncInfo& fc : lst) {
			HiResTimer tm;

			if (verbose) std::cout << fc.name << std::endl;
			unsigned int rc = fc.nrun;
			if (rc > 1) {
				tm.start();
				while (rc) {
					fc.func(qfx);
					--rc;
				}
				tm.end();
			} else {
				tm.start();
				fc.func(qfx);
				tm.end();
			}
			results[idx].second += tm.milisec() / fc.nrun;
			idx++;
		}
		qfx->TearDown();
		if (!has_preset) delete qfx;
		if (verbose) std::cout << ">";
	}
	if (verbose) std::cout << "\n";
	for (auto& r : results) {
		r.second /= (n_samples);
	}
}

template<typename SharedFixture>
void Benchmarker<SharedFixture>::report(int baseline) {
	std::locale comma_locale(std::locale(), new utils::comma_numpunct());
	std::locale oldLoc = std::cout.imbue(comma_locale);
	std::cout << std::endl;
	if (remark_.length() > 0)
		std::cout << "Remark: " << remark_ << std::endl;
	
	_report_methods(results, baseline);
	std::cout.imbue(oldLoc);
}

template<typename SharedFixture>
void Benchmarker<SharedFixture>::_report_methods(const typename Benchmarker<SharedFixture>::RESVector &results, int baseline) {
	if (baseline >= 0 && baseline < results.size()) {
		double baseval = results[baseline].second;
		std::cout << "Baseline_method : " << results[baseline].first << std::endl;
		std::cout << "Baseline_value  = " << baseval << " \t(ms)" << std::endl;
		for (auto& r : results) {
			std::cout << r.first << " \t" << r.second / baseval << " \t" << r.second << std::endl;
		}
	}
	else {
		for (auto& r : results) {
			std::cout << r.first << " \t" << r.second << std::endl;
		}
	}
}

}//namespace
