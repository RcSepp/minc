#include <fstream>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "minc_api.hpp"

const std::string& getTypeNameInternal(const BaseType* type);

UndefinedStmtException::UndefinedStmtException(const StmtAST* stmt)
	: CompileError("undefined statement " + stmt->str(), stmt->loc) {}
UndefinedExprException::UndefinedExprException(const ExprAST* expr)
	: CompileError("undefined expression " + expr->str(), expr->loc) {}
UndefinedIdentifierException::UndefinedIdentifierException(const IdExprAST* id)
	: CompileError('`' + id->str() + "` was not declared in this scope", id->loc) {}
InvalidTypeException::InvalidTypeException(const PlchldExprAST* plchld)
	: CompileError('`' + std::string(plchld->p2) + "` is not a type", plchld->loc) {}

CompileError::CompileError(const char* msg, Location loc)
	: loc(loc), msg(new char[strlen(msg) + 1]), refcount(new int(1))
{
	strcpy(this->msg, msg);
}

CompileError::CompileError(std::string msg, Location loc)
	: loc(loc), msg(new char[msg.size() + 1]), refcount(new int(1))
{
	strcpy(this->msg, msg.c_str());
}

CompileError::CompileError(Location loc, const char* fmt, ...)
	: loc(loc), refcount(new int(1))
{
	va_list args;
	va_start(args, fmt);

	std::stringstream msg;
	while (*fmt != '\0')
	{
		if (*fmt != '%')
		{
			msg << *fmt++;
			continue;
		}
		if (*++fmt == '\0')
			break;

		switch (*fmt++)
		{
		case '%': msg << '%'; break;
		case 'd': case 'i': msg << va_arg(args, int); break;
		case 'u': msg << va_arg(args, unsigned int); break;
		case 'o': msg << std::oct << va_arg(args, unsigned int) << std::dec; break;
		case 'x': msg << std::hex << va_arg(args, int) << std::dec; break;
		case 'p': msg << std::hex << va_arg(args, void*) << std::dec; break;
		case 'c': msg << (char)va_arg(args, int); break;
		case 'f': msg << va_arg(args, double); break;
		case 'S': msg << va_arg(args, std::string); break;
		case 's': msg << va_arg(args, char*); break;
		case 'E': msg << va_arg(args, ExprAST*)->str(); break;
		case 'e': msg << va_arg(args, ExprAST*)->shortStr(); break;
		case 't': msg << getTypeNameInternal(va_arg(args, BaseType*)); break;
		}
	}
	this->msg = new char[msg.str().size() + 1];
	strcpy(this->msg, msg.str().c_str());

	va_end(args);
}

CompileError::CompileError(CompileError& other)
	: loc(other.loc), msg(other.msg), refcount(other.refcount)
{
	other.msg = nullptr;
	++(*refcount);
}

CompileError::~CompileError()
{
	if (--(*refcount) == 0)
	{
		delete[] msg;
		delete refcount;
	}
}

void CompileError::print(std::ostream& out)
{
out << std::endl;
	if (loc.filename != nullptr)
		out << loc.filename << ':';
	out << loc.begin_line << ':';
	out << loc.begin_col << ':';
	out << " \e[1;31merror:\e[0m ";
	out << (msg != nullptr ? msg : "") << std::endl;
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