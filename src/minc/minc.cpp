// STD
#include <chrono>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdio.h>

// Local includes
#include "minc_api.hpp"
#include "minc_cli.h"
#include "minc_dbg.h"
#include "minc_pkgmgr.h"

int ARGC;
char **ARGV;
void getCommandLineArgs(int* argc, char*** argv)
{
	*argc = ARGC;
	*argv = ARGV;
}
void setCommandLineArgs(int argc, char** argv)
{
	ARGC = argc;
	ARGV = argv;
}

void quit(int code)
{
	throw ExitException(code);
}

int main(int argc, char** argv)
{
	const bool use_stdin = argc == 1;
	const bool debug = argc >= 2 && strcmp(argv[1], "-d") == 0;
	const char* path = use_stdin ? nullptr : argv[debug + 1];

	// Remove debug flag from command line arguments
	if (debug)
	{
		argv[1] = argv[0];
		++argv;
		--argc;
	}

	// Remove source file path from command line arguments
	if (!use_stdin)
	{
		argv[1] = argv[0];
		++argv;
		--argc;
	}

	// Store command line arguments
	ARGC = argc;
	ARGV = argv;

	// Open source file
	std::istream* in = use_stdin ? &std::cin : new std::ifstream(path);
	if (!in->good())
	{
		std::cerr << "\e[31merror:\e[0m " << std::string(path) << ": No such file or directory\n";
		if (!use_stdin) delete in;
		return -1;
	}

	// Get absolute path to source file
	char* realPath = use_stdin ? nullptr : realpath(path, nullptr);

	int result = 0;
	if (debug)
		result = launchDebugClient(realPath);
	else
	{
		// Parse source code from file or stdin into AST
		MincBlockExpr* rootBlock = nullptr;
		const char* const PY_EXT = ".py";
		const int LEN_PY_EXT = 3;
		try {
			if (strncmp(realPath + strlen(realPath) - LEN_PY_EXT, PY_EXT, LEN_PY_EXT) == 0)
				rootBlock = MincBlockExpr::parsePythonFile(realPath);
			else
				rootBlock = MincBlockExpr::parseCFile(realPath);
		} catch (const CompileError& err) {
			err.print(std::cerr);
			if (!use_stdin) { ((std::ifstream*)in)->close(); delete in; }
			free(realPath);
			if (rootBlock) delete rootBlock;
			return -1;
		}

		// Name root block
		if (!use_stdin)
		{
			std::string rootBlockName = std::max(realPath, std::max(strrchr(realPath, '/') + 1, strrchr(realPath, '\\') + 1));
			const size_t dt = rootBlockName.rfind(".");
			if (dt != std::string::npos) rootBlockName = rootBlockName.substr(0, dt);

			rootBlock->name = rootBlockName;
		}
		else
			rootBlock->name = "stdin";

		// Print AST
		//printf("%s\n", rootBlock->str().c_str());

		// Build and run root block
		try {
			MincBuildtime buildtime = { nullptr };
			MINC_PACKAGE_MANAGER().import(rootBlock); // Import package manager
			std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
			rootBlock->build(buildtime);
			std::chrono::time_point<std::chrono::high_resolution_clock> endTime = std::chrono::high_resolution_clock::now();
			std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			std::cout << "build took " << diff.count() << "ms" << std::endl;
			MincRuntime runtime(nullptr, false);
			if (rootBlock->run(runtime))
			{
				std::cerr << "\e[31merror:\e[0m terminate called after throwing an instance of <" << rootBlock->lookupSymbolName(runtime.result.type, "UNKNOWN_TYPE") << ">\n";
				result = -1;
			}
		} catch (ExitException err) {
			result = err.code;
		} catch (const MincException& err) {
			err.print(std::cerr);
			result = -1;
		} catch (const std::exception& err) {
			std::cerr << err.what() << '\n';
			result = -1;
		} catch (const MincSymbol& err) {
			std::cerr << "\e[31merror:\e[0m terminate called after throwing an instance of <" << rootBlock->lookupSymbolName(err.type, "UNKNOWN_TYPE") << ">\n";
			result = -1;
		}
		delete rootBlock;
	}

	if (!use_stdin) { ((std::ifstream*)in)->close(); delete in; }
	free(realPath);
	return result;
}