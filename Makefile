SHELL := /bin/bash
SRC_DIR = src/
TEMP_DIR = tmp/
BIN_DIR = bin/

LIBMINC_OBJS = \
	stmtreg.o \
	castreg.o \
	codegen.o \
	module.o \
	llvm_constants.o \
	types.o \
	cparser.o \
	cparser.yy.o \
	pyparser.o \
	pyparser.yy.o \

MINC_OBJS = \
	minc.o \
	minc_pkgmgr.o \
	paws.o \
	paws_int.o \
	paws_string.o \
	paws_fileio.o \
	paws_time.o \
	paws_llvm.o \
	paws_castreg.o \
	paws_stmtreg.o \
	paws_subroutine.o \
	paws_frame.o \
	paws_array.o \
	paws_bootstrap.o \

YACC = bison
CPPFLAGS = -g -std=c++1z
LIBS = `llvm-config --cxxflags --ldflags --system-libs --libs all` -fexceptions -rdynamic

LIBMINC_OBJPATHS = $(addprefix ${TEMP_DIR}, ${LIBMINC_OBJS})
MINC_OBJPATHS = $(addprefix ${TEMP_DIR}, ${MINC_OBJS})

all: ${BIN_DIR}minc

clean:
	-rm -r ${TEMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}minc

depend: $(LIBMINC_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

# Dependency management

${TEMP_DIR}%.d: ${SRC_DIR}%.cpp
	$(CXX) $(CPPFLAGS) -MM -MT ${TEMP_DIR}$*.o $^ > $@;

-include $(LIBMINC_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

# minc binary

${BIN_DIR}minc: ${MINC_OBJPATHS} ${BIN_DIR}libminc.so
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -o $@ ${MINC_OBJPATHS} -L${BIN_DIR} -lminc ${LIBS}

# libminc.so library

${BIN_DIR}libminc.so: ${LIBMINC_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -shared -o $@ ${LIBMINC_OBJPATHS} ${LIBS}

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
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

# Compiler code

${TEMP_DIR}%.o:
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

${TEMP_DIR}minc.o: ${SRC_DIR}minc.cpp ${TEMP_DIR}cparser.cc ${TEMP_DIR}pyparser.cc
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
