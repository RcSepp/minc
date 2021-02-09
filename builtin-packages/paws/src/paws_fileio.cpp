#include <fstream>
#include <memory>
#include <sstream>
#include "minc_api.hpp"
#include "paws_types.h"
#include "minc_pkgmgr.h"

typedef PawsValue<std::shared_ptr<std::fstream>> PawsFile;

MincPackage PAWS_FILEIO("paws.fileio", [](MincBlockExpr* pkgScope) {
	registerType<PawsFile>(pkgScope, "PawsFile");

	class OpenFileKernel : public MincKernel
	{
		const MincStackSymbol* const varId;
	public:
		OpenFileKernel(const MincStackSymbol* varId=nullptr) : varId(varId) {}

		MincKernel* build(MincBuildtime& buildtime, std::vector<MincExpr*>& params)
		{
			const std::string& varname = ((MincIdExpr*)params[0])->name;
			params[1]->build(buildtime);
			params[2]->build(buildtime);
			const MincStackSymbol* varId = ((MincBlockExpr*)params[3])->allocStackSymbol(varname, PawsFile::TYPE, PawsFile::TYPE->size);
			params[3]->build(buildtime);
			return new OpenFileKernel(varId);
		}
		void dispose(MincKernel* kernel)
		{
			delete kernel;
		}

		bool run(MincRuntime& runtime, std::vector<MincExpr*>& params)
		{
			if (params[1]->run(runtime))
				return true;
			const std::string& filename = ((PawsString*)runtime.result.value)->get();
			if (params[2]->run(runtime))
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
				default: throw CompileError(runtime.parentBlock, params[2]->loc, "invalid mode %S", mode); break;
				}

			std::shared_ptr<std::fstream> file = std::make_shared<std::fstream>();
			file->open(filename, openmode);
			MincObject* var = block->getStackSymbolOfNextStackFrame(runtime, varId);
			new(var) PawsFile(file);

			try
			{
				if (block->run(runtime))
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

		MincObject* getType(const MincBlockExpr* parentBlock, const std::vector<MincExpr*>& params) const
		{
			return getVoid().type;
		}
	};
	pkgScope->defineStmt(MincBlockExpr::parseCTplt("open $I($E<PawsString>, $E<PawsString>) $B"), new OpenFileKernel());

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