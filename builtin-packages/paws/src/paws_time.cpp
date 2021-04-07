#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>

typedef std::chrono::duration<double> duration;
typedef PawsValue<duration> PawsDuration;

template<> std::string PawsDuration::Type::toString(MincObject* value) const
{
	duration input_seconds = ((PawsDuration*)value)->get();

	auto h = std::chrono::duration_cast<std::chrono::hours>(input_seconds);
	input_seconds -= h;
	auto m = std::chrono::duration_cast<std::chrono::minutes>(input_seconds);
	input_seconds -= m;
	auto s = std::chrono::duration_cast<std::chrono::seconds>(input_seconds);
	input_seconds -= s;
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(input_seconds);
	input_seconds -= ms;
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(input_seconds);
	input_seconds -= us;
	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(input_seconds);

	auto hc = h.count(), mc = m.count(), sc = s.count(), msc = ms.count(), usc = us.count(), nsc = ns.count();
	uint8_t mask = (!!hc) << 0 | (!!mc) << 1 | (!!sc) << 2 | (!!msc) << 3 | (!!usc) << 4 | (!!nsc) << 5;

	if (mask == 0x0)
		return "0 nanoseconds";

	std::stringstream ss;

	if (hc)
		ss << hc << (hc == 1 ? " hour" : " hours");

	if (mc)
	{
		if (mask & 0x1) ss << (mask & ~0x3 ? ", " : " and ");
		ss << mc << (mc == 1 ? " minute" : " minutes");
	}

	if (sc)
	{
		if (mask & 0x3) ss << (mask & ~0x7 ? ", " : " and ");
		ss << sc << (sc == 1 ? " second" : " seconds");
	}

	if (msc)
	{
		if (mask & 0x7) ss << (mask & ~0xF ? ", " : " and ");
		ss << msc << (msc == 1 ? " millisecond" : " milliseconds");
	}

	if (usc)
	{
		if (mask & 0xF) ss << (mask & ~0x1F ? ", " : " and ");
		ss << usc << (usc == 1 ? " microsecond" : " microseconds");
	}

	if (nsc)
	{
		if (mask & 0x1F) ss << (mask & ~0x3F ? ", " : " and ");
		ss << nsc << (nsc == 1 ? " nanosecond" : " nanoseconds");
	}

	//TODO: Figure out better logic on how to humanize durations
	//		i.e. "1 hour and 15 minutes" vs. "75 minutes", "892.6 milliseconds" vs. "0.8926 seconds" vs. "892 milliseconds and 600 microseconds", ...

	return ss.str();
}

MincPackage PAWS_TIME("paws.time", [](MincBlockExpr* pkgScope) {
	registerType<PawsDuration>(pkgScope, "PawsDuration");

	// Define PawsDuration getters
	defineExpr(pkgScope, "$E<PawsDuration>.hours", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::hours>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.minutes", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::minutes>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.seconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::seconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.milliseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::milliseconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.microseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::microseconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.nanoseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); } );

	//TODO: Define setters

	// Define PawsDuration operators
	defineExpr(pkgScope, "$E<PawsDuration> + $E<PawsDuration>", +[](duration d0, duration d1) -> duration { return d0 + d1; } );
	defineExpr(pkgScope, "$E<PawsDuration> - $E<PawsDuration>", +[](duration d0, duration d1) -> duration { return d0 - d1; } );
	defineExpr(pkgScope, "$E<PawsDouble> * $E<PawsDuration>", +[](double factor, duration d) -> duration { return d * factor; } );
	defineExpr(pkgScope, "$E<PawsDuration> * $E<PawsDouble>", +[](duration d, double factor) -> duration { return d * factor; } );

	// Define PawsDuration type-casts
	defineTypeCast(pkgScope, +[](duration d) -> double { return d.count(); } );
	defineTypeCast(pkgScope, +[](double seconds) -> duration { return duration(seconds); } );

	// Define sleep function
	defineStmt(pkgScope, "sleep($E<PawsDuration>)",
		+[] (duration d) {
			std::this_thread::sleep_for(d);
		}
	);
	defineStmt(pkgScope, "sleep($E<PawsInt>)",
		+[] (int seconds) {
			std::this_thread::sleep_for(std::chrono::seconds(seconds));
		}
	);
	defineStmt(pkgScope, "sleep($E<PawsDouble>)",
		+[] (double seconds) {
			std::this_thread::sleep_for(duration(seconds));
		}
	);

	// Define function to print measured runtime
	class MeasureKernel : public MincKernel
	{
		const MincStackSymbol* const stackSymbol;
	public:
		MeasureKernel(const MincStackSymbol* stackSymbol=nullptr) : stackSymbol(stackSymbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			params[0]->build(buildtime);
			params[1]->build(buildtime);
			return this;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			if (params[0]->run(runtime))
				return true;
			const std::string& taskName = ((PawsString*)runtime.result)->get();
			MincExpr* stmt = params[1];

			// Measure runtime of stmt
			std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
			if (stmt->run(runtime))
				return true;
			std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();

			// Print measured runtime
			std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			std::cout << taskName << ": " << diff.count() << "ms" << std::endl;
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("measure($E<PawsString>) $S"), new MeasureKernel());

	// Define function to measure runtime
	class MeasureKernel2 : public MincKernel
	{
		const MincStackSymbol* const stackSymbol;
	public:
		MeasureKernel2(const MincStackSymbol* stackSymbol=nullptr) : stackSymbol(stackSymbol) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& varName = ((MincIdExpr*)params[0])->name;
			params[1]->build(buildtime);
			return new MeasureKernel2(buildtime.parentBlock->allocStackSymbol(varName, PawsDuration::TYPE, PawsDuration::TYPE->size));
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, const std::vector<MincExpr*>& params)
		{
			// Measure runtime of stmt
			std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
			if (params[1]->run(runtime))
				return true;
			std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();

			// Store measured runtime as `varName`
			new(runtime.getStackSymbol(stackSymbol)) PawsDuration(endTime - startTime);
			return false;
		}
		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("measure $I $S"), new MeasureKernel2());
});