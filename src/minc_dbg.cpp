// STD
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <unordered_set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <unistd.h>

// cppdap
#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

// Local includes
#include "ast.h"
#include "minc_api.h"
#include "minc_cli.h"
#include "minc_dbg.h"
#include "minc_pkgmgr.h"

//#define DEBUG_MULTITHREADING
#ifdef DEBUG_MULTITHREADING
#include <pthread.h> //DELETE
#include <fstream> //DELETE
#include "cparser.h" //DELETE
#endif

#ifdef _MSC_VER
#define OS_WINDOWS 1
#endif

#ifdef OS_WINDOWS
#include <fcntl.h> // _O_BINARY
#include <io.h> // _setmode
#endif

#define LOG_TO_FILE "/home/sepp/Development/minc/log.txt"
// Log file regex: REPLACE "Content-Length[\S\s\n]*?\{" with " {"

#define DEBUG_STEP_EVENTS

std::vector<GetValueStrFunc> valueSerializers;
void registerValueSerializer(GetValueStrFunc serializer)
{
	valueSerializers.push_back(serializer);
}
bool getValueStr(const BaseValue* value, std::string* valueStr)
{
	for (GetValueStrFunc valueSerializer: valueSerializers)
		if (valueSerializer(value, valueStr))
			return true;
	return false;
}

// Declare custom lunch request type with Minc specific properties defined by the debug client
namespace dap
{
	struct MincLaunchRequest : public LaunchRequest
	{
		optional<array<string>> args;
		optional<string> cwd;
		optional<boolean> stopOnEntry;
		optional<boolean> traceAnonymousBlocks;
	};
	DAP_DECLARE_STRUCT_TYPEINFO(MincLaunchRequest);
	DAP_IMPLEMENT_STRUCT_TYPEINFO(
		MincLaunchRequest,
		"launch",
		DAP_FIELD(restart, "__restart"),
		DAP_FIELD(noDebug, "noDebug"),
		DAP_FIELD(args, "args"),
		DAP_FIELD(cwd, "cwd"),
		DAP_FIELD(stopOnEntry, "stopOnEntry"),
		DAP_FIELD(traceAnonymousBlocks, "traceAnonymousBlocks"),
	);
}

class Event
{
public:
	// wait() blocks until the event is fired.
	void wait(bool reset=false)
	{
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [&] { return fired; });
		if (reset)
			fired = false;
	}

	// fire() sets signals the event, and unblocks any calls to wait().
	void fire()
	{
		std::unique_lock<std::mutex> lock(mutex);
		fired = true;
		cv.notify_all();
	}

private:
	std::mutex mutex;
	std::condition_variable cv;
	bool fired = false;
};

class DebugOutputBuffer : public std::stringbuf
{
private:
	std::ostream& srcStream;
	std::streambuf* const originalBuffer;
	dap::Session* const session;

public:
	DebugOutputBuffer(std::ostream& stream, dap::Session* session)
		: std::stringbuf(std::ios_base::out), srcStream(stream), originalBuffer(stream.rdbuf(this)), session(session) {}
	~DebugOutputBuffer()
	{
		srcStream.flush();
		srcStream.rdbuf(originalBuffer);
	}

	int sync()
	{
		// Send buffer to debug client
		dap::OutputEvent event;
		event.category = &srcStream == &std::cerr ? "stderr" : "stdout";
		event.output = str();
// event.line = 10; //DELETE
// event.source = dap::Source(); //DELETE
// event.source->path = "/home/sepp/Development/minc/paws/example11.minc"; //DELETE
		session->send(event);

		// Clear buffer
		str("");

		return 0;
	}

	std::streamsize xsputn(const char* s, std::streamsize n)
	{
		std::streamsize ss = std::stringbuf::xsputn(s, n);
		if (strchr(s, '\n'))
			srcStream.flush();
		return ss;
	}
};

class Debugger
{
public:
	enum StepType { Run, Pause, StepInitial, StepIn, StepOut, StepOver };
	enum StopEventReason { BreakpointHit, Stepped, Paused, Exception, PauseOnEntry };

	const std::unique_ptr<dap::Session> session;
	StepType stepType;
	bool traceAnonymousBlocks;

