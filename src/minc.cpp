// STD
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
	argc = ARGC;
	argv = ARGV;
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

	// >>> Parse source code from file or stdin into AST

	BlockExprAST* rootBlock = nullptr;
	const char* const PY_EXT = ".py";
	const int LEN_PY_EXT = 3;
	try {
		if (strncmp(realPath + strlen(realPath) - LEN_PY_EXT, PY_EXT, LEN_PY_EXT) == 0)
			rootBlock = BlockExprAST::parsePythonFile(realPath);
		else
			rootBlock = BlockExprAST::parseCFile(realPath);
	} catch (CompileError err) {
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
		if (dt != -1) rootBlockName = rootBlockName.substr(0, dt);

		rootBlock->name = rootBlockName;
	}
	else
		rootBlock->name = "stdin";

	// >>> Print AST

	//printf("%s\n", rootBlock->str().c_str());

	// >>> Execute source file

	int result = 0;
	if (debug)
		result = launchDebugClient(rootBlock);
	else
	{
		try {
			MINC_PACKAGE_MANAGER().import(rootBlock); // Import package manager
			rootBlock->codegen(nullptr);
		} catch (ExitException err) {
			result = err.code;
		} catch (CompileError err) {
			err.print(std::cerr);
			result = -1;
		}
	}

	if (!use_stdin) { ((std::ifstream*)in)->close(); delete in; }
	free(realPath);
	delete rootBlock;
	return result;
}