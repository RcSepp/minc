SHELL := /bin/bash
SRC_DIR = src/
INC_DIR = include/
TMP_DIR = tmp/
BIN_DIR = bin/
COV_DIR = coverage/
PKG_DIR = builtin-packages/

LIBMINC_OBJS = \
	libminc/cparser.o \
	libminc/cparser.yy.o \
	libminc/pyparser.o \
	libminc/pyparser.yy.o \
	libminc/MincException.o \
	libminc/MincArgOpExpr.o \
	libminc/MincBinOpExpr.o \
	libminc/MincBlockExpr.o \
	libminc/MincBuildtime.o \
	libminc/MincCastExpr.o \
	libminc/MincCastRegister.o \
	libminc/MincEllipsisExpr.o \
	libminc/MincEncOpExpr.o \
	libminc/MincExpr.o \
	libminc/MincIdExpr.o \
	libminc/MincKernel.o \
	libminc/MincListExpr.o \
	libminc/MincLiteralExpr.o \
	libminc/MincParamExpr.o \
	libminc/MincPlchldExpr.o \
	libminc/MincPostfixExpr.o \
	libminc/MincPrefixExpr.o \
	libminc/MincRuntime.o \
	libminc/MincStatementRegister.o \
	libminc/MincStmt.o \
	libminc/MincStopExpr.o \
	libminc/MincTerOpExpr.o \
	libminc/MincVarBinOpExpr.o \

LIBMINC_PKGMGR_OBJS = \
	libminc_pkgmgr/minc_pkgmgr.o \
	libminc_pkgmgr/minc_discover.o \
	libminc_pkgmgr/json.o \

LIBMINC_DBG_OBJS = \
	libminc_dbg/minc_dbg.o \

LIBMINC_SVR_OBJS = \
	libminc_svr/minc_svr.o \

MINC_OBJS = \
	minc/minc.o \

YACC = bison
CPPFLAGS =  --coverage -g -Wall -std=c++1z -I${INC_DIR} -Ithird_party/cppdap/include -I/usr/include/nodejs/src -I/usr/include/nodejs/deps/v8/include -Ithird_party/node/include -Ithird_party/LspCpp/include -Ithird_party/rapidjson/include `pkg-config --cflags python-3.7`
MINC_LIBS = `pkg-config --libs python-3.7` -lutil -pthread -ldl -rdynamic -lnode -lboost_thread -lboost_chrono

LIBMINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_OBJS})
LIBMINC_PKGMGR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_PKGMGR_OBJS})
LIBMINC_DBG_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_DBG_OBJS})
LIBMINC_SVR_OBJPATHS = $(addprefix ${TMP_DIR}, ${LIBMINC_SVR_OBJS})
MINC_OBJPATHS = $(addprefix ${TMP_DIR}, ${MINC_OBJS})

all: ${BIN_DIR}minc builtin

clean:
	-rm -r ${TMP_DIR}* ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}libminc_svr.so ${BIN_DIR}minc
	$(MAKE) -C ${PKG_DIR}*/ clean

# Dependency management

depend: $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(LIBMINC_SVR_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

${TMP_DIR}%.d: ${SRC_DIR}%.cpp
	$(CXX) $(CPPFLAGS) -MM -MT ${TMP_DIR}$*.o $^ > $@;

-include $(LIBMINC_OBJPATHS:.o=.d) $(LIBMINC_PKGMGR_OBJPATHS:.o=.d) $(LIBMINC_DBG_OBJPATHS:.o=.d) $(LIBMINC_SVR_OBJPATHS:.o=.d) $(MINC_OBJPATHS:.o=.d)

# Coverage

coverage: ${TMP_DIR}/minc/minc.gcda
	lcov -c -d ${TMP_DIR} --include "*/src/*" -o ${TMP_DIR}lcov.info
	genhtml ${TMP_DIR}lcov.info --output-directory ${COV_DIR}
	$(MAKE) -C ${PKG_DIR}*/ $(MAKECMDGOALS)

${TMP_DIR}/minc/minc.gcda: ${BIN_DIR}minc
	${BIN_DIR}minc ${PKG_DIR}paws/test/test.minc #TODO: Create separate coverage test for minc

# Builtin packages

builtin:
	$(MAKE) -C ${PKG_DIR}*/

# minc binary

${BIN_DIR}minc: ${MINC_OBJPATHS} ${BIN_DIR}libminc.so ${BIN_DIR}libminc_pkgmgr.so ${BIN_DIR}libminc_dbg.so ${BIN_DIR}libminc_svr.so
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -o $@ ${MINC_OBJPATHS} -L${BIN_DIR} -lminc -lminc_pkgmgr -lminc_dbg -lminc_svr ${MINC_LIBS}

# libminc.so library

${BIN_DIR}libminc.so: ${LIBMINC_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_OBJPATHS}

# libminc_pkgmgr.so library

${BIN_DIR}libminc_pkgmgr.so: ${LIBMINC_PKGMGR_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_PKGMGR_OBJPATHS}

# libminc_dbg.so library

${BIN_DIR}libminc_dbg.so: ${LIBMINC_DBG_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_DBG_OBJPATHS} third_party/cppdap/lib/libcppdap.a

# libminc_svr.so library

${BIN_DIR}libminc_svr.so: ${LIBMINC_SVR_OBJPATHS}
	-mkdir -p ${BIN_DIR}
	${CXX} ${CPPFLAGS} -shared -o $@ ${LIBMINC_SVR_OBJPATHS} third_party/LspCpp/lib/liblsp.a

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