	struct StackFrame : dap::StackFrame
	{
		const BlockExprAST* const block;

		StackFrame(const BlockExprAST* block, int id) : block(block)
		{
			name = getBlockExprASTName(block);
			if (name.empty())
				name = "Anonymous Block";

			this->id = id;

			source = dap::Source();
			source->path = getExprFilename((ExprAST*)block);
		}
	};
	struct Thread
	{
		int id;
		std::vector<StackFrame> callStack;
		size_t prevStackDepth;

		Thread(int id) : id(id), prevStackDepth(0) {}
	};

private:
	BlockExprAST* const rootBlock;
	std::shared_ptr<dap::Writer> log;
	std::mutex mutex;
	int line = 1;
	std::map<std::string, std::unordered_set<int>> breakpoints;
	Event configured, resume, terminate;

	std::vector<Thread*> threads;
	std::map<std::thread::id, Thread*> threadIdMap;
	void createThread()
	{
		threads.push_back(new Thread((int)threads.size()));
		threadIdMap[std::this_thread::get_id()] = threads.back();
	}
	void removeAllThreads()
	{
		for (Thread* thread: threads)
			delete thread;
		threads.clear();
		threadIdMap.clear();
	}
	Thread& getCurrentThread()
	{
		return *threadIdMap[std::this_thread::get_id()];
	}
	Thread& getOrCreateCurrentThread(bool& threadIsNew)
	{
		auto thread_id = std::this_thread::get_id();
		auto pair = threadIdMap.find(thread_id);
		threadIsNew = pair == threadIdMap.end();
		if (threadIsNew)
		{
			threads.push_back(new Thread((int)threads.size()));
			threadIdMap[thread_id] = threads.back();
			return *threads.back();
		}
		else
			return *pair->second;
	}

public:
	Debugger(BlockExprAST* rootBlock, std::shared_ptr<dap::Writer> log)
		: session(dap::Session::create()), rootBlock(rootBlock), stepType(StepType::Run), traceAnonymousBlocks(false), log(log)
	{
		registerStepEventListener([](const ExprAST* loc, StepEventType type, void* eventArgs) { ((Debugger*)eventArgs)->onStep(loc, type); }, this);
		registerHandler(&Debugger::onInitialize);
		registerSentHandler(&Debugger::onInitializeResponse);
		registerHandler(&Debugger::onConfigurationDoneRequest);
		registerHandler(&Debugger::onLaunchRequest);
		registerHandler(&Debugger::onThreadsRequest);
		registerHandler(&Debugger::onStackTraceRequest);
		registerHandler(&Debugger::onScopeRequest);
		registerHandler(&Debugger::onVariablesRequest);
		registerHandler(&Debugger::onSetBreakpointsRequest);
		registerHandler(&Debugger::onSetExceptionBreakpointsRequest);
		registerHandler(&Debugger::onPauseRequest);
		registerHandler(&Debugger::onContinueRequest);
		registerHandler(&Debugger::onNextRequest);
		registerHandler(&Debugger::onStepInRequest);
		registerHandler(&Debugger::onStepOutRequest);
		registerHandler(&Debugger::onDisconnectRequest);

		session->onError([&](const char* msg) {
			if (log)
			{
				dap::writef(log, "dap::Session error: %s\n", msg);
				log->close();
			}
			terminate.fire();
		});

		// Create first thread
		createThread();
	}

