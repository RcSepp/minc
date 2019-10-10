SHELL := /bin/bash
SRC_DIR = src/
TEMP_DIR = tmp/
BIN_DIR = bin/

YACC = bison
CPPFLAGS = -g -std=c++1z
OBJS = minc.o stmtreg.o castreg.o paws.o paws_bootstrap.o paws_pkgmgr.o codegen.o module.o llvm_constants.o types.o cparser.o cparser.yy.o pyparser.o pyparser.yy.o
LIBS = `llvm-config --cxxflags --ldflags --system-libs --libs all` -fexceptions -rdynamic

OBJPATHS = $(addprefix ${TEMP_DIR}, ${OBJS})

all: ${BIN_DIR}minc

clean:
	-rm -r ${TEMP_DIR}* ${BIN_DIR}minc

# Final binary

${BIN_DIR}minc: ${OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -o $@ ${OBJPATHS} ${LIBS}

# Parser code

${TEMP_DIR}%.yy.cc: ${SRC_DIR}%.l ${TEMP_DIR}%.o
	-mkdir -p ${TEMP_DIR}
	$(LEX) -o $@ $<

${TEMP_DIR}%.cc: ${SRC_DIR}%.y
	-mkdir -p ${TEMP_DIR}
	$(YACC) -o $@ $<

# Generated parser code

${TEMP_DIR}%.o: ${TEMP_DIR}%.cc
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c $<

# Compiler code

${TEMP_DIR}%.o: ${SRC_DIR}%.cpp
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c $<

${TEMP_DIR}minc.o: ${SRC_DIR}minc.cpp ${TEMP_DIR}cparser.cc ${TEMP_DIR}pyparser.cc
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c $<
