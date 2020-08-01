SHELL := /bin/bash
SRC_DIR = src/
INC_DIR = include/
TMP_DIR = tmp/
BIN_DIR = bin/

LIBMINC_OBJS = \
	libminc/codegen.o \
	libminc/cparser.o \
	libminc/cparser.yy.o \
	libminc/pyparser.o \
	libminc/pyparser.yy.o \
	libminc/MincException.o \
	libminc/MincArgOpExpr.o \
	libminc/MincBinOpExpr.o \
	libminc/MincBlockExpr.o \
	libminc/MincCastExpr.o \
	libminc/CastRegister.o \
	libminc/MincEllipsisExpr.o \
	libminc/MincEncOpExpr.o \
	libminc/MincExpr.o \
	libminc/MincIdExpr.o \
	libminc/MincListExpr.o \
	libminc/MincLiteralExpr.o \
	libminc/MincParamExpr.o \
	libminc/MincPlchldExpr.o \
	libminc/MincPostfixExpr.o \
	libminc/MincPrefixExpr.o \
	libminc/StatementRegister.o \
	libminc/MincStmt.o \
	libminc/MincStopExpr.o \
	libminc/ResolvingMincExprIter.o \
	libminc/MincTerOpExpr.o \
	libminc/MincVarBinOpExpr.o \

LIBMINC_PKGMGR_OBJS = \
	libminc_pkgmgr/minc_pkgmgr.o \
	libminc_pkgmgr/minc_discover.o \
	libminc_pkgmgr/json.o \

LIBMINC_DBG_OBJS = \
	libminc_dbg/minc_dbg.o \

MINC_OBJS = \
	minc/minc.o \

PAWS_OBJS = \
	paws/paws.o \
	paws/paws_int.o \
	paws/paws_string.o \
	paws/paws_extend.o \
	paws/paws_fileio.o \
	paws/paws_time.o \
	paws/paws_castreg.o \
	paws/paws_stmtreg.o \
	paws/paws_struct.o \
	paws/paws_subroutine.o \
	paws/paws_frame.o \
	paws/paws_frame_eventloop.o \
	paws/paws_array.o \

YACC = bison
CPPFLAGS = -g -Wall -std=c++1z -I${INC_DIR} -Ithird_party/cppdap/include -I/usr/include/nodejs/src -I/usr/include/nodejs/deps/v8/include -Ithird_party/node/include `pkg-config --cflags python-3.7`
MINC_LIBS = `pkg-config --libs python-3.7` -lutil -pthread -ldl -rdynamic -lnode

LIBMINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_OBJS})
LIBMINC_PKGMGR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_PKGMGR_OBJS})
LIBMINC_DBG_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_DBG_OBJS})
MINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${MINC_OBJS})
PAWS_OBJPATHS = $(addprefix ${TMP_DIR}, ${PAWS_OBJS})

all: ${BIN_DIR}minc

clean:
	-rm -r ${TMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}minc

# Dependency management

depend: $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d) $(PAWS_OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

-include $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d) $(PAWS_OBJPATHS:.o=.d)

# minc binary

${BIN_DIR}minc: ${MINC_OBJPATHS} ${PAWS_OBJPATHS} ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} ${INCLUDES} -o $@ ${MINC_OBJPATHS} ${PAWS_OBJPATHS} -L${BIN_DIR} -lminc -lminc_pkgmgr -lminc_dbg ${MINC_LIBS}

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
	-mkdir -p `dirname $@`
	$(LEX) -o $@ $<

${TMP_DIR}%.cc: ${SRC_DIR}%.y
	-mkdir -p `dirname $@`
	$(YACC) -o $@ $<

# Generated parser code

${TMP_DIR}%.o: ${TMP_DIR}%.cc
	-mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

# Compiler code

${TMP_DIR}%.o: ${SRC_DIR}%.cpp
	-mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<

${TMP_DIR}libminc/codegen.o: ${SRC_DIR}libminc/codegen.cpp ${TMP_DIR}libminc/cparser.cc ${TMP_DIR}libminc/pyparser.cc
	-mkdir -p `dirname $@`
	$(CXX) ${CPPFLAGS} -o $@ -c -fPIC $<
