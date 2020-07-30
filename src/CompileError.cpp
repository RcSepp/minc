#include <fstream>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "minc_api.hpp"

UndefinedStmtException::UndefinedStmtException(const MincStmt* stmt)
	: CompileError("undefined statement " + stmt->str(), stmt->loc) {}
UndefinedExprException::UndefinedExprException(const MincExpr* expr)
	: CompileError("undefined expression " + expr->str(), expr->loc) {}
UndefinedIdentifierException::UndefinedIdentifierException(const MincIdExpr* id)
	: CompileError('`' + id->str() + "` was not declared in this scope", id->loc) {}
InvalidTypeException::InvalidTypeException(const MincPlchldExpr* plchld)
	: CompileError('`' + std::string(plchld->p2) + "` is not a type", plchld->loc) {}

MincException::MincException(const char* msg, MincLocation loc)
	: refcount(new int(1)), msg(new char*()), loc(loc)
{
	*this->msg = new char[strlen(msg) + 1];
	strcpy(*this->msg, msg);
}

MincException::MincException(std::string msg, MincLocation loc)
	: refcount(new int(1)), msg(new char*()), loc(loc)
{
	*this->msg = new char[msg.size() + 1];
	strcpy(*this->msg, msg.c_str());
}

MincException::MincException(MincLocation loc)
	: refcount(new int(1)), msg(new char*()), loc(loc)
{
	*this->msg = nullptr;
}

MincException::MincException(const MincException& other)
	: refcount(other.refcount), msg(other.msg), loc(other.loc)
{
	++(*refcount);
}

CompileError::CompileError(std::string msg, MincLocation loc)
	: MincException(msg, loc)
{
}

CompileError::CompileError(const char* msg, MincLocation loc)
	: MincException(msg, loc)
{
}

CompileError::CompileError(const MincBlockExpr* scope, MincLocation loc, const char* fmt, ...)
	: MincException(loc)
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
		case 'E': msg << va_arg(args, MincExpr*)->str(); break;
		case 'e': msg << va_arg(args, MincExpr*)->shortStr(); break;
		case 'T': msg << scope->lookupSymbolName(va_arg(args, MincExpr*)->getType(scope), "UNKNOWN_TYPE"); break;
		case 't': msg << scope->lookupSymbolName(va_arg(args, MincObject*), "UNKNOWN_TYPE"); break;
		}
	}
	*this->msg = new char[msg.str().size() + 1];
	strcpy(*this->msg, msg.str().c_str());

	va_end(args);
}

MincException::~MincException()
{
	if (--(*refcount) == 0)
	{
		if (*msg != nullptr)
			delete[] *msg;
		delete msg;
		delete refcount;
	}
}

void MincException::print(std::ostream& out) const noexcept
{
out << std::endl;
	if (loc.filename != nullptr)
		out << loc.filename << ':';
	out << loc.begin_line << ':';
	out << loc.begin_column << ':';
	out << " \e[1;31merror:\e[0m ";
	const char* what;
	out << ((what = this->what()) != nullptr ? what : "") << std::endl;
	if (loc.filename != nullptr && loc.begin_line == loc.end_line && loc.begin_column > loc.end_column) //TODO: Cleanup
	{
		std::ifstream in(loc.filename);
		char c;
		for (unsigned lineno = 1; lineno < loc.begin_line; in.read(&c, 1))
			if (c == '\n')
				++lineno;
		char linebuf[0x1000]; //TODO: Read line without fixed buffer size
		linebuf[0] = c;
		in.getline(linebuf + 1, 0x1000);
		out << std::string(linebuf, linebuf + loc.begin_column - 1);
		out << "\e[31m" << std::string(linebuf + loc.begin_column - 1, linebuf + loc.end_column - 1) << "\e[0m";
		out << std::string(linebuf + loc.end_column - 1) << std::endl;
		for (unsigned i = 0; i < loc.begin_column; ++i) linebuf[i] = linebuf[i] == '\t' ? '\t' : ' ';
		out << std::string(linebuf, linebuf + loc.begin_column - 1);
		out << "\e[31m" << std::string(1, '^') << std::string(loc.end_column - loc.begin_column - 1, '~') << "\e[0m" << std::endl;
		in.close();
	}
}