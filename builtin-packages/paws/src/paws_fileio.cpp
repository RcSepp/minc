#include <fstream>
#include <memory>
#include <sstream>
#include "minc_api.h"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<std::shared_ptr<std::fstream>> PawsFile;

MincPackage PAWS_FILEIO("paws.fileio", [](MincBlockExpr* pkgScope) {
	registerType<PawsFile>(pkgScope, "PawsFile");

	defineStmt6_2(pkgScope, "open $I($E<PawsString>, $E<PawsString>) $B",
		[](MincBlockExpr* parentBlock, std::vector<MincExpr*>& params, void* stmtArgs) {
			const char* varname = getIdExprName((MincIdExpr*)params[0]);
			buildExpr(params[1], parentBlock);
			buildExpr(params[2], parentBlock);
			defineSymbol((MincBlockExpr*)params[3], varname, PawsFile::TYPE, nullptr);
			buildExpr(params[3], parentBlock);
		},
		[](MincRuntime& runtime, std::vector<MincExpr*>& params, void* stmtArgs) -> bool {
			const char* varname = getIdExprName((MincIdExpr*)params[0]);
			if (runExpr2(params[1], runtime))
				return true;
			const std::string& filename = ((PawsString*)runtime.result.value)->get();
			if (runExpr2(params[2], runtime))
				return true;
			const std::string& mode = ((PawsString*)runtime.result.value)->get();
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

			std::shared_ptr<std::fstream> file = std::make_shared<std::fstream>();
			file->open(filename, openmode);
			defineSymbol(block, varname, PawsFile::TYPE, new PawsFile(file));

			try
			{
				if (runExpr2((MincExpr*)block, runtime))
				{
					file->close();
					return true;
				}
			}
			catch (...)
			{
				file->close();
				throw;
			}
			file->close();
			return false;
		}
	);

	defineExpr(pkgScope, "$E<PawsFile>.read()",
		+[](std::shared_ptr<std::fstream> file) -> std::string {
			std::stringstream sstr;
			sstr << file->rdbuf();
			return sstr.str();
		}
	);

	defineExpr(pkgScope, "$E<PawsFile>.write($E<PawsString>)",
		+[](std::shared_ptr<std::fstream> file, std::string str) -> void {
			*file << str;
		}
	);
});