	int run()
	{
		// Make sure configuration has finished
		configured.wait();

		// Reset threads
		removeAllThreads();
		createThread();

#ifdef DEBUG_MULTITHREADING
const char* path = "/home/sepp/Development/minc/paws/example11.minc";
BlockExprAST* rootBlock2;
std::ifstream in(path);
if (!in.good())
{
	std::cerr << "\e[31merror:\e[0m " << std::string(path) << ": No such file or directory\n";
	return -1;
}
CLexer lexer(&in, &std::cout);
yy::CParser parser(lexer, path, &rootBlock2);
if (parser.parse())
{
	in.close();
	return -1;
}
MINC_PACKAGE_MANAGER().import(rootBlock2); // Import package manager
auto t = std::thread([](Debugger* debugger, BlockExprAST* rootBlock2) {
	try {
		codegenExpr((ExprAST*)rootBlock2, nullptr);
		debugger->session->send(dap::TerminatedEvent());
	} catch (ExitException err) {
		debugger->session->send(dap::TerminatedEvent());
	} catch (CompileError err) {
		StackFrame& top = debugger->getCurrentThread().callStack.back();
		top.line = err.loc.begin_line;
		top.column = err.loc.begin_col;
		top.endLine = err.loc.end_line;
		top.endColumn = err.loc.end_col;
		debugger->sendStopEvent(StopEventReason::Exception, err.msg);
	}
}, this, rootBlock2);
#endif

		int result = 0;
		try {
			MINC_PACKAGE_MANAGER().import(rootBlock); // Import package manager
			codegenExpr((ExprAST*)rootBlock, nullptr);
			session->send(dap::TerminatedEvent());
		} catch (ExitException err) {
			result = err.code;
			session->send(dap::TerminatedEvent());
		} catch (CompileError err) {
			StackFrame& top = getCurrentThread().callStack.back();
			top.line = err.loc.begin_line;
			top.column = err.loc.begin_col;
			top.endLine = err.loc.end_line;
			top.endColumn = err.loc.end_col;
			sendStopEvent(StopEventReason::Exception, err.msg);
		}

#ifdef DEBUG_MULTITHREADING
t.join();
#endif

		// Wait for the debug client to quit
		terminate.wait();
		return result;
	}

	void bind(const std::shared_ptr<dap::Reader>& r, const std::shared_ptr<dap::Writer>& w)
	{
		session->bind(r, w);
	}
	void bind(const std::shared_ptr<dap::ReaderWriter>& rw)
	{
		session->bind(rw);
	}

	// >>> MINC EVENT HANDLERS

private:
	void onStep(const ExprAST* loc, StepEventType type)
	{
		std::unique_lock<std::mutex> lock(mutex);

		// Get current thread or create new thread if a new thread_id is encountered
		bool threadIsNew;
		Thread& currentThread = getOrCreateCurrentThread(threadIsNew);
		if (threadIsNew)
		{
			// Notify the client that a new thread has started
			dap::ThreadEvent event;
			event.reason = "started";
			event.threadId = currentThread.id;
			session->send(event);
		}

		// Get thread call stack
		std::vector<StackFrame>& callStack = currentThread.callStack;


#ifdef DEBUG_STEP_EVENTS
		switch (type)
		{
		case STEP_IN: dap::writef(log, "STEP INTO"); break;
		case STEP_OUT: dap::writef(log, "STEP OUT OF"); break;
		case STEP_SUSPEND: dap::writef(log, "SUSPEND"); break;
		case STEP_RESUME: dap::writef(log, "RESUME"); break;
		}

		if (ExprASTIsBlock(loc)) dap::writef(log, " BLOCK");
		else if (ExprASTIsStmt(loc)) dap::writef(log, " STMT");
		else dap::writef(log, " EXPR");

		if (getExprLine(loc) == getExprEndLine(loc)) dap::writef(log, " %i\n", getExprLine(loc));
		else dap::writef(log, " %i...%i\n", getExprLine(loc), getExprEndLine(loc));
#endif

		if (ExprASTIsBlock(loc))
		{
			// Skip anonymous blocks if they are disabled, unless this is a root block
			if (!traceAnonymousBlocks && !callStack.empty() && getBlockExprASTName((BlockExprAST*)loc).empty())
				return;

			switch (type)
			{
			case STEP_IN:
			case STEP_RESUME:
				callStack.push_back(StackFrame((BlockExprAST*)loc, (int)(callStack.size() | 0x10000 * currentThread.id)));
				break;

			case STEP_OUT:
			case STEP_SUSPEND:
				callStack.pop_back();
				break;
			}
		}
		else if (ExprASTIsStmt(loc))
		{
			assert(!callStack.empty());
			StackFrame& top = callStack.back();
			int stackDelta = (int)callStack.size() - (int)currentThread.prevStackDepth;
			bool paused = true;
			switch (type)
			{
			case STEP_IN:
				top.line = getExprLine(loc);
				top.column = getExprColumn(loc);
				top.endLine = getExprEndLine(loc);
				top.endColumn = getExprEndColumn(loc);

				if (breakpoints[top.source->path.value()].count(top.line))
					sendStopEvent(StopEventReason::BreakpointHit, "");
				else if (stepType == StepType::StepIn && stackDelta >= 0)
					sendStopEvent(StopEventReason::Stepped, "");
				else if (stepType == StepType::StepOut && stackDelta < 0)
					sendStopEvent(StopEventReason::Stepped, "");
				else if (stepType == StepType::StepOver && stackDelta <= 0)
					sendStopEvent(StopEventReason::Stepped, "");
				else if (stepType == StepType::StepInitial)
				{
					stepType == StepType::Run;
					sendStopEvent(StopEventReason::PauseOnEntry, "");
				}
				else
					paused = false;

				// Remember current stack depth
				// StepOver and StepOut compute currentThread.prevStackDepth relative to the stack depth during the last pause
				if (paused || (stepType != StepType::StepOver && stepType != StepType::StepOut))
					currentThread.prevStackDepth = callStack.size();

				break;

			case STEP_RESUME:
				top.line = getExprLine(loc);
				top.column = getExprColumn(loc);
				top.endLine = getExprEndLine(loc);
				top.endColumn = getExprEndColumn(loc);
				break;
			}
		}
	}

