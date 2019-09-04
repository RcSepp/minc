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

const std::string APP_NAME = "minc";
const std::string HELP_MESSAGE = "minc compiler\n";
const std::map<std::string, std::string> COMMANDS = {
	{"build", "compile to executable"},
	{"run", "compile and run"},
	{"parse", "compile to LLVM IR"},
	{"debug", "compile LLVM IR and binary"},
};

int main(int argc, char **argv)
{
	int result = 0;

	// >>> Parse command line

	if (argc < 2 || COMMANDS.find(argv[1]) == COMMANDS.end())
	{
		std::cout << HELP_MESSAGE << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "\t" << APP_NAME << " [arguments]" << std::endl;
		std::cout << std::endl;
		std::cout << "The commands are:" << std::endl;
		std::cout << std::endl;
		for (auto cmd: COMMANDS)
			std::cout << "\t" << cmd.first << std::string(8 - cmd.first.size(), ' ') << cmd.second << std::endl;
		return 0;
	}
	const std::string command = argv[1];

	std::string sourcePath = argc > 2 ? argv[2] : "-";
	std::string outputPath = argv[2];
	size_t sl = outputPath.find_last_of("/\\");
	if (sl != std::string::npos) outputPath = outputPath.substr(sl + 1);
	size_t dt = outputPath.find_last_of(".");
	if (dt != std::string::npos) outputPath = outputPath.substr(0, dt);
	bool outputDebugSymbols = true;

	const std::string PAWS_EXT = ".minc";
	const bool sourceIsPaws = sourcePath.length() >= PAWS_EXT.length() && sourcePath.compare(sourcePath.length() - PAWS_EXT.length(), PAWS_EXT.length(), PAWS_EXT) == 0;
	const std::string PY_EXT = ".py";
	const bool sourceIsPython = sourcePath.length() >= PY_EXT.length() && sourcePath.compare(sourcePath.length() - PY_EXT.length(), PY_EXT.length(), PY_EXT) == 0;
	
	if (!sourceIsPaws)
	{
		initCompiler();
		initBuiltinSymbols();
	}

	// Open source file
	std::istream* in = sourcePath != "-" ? new std::ifstream(sourcePath) : &std::cin;
	if (!in->good())
	{
		std::cerr << "\e[31merror:\e[0m " << sourcePath << ": No such file or directory\n";
		return -1;
	}

	// Get absolute path to source file
	char buf[1024];
	const char* realPath = sourcePath == "-" ? nullptr : realpath(sourcePath.c_str(), buf);

	// >>> Parse source code from file or stdin into AST

	BlockExprAST* rootBlock;
	if (sourceIsPython)
	{
		PyLexer lexer(in, &std::cout);
		yy::PyParser parser(lexer, realPath, &rootBlock);
		if (parser.parse())
		{
			if (realPath != "-") ((std::ifstream*)in)->close();
			return -1;
		}
	}
	else
	{
		CLexer lexer(in, &std::cout);
		yy::CParser parser(lexer, realPath, &rootBlock);
		if (parser.parse())
		{
			if (realPath != "-") ((std::ifstream*)in)->close();
			return -1;
		}
	}

	// >>> Print AST

	//printf("%s\n", rootBlock->str().c_str());

	// >>> Compile AST

	IModule* module;
	try {
		if (sourceIsPaws)
		{
			argv[2] = argv[1];
			argv[1] = argv[0];
			result = PAWRun(rootBlock, --argc, ++argv);
		}
		else
		{
			module = createModule(realPath, rootBlock, outputDebugSymbols);
			defineBuiltinSymbols(rootBlock);
			rootBlock->codegen(nullptr);
			module->finalize();
		}
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
		if (sourcePath != "-") ((std::ifstream*)in)->close();
		return -1;
	}
	if (sourcePath != "-") ((std::ifstream*)in)->close();

	// >>> Execute command

	if (!sourceIsPaws)
	{
		if (command == "parse" || command == "debug")
		{
			if (outputPath == "-")
				module->print();
			else
				module->print(outputPath + ".ll");
			
		}
		if (command == "build")
		{
			std::string errstr;
			if (!module->compile(outputPath + ".o", errstr))
			{
				std::cerr << errstr;
				return -1;
			}
		}
		if (command == "run" || command == "debug")
		{
			module->run();
		}
	}

	return result;
}