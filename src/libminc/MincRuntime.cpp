#include "minc_api.hpp"

#define STACK_SIZE 1024*1024

MincRuntime::MincRuntime()
	: stackFrames(new MincStackFrame[1024]), heapFrame(nullptr), currentStackPointerIndex(0), stackSize(STACK_SIZE), stack(new unsigned char[stackSize]), currentStackSize(0)
{
}

MincRuntime::MincRuntime(MincBlockExpr* parentBlock, bool resume)
	: parentBlock(parentBlock), resume(resume), stackFrames(new MincStackFrame[1024]), heapFrame(nullptr), currentStackPointerIndex(0), stackSize(STACK_SIZE), stack(new unsigned char[stackSize]), currentStackSize(0)
{
}

MincRuntime::~MincRuntime()
{
	delete[] stack;
}

MincObject* MincRuntime::getStackSymbol(const MincStackSymbol* stackSymbol)
{
	const MincStackFrame* stackFrame = stackSymbol->scope->stackFrame;
	if (stackFrame == nullptr)
		throw CompileError(stackSymbol->scope, currentExpr->loc, "accessing symbol in stack frame of inactive block `%S`", stackSymbol->scope->name);

	if (stackSymbol->scope->isResumable)
		return (MincObject*)(stackFrame->heapPointer + stackSymbol->location);
	else
		return (MincObject*)(stack + stackFrame->stackPointer + stackSymbol->location);
}

MincObject* MincRuntime::getStackSymbolOfNextStackFrame(const MincStackSymbol* stackSymbol)
{
	size_t location = currentStackSize + stackSymbol->location;
	return (MincObject*)(stack + location);
}