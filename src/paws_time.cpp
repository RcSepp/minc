#include "api.h"
#include "paws_types.h"
#include "paws_pkgmgr.h"
#include <thread>
#include <chrono>
#include <iostream>

typedef std::chrono::duration<double> duration;
typedef PawsType<duration> PawsDuration;

PawsPackage PAWS_TIME("time", [](BlockExprAST* pkgScope) {
	registerType<PawsDuration>(pkgScope, "PawsDuration");

	// Define PawsDuration getters
	defineExpr(pkgScope, "$E<PawsDuration>.hours", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::hours>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.minutes", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::minutes>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.seconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::seconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.milliseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::milliseconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.microseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::microseconds>(d).count(); } );
	defineExpr(pkgScope, "$E<PawsDuration>.nanoseconds", +[](duration d) -> int { return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); } );

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

	// Define function to print measured runtime
	defineStmt2(pkgScope, "measure($E<PawsString>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const std::string& taskName = ((PawsString*)codegenExpr(params[0], parentBlock).value)->val;
			ExprAST* stmt = params[1];

			// Measure runtime of stmt
			std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
			codegenExpr(stmt, parentBlock);
			std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();

			// Print measured runtime
			std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			std::cout << taskName << ": " << diff.count() << "ms" << std::endl;
		}
	);

	// Define function to measure runtime
	defineStmt2(pkgScope, "measure $I($E<PawsString>) $S",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			const char* varName = getIdExprASTName((IdExprAST*)params[0]);
			const std::string& taskName = ((PawsString*)codegenExpr(params[1], parentBlock).value)->val;
			ExprAST* stmt = params[2];

			// Measure runtime of stmt
			std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
			codegenExpr(stmt, parentBlock);
			std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();

			// Store measured runtime as `varName`
			defineSymbol(parentBlock, varName, PawsDuration::TYPE, new PawsDuration(endTime - startTime));
		}
	);
});