	// >>> DEBUG CLIENT EVENT HANDLERS

	template<class R, class P0> void registerHandler(R (Debugger::*handler)(P0))
	{
		session->registerHandler([this, handler](P0 p0) -> R {
			return (*this.*handler)(p0);
		});
	}
	template<class R, class P0> void registerSentHandler(R (Debugger::*handler)(P0))
	{
		session->registerSentHandler([this, handler](P0 p0) -> R {
			return (*this.*handler)(p0);
		});
	}

	dap::InitializeResponse onInitialize(const dap::InitializeRequest& request)
	{
		dap::InitializeResponse response;
		response.supportsConfigurationDoneRequest = true;
		return response;
	}

	void onInitializeResponse(const dap::ResponseOrError<dap::InitializeResponse>& response)
	{
		session->send(dap::InitializedEvent());
	}

	dap::ConfigurationDoneResponse onConfigurationDoneRequest(const dap::ConfigurationDoneRequest&)
	{
		configured.fire();
		return dap::ConfigurationDoneResponse();
	}

	dap::LaunchResponse onLaunchRequest(const dap::MincLaunchRequest& request)
	{
		if (request.stopOnEntry.has_value() && request.stopOnEntry.value())
			stepType = Debugger::StepType::StepInitial;
		if (request.args.has_value())
		{
			auto args = request.args.value();
			int argc;
			char** argv;
			getCommandLineArgs(&argc, &argv);
			char* programName = argv[0];
			argc = args.size() + 1;
			argv = new char*[argc];
			argv[0] = programName;
			for (size_t i = 0; i < args.size(); ++i)
			{
				argv[i + 1] = new char[args[i].size() + 1];
				strcpy(argv[i + 1], args[i].c_str());
			}
			setCommandLineArgs(argc, argv);
		}
		if (request.cwd.has_value())
			chdir(request.cwd.value().c_str());
		traceAnonymousBlocks = request.traceAnonymousBlocks.has_value() && request.traceAnonymousBlocks.value();
		return dap::LaunchResponse();
	}

	dap::ThreadsResponse onThreadsRequest(const dap::ThreadsRequest& request)
	{
		dap::ThreadsResponse response;
		for (size_t i = 0; i < threads.size(); ++i)
		{
			dap::Thread thread;
			thread.id = i;
			thread.name = "Thread " + std::to_string(i);
			response.threads.push_back(thread);
		}
		return response;
	}

	dap::ResponseOrError<dap::StackTraceResponse> onStackTraceRequest(const dap::StackTraceRequest& request)
	{
		// Verify thread id
		if (request.threadId >= threads.size())
			return dap::Error("Unknown threadId '%d'", int(request.threadId));

		// Get thread call stack
		std::vector<StackFrame>& callStack = threads[request.threadId]->callStack;

		dap::StackTraceResponse response;

		for (auto frame = callStack.crbegin(); frame != callStack.crend(); ++frame)
			response.stackFrames.push_back(*frame);
		response.totalFrames = (int)callStack.size();
		return response;
	}

