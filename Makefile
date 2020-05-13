SHELL := /bin/bash
SRC_DIR = src/
TEMP_DIR = tmp/
BIN_DIR = bin/

LIBMINC_OBJS = \
	stmtreg.o \
	castreg.o \
	codegen.o \
	cparser.o \
	cparser.yy.o \
	pyparser.o \
	pyparser.yy.o \

LIBMINC_PKGMGR_OBJS = \
	minc_pkgmgr.o \
	minc_discover.o \
	json.o \

LIBMINC_DBG_OBJS = \
	minc_dbg.o \

MINC_OBJS = \
	minc.o \
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
	paws_frame_eventloop.o \
	paws_array.o \
	paws_compile.o \
	llvm_constants.o \
	module.o \
	types.o \
	paws_bootstrap.o \

YACC = bison
CPPFLAGS = -g -std=c++1z -Ithird_party/cppdap/include `pkg-config --cflags python-3.7`
MINC_LIBS = `llvm-config --cxxflags --ldflags --system-libs --libs all` `pkg-config --libs python-3.7` -pthread -ldl -rdynamic

LIBMINC_OBJPATHS = $(addprefix ${TEMP_DIR}, ${LIBMINC_OBJS})
LIBMINC_PKGMGR_OBJPATHS = $(addprefix ${TEMP_DIR}, ${LIBMINC_PKGMGR_OBJS})
LIBMINC_DBG_OBJPATHS = $(addprefix ${TEMP_DIR}, ${LIBMINC_DBG_OBJS})
MINC_OBJPATHS = $(addprefix ${TEMP_DIR}, ${MINC_OBJS})

all: ${BIN_DIR}minc

clean:
	-rm -r ${TEMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}minc

# Dependency management

depend: $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

${TEMP_DIR}%.d: ${SRC_DIR}%.cpp
	$(CXX) $(CPPFLAGS) -MM -MT ${TEMP_DIR}$*.o $^ > $@;

-include $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

# minc binary

${BIN_DIR}minc: ${MINC_OBJPATHS} ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -o $@ ${MINC_OBJPATHS} -L${BIN_DIR} -lminc -lminc_pkgmgr -lminc_dbg ${MINC_LIBS}

# libminc.so library

${BIN_DIR}libminc.so: ${LIBMINC_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -shared -o $@ ${LIBMINC_OBJPATHS}

# libminc_pkgmgr.so library

${BIN_DIR}libminc_pkgmgr.so: ${LIBMINC_PKGMGR_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -shared -o $@ ${LIBMINC_PKGMGR_OBJPATHS}

# libminc_dbg.so library

${BIN_DIR}libminc_dbg.so: ${LIBMINC_DBG_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -shared -o $@ ${LIBMINC_DBG_OBJPATHS} third_party/cppdap/lib/libcppdap.a

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

${TEMP_DIR}%.o: ${SRC_DIR}%.cpp
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

${TEMP_DIR}minc.o: ${SRC_DIR}minc.cpp ${TEMP_DIR}cparser.cc ${TEMP_DIR}pyparser.cc
	-mkdir -p ${TEMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
