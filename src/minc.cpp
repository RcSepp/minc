// STD
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>

// Local includes
#include "cparser.h"
#include "pyparser.h"
#include "api.h"
#include "builtin.h"
#include "paws.h"

int main(int argc, char **argv)
{
	const bool use_stdin = argc == 1;

	// Open source file
	std::istream* in = use_stdin ? &std::cin : new std::ifstream(argv[1]);
	if (!in->good())
	{
		std::cerr << "\e[31merror:\e[0m " << std::string(argv[1]) << ": No such file or directory\n";
		return -1;
	}

	// Get absolute path to source file
	char buf[1024];
	const char* realPath = use_stdin ? nullptr : realpath(argv[1], buf);

	// >>> Parse source code from file or stdin into AST

	BlockExprAST* rootBlock;
	CLexer lexer(in, &std::cout);
	yy::CParser parser(lexer, realPath, &rootBlock);
	if (parser.parse())
	{
		if (!use_stdin) ((std::ifstream*)in)->close();
		return -1;
	}

	// >>> Print AST

	//printf("%s\n", rootBlock->str().c_str());

	// >>> Compile AST

	int result = 0;
	try {
		argv[1] = argv[0];
		result = PAWRun(rootBlock, --argc, ++argv);
	} catch (CompileError err) {
		err.print(std::cerr);
		if (!use_stdin) ((std::ifstream*)in)->close();
		return -1;
	}
	if (!use_stdin) ((std::ifstream*)in)->close();

	return result;
}