	dap::ResponseOrError<dap::ScopesResponse> onScopeRequest(const dap::ScopesRequest& request)
	{
		int threadId = request.frameId >> 16, frameId = request.frameId & 0xFFFF;
		if (threadId >= threads.size())
			return dap::Error("Unknown frameId '%d'", int(request.frameId));
		std::vector<StackFrame>& callStack = threads[threadId]->callStack;
		if (frameId >= (int)callStack.size())
			return dap::Error("Unknown frameId '%d'", int(request.frameId));

		const BlockExprAST* const block = callStack[frameId].block;
		dap::ScopesResponse response;
		dap::Scope scope;

		scope.name = "Locals";
		scope.presentationHint = "locals";
		scope.variablesReference = 0x1000 + request.frameId * 4 + 0;
		scope.namedVariables = (int)countBlockExprASTSymbols(block);
		response.scopes.push_back(scope);

		scope.name = "Statements";
		scope.presentationHint = "locals";
		scope.variablesReference = 0x1000 + request.frameId * 4 + 1;
		scope.namedVariables = (int)countBlockExprASTStmts(block);
		response.scopes.push_back(scope);

		scope.name = "Expressions";
		scope.presentationHint = "locals";
		scope.variablesReference = 0x1000 + request.frameId * 4 + 2;
		scope.namedVariables = (int)countBlockExprASTExprs(block);
		response.scopes.push_back(scope);

		return response;
	}

