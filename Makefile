SHELL := /bin/bash
SRC_DIR = src/
INC_DIR = include/
TMP_DIR = tmp/
BIN_DIR = bin/

LIBMINC_OBJS = \
	codegen.o \
	CompileError.o \
	cparser.o \
	cparser.yy.o \
	pyparser.o \
	pyparser.yy.o \
	MincArgOpExpr.o \
	MincBinOpExpr.o \
	MincBlockExpr.o \
	MincCastExpr.o \
	CastRegister.o \
	MincEllipsisExpr.o \
	MincEncOpExpr.o \
	MincExpr.o \
	MincIdExpr.o \
	MincListExpr.o \
	MincLiteralExpr.o \
	MincParamExpr.o \
	MincPlchldExpr.o \
	MincPostfixExpr.o \
	MincPrefixExpr.o \
	StatementRegister.o \
	MincStmt.o \
	MincStopExpr.o \
	StreamingMincExprIter.o \
	MincTerOpExpr.o \
	MincVarBinOpExpr.o \

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
	paws_extend.o \
	paws_fileio.o \
	paws_time.o \
	paws_castreg.o \
	paws_stmtreg.o \
	paws_struct.o \
	paws_subroutine.o \
	paws_frame.o \
	paws_frame_eventloop.o \
	paws_array.o \

YACC = bison
CPPFLAGS = -g -Wall -std=c++1z -I${INC_DIR} -Ithird_party/cppdap/include -I/usr/include/nodejs/src -I/usr/include/nodejs/deps/v8/include `pkg-config --cflags python-3.7`
MINC_LIBS = `pkg-config --libs python-3.7` -lutil -pthread -ldl -rdynamic -lnode

LIBMINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_OBJS})
LIBMINC_PKGMGR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_PKGMGR_OBJS})
LIBMINC_DBG_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_DBG_OBJS})
MINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${MINC_OBJS})

all: ${BIN_DIR}minc

clean:
	-rm -r ${TMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}minc

# Dependency management

depend: $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

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

${TMP_DIR}%.yy.cc: ${SRC_DIR}%.l ${TMP_DIR}%.o
	-mkdir -p ${TMP_DIR}
	$(LEX) -o $@ $<

${TMP_DIR}%.cc: ${SRC_DIR}%.y
	-mkdir -p ${TMP_DIR}
	$(YACC) -o $@ $<

# Generated parser code

${TMP_DIR}%.o: ${TMP_DIR}%.cc
	-mkdir -p ${TMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

# Compiler code

${TMP_DIR}%.o: ${SRC_DIR}%.cpp
	-mkdir -p ${TMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

${TMP_DIR}minc.o: ${SRC_DIR}minc.cpp ${TMP_DIR}cparser.cc ${TMP_DIR}pyparser.cc
	-mkdir -p ${TMP_DIR}
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
