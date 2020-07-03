#include <fstream>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<std::fstream&> PawsFile;

MincPackage PAWS_FILEIO("paws.fileio", [](MincBlockExpr* pkgScope) {
	registerType<PawsFile>(pkgScope, "PawsFile");

	defineStmt2(pkgScope, "open $I($E<PawsString>, $E<PawsString>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			const char* varname = getIdExprName((MincIdExpr*)params[0]);
			const std::string& filename = ((PawsString*)codegenExpr(params[1], parentBlock).value)->get();
			const std::string& mode = ((PawsString*)codegenExpr(params[2], parentBlock).value)->get();
			MincBlockExpr* block = (MincBlockExpr*)params[3];

			std::ios_base::openmode openmode = (std::ios_base::openmode)0;
			for (char m: mode)
				switch (m)
				{
				case 'r': openmode |= std::ios_base::in; break;
				case 'w': openmode |= std::ios_base::out | std::ios_base::trunc; break;
				case 'a': openmode |= std::ios_base::out | std::ios_base::app; break;
				case 'b': openmode |= std::ios_base::binary; break;
				case 't': openmode &= ~std::ios_base::binary; break;
				default: raiseCompileError(("invalid mode " + mode).c_str(), params[2]); break;
				}

			std::fstream file;
			file.open(filename, openmode);
			defineSymbol(block, varname, PawsFile::TYPE, new PawsFile(file));

			try
			{
				codegenExpr((MincExpr*)block, parentBlock);
			}
			catch (...)
			{
				file.close();
				throw;
			}
			file.close();
		}
	);

	defineExpr(pkgScope, "$E<PawsFile>.read()",
		+[](std::fstream& file) -> std::string {
			std::stringstream sstr;
			sstr << file.rdbuf();
			return sstr.str();
		}
	);

	defineExpr(pkgScope, "$E<PawsFile>.write($E<PawsString>)",
		+[](std::fstream& file, std::string str) -> void {
			file << str;
		}
	);
});