	dap::ResponseOrError<dap::VariablesResponse> onVariablesRequest(const dap::VariablesRequest& request)
	{
		int threadId = request.variablesReference >> 16, frameId = ((request.variablesReference & 0xFFFF) - 0x1000) >> 2;
		if (threadId >= threads.size())
			return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
		std::vector<StackFrame>& callStack = threads[threadId]->callStack;
		if (frameId >= (int)callStack.size())
			return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));

		dap::VariablesResponse response;

		switch (request.variablesReference & 0x3)
		{
		case 0:
			{
				auto cbk = [&](const std::string& name, const Variable& symbol) {
					dap::Variable var;
					var.name = name;
					if (!getValueStr(symbol.value, &var.value))
						var.value = "UNKNOWN";
					var.type = getTypeName(symbol.type);
					response.variables.push_back(var);
				};
				for (const BlockExprAST* block = callStack[frameId].block; block != nullptr; block = getBlockExprASTParent(block))
				{
					iterateBlockExprASTSymbols(block, cbk);
					for (const BlockExprAST* ref: getBlockExprASTReferences(block))
						iterateBlockExprASTSymbols(ref, cbk);
				}
			}
			break;

		case 1:
			{
				auto cbk = [&](const ExprListAST* tplt, const CodegenContext* stmt) {
					dap::Variable var;
					var.name = ExprASTToShortString(tplt);
					var.value = "TODO";
					response.variables.push_back(var);
				};
				for (const BlockExprAST* block = callStack[frameId].block; block != nullptr; block = getBlockExprASTParent(block))
				{
					iterateBlockExprASTStmts(block, cbk);
					for (const BlockExprAST* ref: getBlockExprASTReferences(block))
						iterateBlockExprASTStmts(ref, cbk);
				}
			}
			break;

		case 2:
			{
				auto cbk = [&](const ExprAST* tplt, const CodegenContext* expr) {
					dap::Variable var;
					var.name = ExprASTToShortString(tplt);
					var.value = "TODO";
					response.variables.push_back(var);
				};
				for (const BlockExprAST* block = callStack[frameId].block; block != nullptr; block = getBlockExprASTParent(block))
				{
					iterateBlockExprASTExprs(block, cbk);
					for (const BlockExprAST* ref: getBlockExprASTReferences(block))
						iterateBlockExprASTExprs(ref, cbk);
				}
			}
			break;
		}

		return response;
	}

	dap::SetBreakpointsResponse onSetBreakpointsRequest(const dap::SetBreakpointsRequest& request)
	{
		dap::SetBreakpointsResponse response;
		std::unique_lock<std::mutex> lock(mutex);
		std::unordered_set<int>& sourceBreakpoints = this->breakpoints.insert({ request.source.path.value(), {} }).first->second;

		auto breakpoints = request.breakpoints.value({});
		sourceBreakpoints.clear();
		response.breakpoints.resize(breakpoints.size());
		for (size_t i = 0; i < breakpoints.size(); i++)
		{
			dap::writef(log, "addBreakpoint(%i)\n", (int)i);
			sourceBreakpoints.emplace(breakpoints[i].line);
			response.breakpoints[i].verified = true;
		}

		return response;
	}

	dap::SetExceptionBreakpointsResponse onSetExceptionBreakpointsRequest (const dap::SetExceptionBreakpointsRequest&)
	{
		return dap::SetExceptionBreakpointsResponse();
	}

	dap::PauseResponse onPauseRequest (const dap::PauseRequest&)
	{
		stepType = Debugger::StepType::Pause;
		return dap::PauseResponse();
	}

	dap::ContinueResponse onContinueRequest (const dap::ContinueRequest&)
	{
		stepType = Debugger::StepType::Run;
		resume.fire();
		return dap::ContinueResponse();
	}

	dap::NextResponse onNextRequest (const dap::NextRequest&)
	{
		stepType = Debugger::StepType::StepOver;
		resume.fire();
		return dap::NextResponse();
	}

	dap::StepInResponse onStepInRequest (const dap::StepInRequest&)
	{
		stepType = Debugger::StepType::StepIn;
		resume.fire();
		return dap::StepInResponse();
	}

	dap::StepOutResponse onStepOutRequest (const dap::StepOutRequest&)
	{
		stepType = Debugger::StepType::StepOut;
		resume.fire();
		return dap::StepOutResponse();
	}

	dap::DisconnectResponse onDisconnectRequest(const dap::DisconnectRequest& request)
	{
		if (request.terminateDebuggee.value(false))
			terminate.fire();
		return dap::DisconnectResponse();
	}

	// >>> DEBUG CLIENT INVOKERS

	void sendStopEvent(StopEventReason reason, std::string description="")
	{
		switch (reason)
		{
		case StopEventReason::Stepped:
			{
				// The debugger has single-line stepped. Inform the client.
				dap::StoppedEvent event;
				event.reason = "step";
				event.threadId = getCurrentThread().id;
				session->send(event);
				resume.wait(true);
				break;
			}
		case StopEventReason::BreakpointHit:
			{
				// The debugger has hit a breakpoint. Inform the client.
				dap::StoppedEvent event;
				event.reason = "breakpoint";
				event.threadId = getCurrentThread().id;
				session->send(event);
				resume.wait(true);
				break;
			}
		case StopEventReason::Paused:
			{
				// The debugger has been suspended. Inform the client.
				dap::StoppedEvent event;
				event.reason = "pause";
				event.threadId = getCurrentThread().id;
				session->send(event);
				resume.wait(true);
				break;
			}
		case StopEventReason::Exception:
			{
				// The debugger has caught an exception. Inform the client.
				dap::StoppedEvent event;
				event.reason = "exception";
				event.threadId = getCurrentThread().id;
				event.text = description;
				session->send(event);
				resume.wait(true);
				break;
			}
		case StopEventReason::PauseOnEntry:
			{
				// The debugger has been suspended. Inform the client.
				dap::StoppedEvent event;
				event.reason = "entry";
				event.threadId = getCurrentThread().id;
				session->send(event);
				resume.wait(true);
				break;
			}
		}
	}
};

#include "paws_types.h" //DELETE

int launchDebugClient(BlockExprAST* rootBlock)
{
#ifdef OS_WINDOWS
  // Change stdin & stdout from text mode to binary mode.
  // This ensures sequences of \r\n are not changed to \n.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif  // OS_WINDOWS

	std::shared_ptr<dap::Writer> log;
#ifdef LOG_TO_FILE
	log = dap::file(LOG_TO_FILE);
#endif

	Debugger debugger(rootBlock, log);
	auto session = debugger.session.get();

	DebugOutputBuffer redirectStdout(std::cout, session), redirectStderr(std::cerr, session);

	std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
	std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
	if (log)
		debugger.bind(spy(in, log), spy(out, log));
	else
		debugger.bind(in, out);

	return debugger.run();
}