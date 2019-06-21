//#include <llvm/IR/LLVMContext.h>

//using namespace llvm;

#include "../src/ast.h"
#include "../src/driver.h"

void FOO(BlockAST* parentBlock, int16_t bla)
{
	//parentBlock->addToScope("fooooo", nullptr);
	parentBlock->addToScope();
}

int main()
{
	/*LLVMContext TheContext;
	TheContext.emitError("foo");
	return 0;*/

	Driver driver;
	BlockAST block(driver, {});

	FOO(&block, 123);

	return 0;
}