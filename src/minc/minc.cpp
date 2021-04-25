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
#include "minc_svr.h"

const char* HELP_MESSAGE =
	"Minimal Compiler\n"
	"\n"
	"Usage: minc [command] [file]\n"
	"\n"
	"Commands:\n"
	"  run (default) : Build and run the provided source file\n"
	"  debug         : Launch debug adapter\n"
	"  server        : Launch language server\n"
	"  help          : Show this help message and exit\n"
	"\n"
	"Arguments:\n"
	"  command : The command to execute (see 'Commands' section above)\n"
	"  file    : Source file to apply the command onto\n"
	"          : The file extension determines which parser to use\n"
	"          : (default: C Parser)\n"
;

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

	enum Mode {
		RUN, DEBUG, SERVER, HELP
	} mode = RUN;
	if (argc > 1)
	{
		if (strcmp(argv[1], "run") == 0)
			mode = RUN;
		else if (strcmp(argv[1], "debug") == 0)
			mode = DEBUG;
		else if (strcmp(argv[1], "server") == 0)
			mode = SERVER;
		else if (strcmp(argv[1], "help") == 0)
			mode = HELP;
		else if (argc > 2)
		{
			std::cerr << "\e[31merror:\e[0m Invalid mode '" << std::string(argv[1]) << "'. Should be one of 'run', 'debug', 'server' or 'help'\n";
			return -1;
		}
	}
	
	if (argc > 2)
	{
		// Remove mode flag from command line arguments
		argv[1] = argv[0];
		++argv;
		--argc;
	}

	const char* path = nullptr;
	if (!use_stdin)
	{
		path = argv[1];

		// Remove source file path from command line arguments
		argv[1] = argv[0];
		++argv;
		--argc;
	}

	if (mode == SERVER)
		return launchLanguageServer(argc, argv);
	else if (mode == HELP)
	{
		std::cout << HELP_MESSAGE;
		return 0;
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
	if (mode == DEBUG)
		result = launchDebugClient(realPath);
	else if (mode == RUN)
	{
		// Parse source code from file or stdin into AST
		MincBlockExpr* rootBlock = nullptr;
		try {
			if (use_stdin)
				rootBlock = MincBlockExpr::parseCStream(*in);
			else
				rootBlock = MincBlockExpr::parseFile(realPath, MincBlockExpr::flavorFromFile(realPath));
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
				std::cerr << "\e[31merror:\e[0m terminate called after throwing an instance of <" << rootBlock->lookupSymbolName(runtime.exceptionType, "UNKNOWN_TYPE") << ">\n";
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