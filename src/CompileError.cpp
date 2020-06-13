#include <fstream>
#include <stdio.h>
#include "minc_api.hpp"

UndefinedStmtException::UndefinedStmtException(const StmtAST* stmt)
	: CompileError("undefined statement " + stmt->str(), stmt->loc) {}
UndefinedExprException::UndefinedExprException(const ExprAST* expr)
	: CompileError("undefined expression " + expr->str(), expr->loc) {}
UndefinedIdentifierException::UndefinedIdentifierException(const IdExprAST* id)
	: CompileError('`' + id->str() + "` was not declared in this scope", id->loc) {}
InvalidTypeException::InvalidTypeException(const PlchldExprAST* plchld)
	: CompileError('`' + std::string(plchld->p2) + "` is not a type", plchld->loc) {}

void CompileError::print(std::ostream& out)
{
out << std::endl;
	if (loc.filename != nullptr)
		out << loc.filename << ':';
	out << loc.begin_line << ':';
	out << loc.begin_col << ':';
	out << " \e[1;31merror:\e[0m ";
	out << msg << std::endl;
	for (std::string& hint: hints)
		out << "\t\e[1;94mnote:\e[0m " << hint << std::endl;
	if (loc.filename != nullptr && loc.begin_line == loc.end_line && loc.begin_col > loc.end_col) //TODO: Cleanup
	{
		std::ifstream in(loc.filename);
		char c;
		for (int lineno = 1; lineno < loc.begin_line; in.read(&c, 1))
			if (c == '\n')
				++lineno;
		char linebuf[0x1000]; //TODO: Read line without fixed buffer size
		linebuf[0] = c;
		in.getline(linebuf + 1, 0x1000);
		out << std::string(linebuf, linebuf + loc.begin_col - 1);
		out << "\e[31m" << std::string(linebuf + loc.begin_col - 1, linebuf + loc.end_col - 1) << "\e[0m";
		out << std::string(linebuf + loc.end_col - 1) << std::endl;
		for (int i = 0; i < loc.begin_col; ++i) linebuf[i] = linebuf[i] == '\t' ? '\t' : ' ';
		out << std::string(linebuf, linebuf + loc.begin_col - 1);
		out << "\e[31m" << std::string(1, '^') << std::string(loc.end_col - loc.begin_col - 1, '~') << "\e[0m" << std::endl;
		in.close();
	}
}