#include <iostream>
#include "minc_api.h"
#include "module.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<IModule*> PawsModule;

void importFile(BlockExprAST* parentBlock, std::string importPath)
{
	char* importRealPath = realpath(importPath.c_str(), nullptr);
	importPath = importPath.substr(std::max(importPath.rfind("/"), importPath.rfind("\\")) + 1);
	const size_t dt = importPath.rfind(".");
	if (dt != -1) importPath = importPath.substr(0, dt);

	// Parse imported source code //TODO: Implement file caching
	BlockExprAST* importBlock = parseCFile(importRealPath);

	// Codegen imported module
	const int outputDebugSymbols = ((PawsInt*)importSymbol(parentBlock, "outputDebugSymbols")->value)->get();
	IModule* importModule = createModule(importRealPath, importPath + ":main", outputDebugSymbols);
	setScopeType(importBlock, FILE_SCOPE_TYPE);
	defineSymbol(importBlock, "FILE_SCOPE", PawsBlockExprAST::TYPE, new PawsBlockExprAST(importBlock));
	codegenExpr((ExprAST*)importBlock, parentBlock);
	importModule->finalize();

	// Codegen a call to the imported module's main function
	importModule->buildRun();

	// Import imported module into importing scope
	::importBlock(parentBlock, importBlock);

	// Execute command on imported module
	const std::string& command = ((PawsString*)importSymbol(parentBlock, "command")->value)->get();
	if (command == "parse" || command == "debug")
		importModule->print(importPath + ".ll");
	if (command == "build")
	{
		std::string errstr = "";
		if (!importModule->compile(importPath + ".o", errstr))
			std::cerr << errstr;
	}
}

MincPackage PAWS_COMPILE("paws.compile", [](BlockExprAST* pkgScope) {
	registerType<PawsModule>(pkgScope, "PawsModule");

	defineExpr(pkgScope, "initCompiler()",
		+[]() -> void {
			initCompiler();
		}
	);

	defineExpr(pkgScope, "loadLibrary($E<PawsString>)",
		+[](std::string filename) -> void {
			loadLibrary(filename.c_str());
		}
	);

	defineExpr(pkgScope, "createModule($E<PawsString>, $E<PawsString>, $E<PawsInt>)",
		+[](std::string sourcePath, std::string moduleFuncName, int outputDebugSymbols) -> IModule* {
			return createModule(sourcePath, moduleFuncName, outputDebugSymbols);
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.print($E<PawsString>)",
		+[](IModule* module, std::string outputPath) -> void {
			module->print(outputPath);
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.print()",
		+[](IModule* module) -> void {
			module->print();
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.compile($E<PawsString>, $E<PawsString>)",
		+[](IModule* module, std::string outputPath, std::string errStr) -> int {
			return module->compile(outputPath, errStr);
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.run()",
		+[](IModule* module) -> int {
			return module->run();
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.buildRun()",
		+[](IModule* module) -> void {
			module->buildRun();
		}
	);

	defineExpr(pkgScope, "$E<PawsModule>.finalize()",
		+[](IModule* module) -> void {
			module->finalize();
		}
	);

	// Define import statement
	defineStmt2(pkgScope, "import $E<PawsString>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::string importPath = ((PawsString*)codegenExpr(params[0], parentBlock).value)->get();
			importFile(parentBlock, importPath);
		}
	);

	// Define library import statement
	defineStmt2(pkgScope, "import <$I.$I>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			std::string importPath = "../lib/" + std::string(getIdExprASTName((IdExprAST*)params[0])) + "." + std::string(getIdExprASTName((IdExprAST*)params[1]));
			importFile(parentBlock, importPath);
		}
	);
});