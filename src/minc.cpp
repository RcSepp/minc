// STD
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>

// Local includes
#include "cparser.h"
#include "pyparser.h"
#include "minc_api.h"
#include "minc_pkgmgr.h"

int ARGC;
char **ARGV;
void getCommandLineArgs(int* argc, char*** argv)
{
	*argc = ARGC;
	*argv = ARGV;
}

struct ExitException
{
	const int code;
	ExitException(int code) : code(code) {}
};
void quit(int code)
{
	throw ExitException(code);
}

int main(int argc, char** argv)
{
	const bool use_stdin = argc == 1;
	const char* path = use_stdin ? nullptr : argv[1];

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
		return -1;
	}

	// Get absolute path to source file
	char buf[1024];
	const char* realPath = use_stdin ? nullptr : realpath(path, buf);

	// >>> Parse source code from file or stdin into AST

	BlockExprAST* rootBlock;
	CLexer lexer(in, &std::cout);
	yy::CParser parser(lexer, realPath, &rootBlock);
	if (parser.parse())
	{
		if (!use_stdin) ((std::ifstream*)in)->close();
		return -1;
	}

	// Name root block
	if (!use_stdin)
	{
		std::string rootBlockName = std::max(realPath, std::max(strrchr(realPath, '/') + 1, strrchr(realPath, '\\') + 1));
		const size_t dt = rootBlockName.rfind(".");
		if (dt != -1) rootBlockName = rootBlockName.substr(0, dt);

		setBlockExprASTName(rootBlock, rootBlockName);
	}
	else
		setBlockExprASTName(rootBlock, "stdin");

	// >>> Print AST

	//printf("%s\n", rootBlock->str().c_str());

	// >>> Execute source file

	int result = 0;
	try {
		MINC_PACKAGE_MANAGER().import(rootBlock); // Import package manager
		codegenExpr((ExprAST*)rootBlock, nullptr);
	} catch (ExitException err) {
		result = err.code;
	} catch (CompileError err) {
		err.print(std::cerr);
		if (!use_stdin) ((std::ifstream*)in)->close();
		return -1;
	}
	if (!use_stdin) ((std::ifstream*)in)->close();

	return result;
}