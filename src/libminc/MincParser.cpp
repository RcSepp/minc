#include <cstring>
#include "minc_api.h"

static const MincFlavor DEFAULT_FALVOR = MincFlavor::C_FLAVOR;

extern "C"
{
	MincFlavor flavorFromFile(const char* filename)
	{
		static const char* const MINC_EXT = ".minc";
		const int LEN_MINC_EXT = 5;
		static const char* const PY_EXT = ".py";
		const int LEN_PY_EXT = 3;
		static const char* const GO_EXT = ".go";
		const int LEN_GO_EXT = 3;

		const size_t len = strlen(filename);

		if (strncmp(filename + len - LEN_MINC_EXT, MINC_EXT, LEN_MINC_EXT) == 0)
		{
			if (strncmp(filename + len - LEN_MINC_EXT - LEN_PY_EXT, PY_EXT, LEN_PY_EXT) == 0)
				return MincFlavor::PYTHON_FLAVOR;
			else if (strncmp(filename + len - LEN_MINC_EXT - LEN_GO_EXT, GO_EXT, LEN_GO_EXT) == 0)
				return MincFlavor::GO_FLAVOR;
			else
				return DEFAULT_FALVOR;
		}
		if (strncmp(filename + len - LEN_PY_EXT, PY_EXT, LEN_PY_EXT) == 0)
			return MincFlavor::PYTHON_FLAVOR;
		else if (strncmp(filename + len - LEN_GO_EXT, GO_EXT, LEN_GO_EXT) == 0)
			return MincFlavor::GO_FLAVOR;
		else
			return MincFlavor::UNKNOWN_FLAVOR;
	}

	MincBlockExpr* parseStream(std::istream& stream, MincFlavor flavor)
	{
		switch (flavor)
		{
		case MincFlavor::C_FLAVOR: return parseCStream(stream);
		case MincFlavor::PYTHON_FLAVOR: return parsePythonStream(stream);
		case MincFlavor::GO_FLAVOR: return parseGoStream(stream);
		default: throw CompileError("Unknown flavor");
		}
	}

	MincBlockExpr* parseFile(const char* filename, MincFlavor flavor)
	{
		switch (flavor)
		{
		case MincFlavor::C_FLAVOR: return parseCFile(filename);
		case MincFlavor::PYTHON_FLAVOR: return parsePythonFile(filename);
		case MincFlavor::GO_FLAVOR: return parseGoFile(filename);
		default: throw CompileError("Unknown flavor");
		}
	}

	MincBlockExpr* parseCode(const char* code, MincFlavor flavor)
	{
		switch (flavor)
		{
		case MincFlavor::C_FLAVOR: return parseCCode(code);
		case MincFlavor::PYTHON_FLAVOR: return parsePythonCode(code);
		case MincFlavor::GO_FLAVOR: return parseGoCode(code);
		default: throw CompileError("Unknown flavor");
		}
	}

	const std::vector<MincExpr*> parseTplt(const char* tpltStr, MincFlavor flavor)
	{
		switch (flavor)
		{
		case MincFlavor::C_FLAVOR: return parseCTplt(tpltStr);
		case MincFlavor::PYTHON_FLAVOR: return parsePythonTplt(tpltStr);
		case MincFlavor::GO_FLAVOR: return parseGoTplt(tpltStr);
		default: throw CompileError("Unknown flavor");
		}
	}
}