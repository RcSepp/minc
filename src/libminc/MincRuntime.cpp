#include "minc_types.h"

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