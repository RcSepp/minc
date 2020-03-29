#include "paws_frame_eventloop.h"
#include <cassert>
#include "ast.h" // Including "ast.h" instead of "minc_api.h" for CompileError
#include "paws_types.h"
#include "paws_subroutine.h"
#include "minc_pkgmgr.h"
#include <limits> // For NaN
#include <cmath> // For isnan()

struct Awaitable
{
	Variable result;
	double delay; //TODO: Replace delays with timestamps

	Awaitable(double delay = 0.0) : delay(delay), result(nullptr, nullptr)
	{
		if (std::isnan(delay))
			throw std::invalid_argument("Trying to create awaitable with NaN delay");
	}
	virtual void resume() = 0;

	bool isDone() { return std::isnan(delay); }
	void setDone(const Variable& result)
	{
		this->result = result;
		this->delay = std::numeric_limits<double>::quiet_NaN();
	}
};
typedef PawsValue<Awaitable*> PawsAwaitable;

struct Frame
{
	PawsType* returnType;
	std::vector<PawsType*> argTypes;
	std::vector<std::string> argNames;
	BlockExprAST* body;

	Frame() = default;
	Frame(PawsType* returnType, std::vector<PawsType*> argTypes, std::vector<std::string> argNames, BlockExprAST* body)
		: returnType(returnType), argTypes(argTypes), argNames(argNames), body(body) {}
};
typedef PawsValue<Frame*> PawsFrame;

struct FrameInstance : public Awaitable
{
private:
	const Frame* frame;
	BlockExprAST* instance;
	Awaitable* blocker;

public:
	FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs);
	~FrameInstance() { removeBlockExprAST(instance); }
	void resume();
};

struct AwaitException
{
	Awaitable* blocker;
	AwaitException(Awaitable* blocker) : blocker(blocker) {}
};

struct SleepInstance : public Awaitable
{
	SleepInstance(double duration) : Awaitable(duration) {}
	void resume()
	{
		setDone(Variable(PawsVoid::TYPE, nullptr));
	}
};

FrameInstance::FrameInstance(const Frame* frame, BlockExprAST* callerScope, const std::vector<ExprAST*>& argExprs)
	: frame(frame), instance(cloneBlockExprAST(frame->body)), blocker(nullptr)
{
	instance->parent = frame->body;

	// Define arguments in frame instance
	for (size_t i = 0; i < argExprs.size(); ++i)
		defineSymbol(instance, frame->argNames[i].c_str(), frame->argTypes[i], codegenExpr(argExprs[i], callerScope).value);

	// Define await statement in frame instance scope
	defineExpr3(instance, "await $E<PawsAwaitable>",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			Awaitable* blocker = ((PawsAwaitable*)codegenExpr(getCastExprASTSource((CastExprAST*)params[0]), parentBlock).value)->get();
			if (blocker->isDone())
			{
				Variable result = blocker->result;
				return result;
			}
			else
				throw AwaitException(blocker);
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);
}

void FrameInstance::resume()
{
	// Execute blocking awaitable
	if (blocker != nullptr)
	{
		blocker->resume();
		if (blocker->isDone()) // If blocking awaitable finished, ...
			blocker = nullptr; // Remove blocking awaitable
			// Resume frame instance
		else // If blocking awaitable is still blocking, ...
		{
			delay = blocker->delay; // Update frame instance delay to new blocker delay
			return; // Frame instance is blocked. Don't resume it
		}
	}

	// Resume frame instance
	try
	{
		codegenExpr((ExprAST*)instance, getBlockExprASTParent(instance));
		if (frame->returnType != getVoid().type && frame->returnType != PawsVoid::TYPE)
			raiseCompileError("missing return statement in frame body", (ExprAST*)instance);
		setDone(Variable(PawsVoid::TYPE, nullptr));
	}
	catch (ReturnException err)
	{
		setDone(err.result);
	}
	catch (AwaitException err)
	{
		blocker = err.blocker;
		delay = blocker->delay; // Update frame instance delay to blocker delay
	}
}

