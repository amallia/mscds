#include <celero/Print.h>
#include <celero/Console.h>
#include <celero/BenchmarkInfo.h>
#include <celero/TestVector.h>
#include <celero/Utilities.h>

#include <iostream>

using namespace celero;

void print::StageBanner(const std::string& x)
{
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Green_Bold);
	std::cout << "[==========] " << std::endl;
	std::cout << "[ STAGE    ] ";
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
	std::cout << x << std::endl;
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Green_Bold);
	std::cout << "[==========] " << std::endl;
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
}

void print::GreenBar(const std::string& x)
{
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Green_Bold);
	std::cout << "[==========] ";
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
	std::cout << x << std::endl;
}

void print::Run(std::shared_ptr<BenchmarkInfo> x)
{
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
	std::cout << "[ RUN      ] ";
	std::cout << x->get() << std::endl;
}

void print::Auto(std::shared_ptr<BenchmarkInfo> x)
{
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Yellow);
	std::cout << "[   AUTO   ] ";
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
	std::cout << x->get() << std::endl;
}

void print::Done(std::shared_ptr<BenchmarkInfo> x)
{
	celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
	std::cout << "[     DONE ] ";
	std::cout << x->getShort() << " ";
	std::cout << " (" << (static_cast<double>(x->getRunTime()) / celero::UsPerSec) << " sec)";
	std::cout << " [" << x->getOps() << " calls in " << x->getRunTime() << " usec]" << " [" << x->getUsPerOp() << " us/call]" << " [" << x->getOpsPerSecond() << " calls/sec]" << std::endl;
}

void print::Baseline(std::shared_ptr<BenchmarkInfo> x)
{	
	auto baselineGroupName = x->getGroupName();
	if(baselineGroupName.empty() == false)
	{
		auto baselineTest = celero::TestVector::Instance().getBaseline(baselineGroupName);

		if(baselineTest != nullptr)
		{
			celero::console::SetConsoleColor(celero::console::ConsoleColor_Cyan);
			std::cout << "[ BASELINE ] ";
			std::cout << x->getShort() << " ";
			const auto baselineOps = static_cast<double>(x->getRunTime())/static_cast<double>(baselineTest->getBaselineUnit());
			std::cout << baselineOps << std::endl;
			celero::console::SetConsoleColor(celero::console::ConsoleColor_Default);
		}
	}
}
