all: Core.ll

clean:
	-rm Core.ll

Core.ll: Core.cpp
	clang -O1 -S -emit-llvm -o $@ $<