MincPackage PAWS_FRAME("paws.frame", [](BlockExprAST* pkgScope) {
	MINC_PACKAGE_MANAGER().importPackage(pkgScope, "paws.subroutine");

	registerType<PawsAwaitable>(pkgScope, "PawsAwaitable");
	registerType<PawsFrame>(pkgScope, "PawsFrame");
	defineOpaqueInheritanceCast(pkgScope, PawsFrame::TYPE, PawsAwaitable::TYPE);

	// Define sleep function
	defineConstantFunction(pkgScope, "sleep", PawsTpltType::get(PawsAwaitable::TYPE, PawsVoid::TYPE), { PawsDouble::TYPE }, { "duration" },
		[](BlockExprAST* callerScope, const std::vector<ExprAST*>& args, void* funcArgs) -> Variable
		{
			double duration = ((PawsDouble*)codegenExpr(args[0], callerScope).value)->get();
			return Variable(PawsTpltType::get(PawsAwaitable::TYPE, PawsVoid::TYPE), new PawsAwaitable(new SleepInstance(duration)));
		}
	);

	// Define frame definition
	defineStmt2(pkgScope, "$E<PawsMetaType> frame $I($E<PawsMetaType> $I, ...) $B",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* stmtArgs) {
			PawsType* returnType = ((PawsMetaType*)codegenExpr(params[0], parentBlock).value)->get();
			const char* frameName = getIdExprASTName((IdExprAST*)params[1]);
			const std::vector<ExprAST*>& argTypeExprs = getExprListASTExpressions((ExprListAST*)params[2]);
			const std::vector<ExprAST*>& argNameExprs = getExprListASTExpressions((ExprListAST*)params[3]);
			BlockExprAST* block = (BlockExprAST*)params[4];

			// Set frame parent to frame definition scope
			setBlockExprASTParent(block, parentBlock);

			// Define return statement in frame scope
			definePawsReturnStmt(block, returnType);

			Frame* frame = new Frame();
			frame->returnType = returnType;
			frame->argTypes.reserve(argTypeExprs.size());
			for (ExprAST* argTypeExpr: argTypeExprs)
				frame->argTypes.push_back(((PawsMetaType*)codegenExpr(argTypeExpr, parentBlock).value)->get());
			frame->argNames.reserve(argNameExprs.size());
			for (ExprAST* argNameExpr: argNameExprs)
				frame->argNames.push_back(getIdExprASTName((IdExprAST*)argNameExpr));
			frame->body = block;

			PawsType* frameType = PawsTpltType::get(PawsFrame::TYPE, returnType);
			defineSymbol(parentBlock, frameName, frameType, new PawsFrame(frame));
		}
	);

	// Define frame call
	defineExpr3(pkgScope, "$E<PawsFrame>($E, ...)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Frame* frame = ((PawsFrame*)codegenExpr(params[0], parentBlock).value)->get();
			std::vector<ExprAST*>& argExprs = getExprListASTExpressions((ExprListAST*)params[1]);

			// Check number of arguments
			if (frame->argTypes.size() != argExprs.size())
				raiseCompileError("invalid number of frame arguments", params[0]);

			// Check argument types and perform inherent type casts
			for (size_t i = 0; i < argExprs.size(); ++i)
			{
				ExprAST* argExpr = argExprs[i];
				BaseType *expectedType = frame->argTypes[i], *gotType = getType(argExpr, parentBlock);

				if (expectedType != gotType)
				{
					ExprAST* castExpr = lookupCast(parentBlock, argExpr, expectedType);
					if (castExpr == nullptr)
					{
						std::string candidateReport = reportExprCandidates(parentBlock, argExpr);
						raiseCompileError(
							("invalid frame argument type: " + ExprASTToString(argExpr) + "<" + getTypeName(gotType) + ">, expected: <" + getTypeName(expectedType) + ">\n" + candidateReport).c_str(),
							argExpr
						);
					}
					argExprs[i] = castExpr;
				}
			}

			// Call frame
			FrameInstance* instance = new FrameInstance(frame, parentBlock, argExprs);
			instance->resume();

			PawsType* const awaitableType = PawsTpltType::get(PawsAwaitable::TYPE, frame->returnType);
			return Variable(awaitableType, new PawsAwaitable(instance));
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			PawsTpltType* const frameType = (PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock);
			PawsTpltType* const awaitableType = PawsTpltType::get(PawsAwaitable::TYPE, frameType->tpltType);
			return awaitableType;
		}
	);

	// Define frames-main
	defineExpr3(pkgScope, "frames.main($E<PawsFrame>)",
		[](BlockExprAST* parentBlock, std::vector<ExprAST*>& params, void* exprArgs) -> Variable {
			const Frame* frame = ((PawsFrame*)codegenExpr(params[0], parentBlock).value)->get();
			std::vector<ExprAST*> argExprs;
			Awaitable* awaitable = new FrameInstance(frame, parentBlock, argExprs);
			Variable result(PawsVoid::TYPE, nullptr);
			CompileError* error = nullptr;

			EventLoop eventloop;
			std::function<void(void)> cbk = [&]() {
				try
				{
					awaitable->resume();
				}
				catch (CompileError err)
				{
					error = new CompileError(err.msg, Location(err.loc));
					eventloop.post(bind(&EventLoop::close, &eventloop), 0.0f);
					return;
				}

				if (awaitable->isDone())
				{
					result = awaitable->result;
					eventloop.post(bind(&EventLoop::close, &eventloop), 0.0f);
				}
				else
					eventloop.post(cbk, awaitable->delay);
			};
			eventloop.post(cbk, 0.0f);
			eventloop.run();

			delete awaitable;

			if (error)
				throw *error;
			else
				return result;
		}, [](const BlockExprAST* parentBlock, const std::vector<ExprAST*>& params, void* exprArgs) -> BaseType* {
			assert(ExprASTIsCast(params[0]));
			return ((PawsTpltType*)getType(getCastExprASTSource((CastExprAST*)params[0]), parentBlock))->tpltType;
		}
	);
});