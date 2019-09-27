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
std::cerr << std::endl;
		if (err.loc.filename != nullptr)
			std::cerr << err.loc.filename << ':';
		std::cerr << err.loc.begin_line << ':';
		std::cerr << err.loc.begin_col << ':';
		std::cerr << " \e[1;31merror:\e[0m ";
		std::cerr << err.msg << std::endl;
		for (std::string& hint: err.hints)
			std::cerr << "\t\e[1;94mnote:\e[0m " << hint << std::endl;
		if (err.loc.filename != nullptr && err.loc.begin_line == err.loc.end_line && err.loc.begin_col > err.loc.end_col) //TODO: Cleanup
		{
			in->seekg(0, in->beg);
			char c;
			for (int lineno = 1; lineno < err.loc.begin_line; in->read(&c, 1))
				if (c == '\n')
					++lineno;
			char linebuf[0x1000]; //TODO: Read line without fixed buffer size
			linebuf[0] = c;
			in->getline(linebuf + 1, 0x1000);
			std::cerr << std::string(linebuf, linebuf + err.loc.begin_col - 1);
			std::cerr << "\e[31m" << std::string(linebuf + err.loc.begin_col - 1, linebuf + err.loc.end_col - 1) << "\e[0m";
			std::cerr << std::string(linebuf + err.loc.end_col - 1) << std::endl;
			for (int i = 0; i < err.loc.begin_col; ++i) linebuf[i] = linebuf[i] == '\t' ? '\t' : ' ';
			std::cerr << std::string(linebuf, linebuf + err.loc.begin_col - 1);
			std::cerr << "\e[31m" << std::string(1, '^') << std::string(err.loc.end_col - err.loc.begin_col - 1, '~') << "\e[0m" << std::endl;
		}
		if (!use_stdin) ((std::ifstream*)in)->close();
		return -1;
	}
	if (!use_stdin) ((std::ifstream*)in)->close();

	return result;
}