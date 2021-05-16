// STD
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>

// LspCpp
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/general/initialized.h"
#include "LibLsp/lsp/general/shutdown.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/textDocument/code_lens.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/textDocument/did_close.h"
#include "LibLsp/lsp/textDocument/did_save.h"
#include "LibLsp/lsp/textDocument/highlight.h"
#include "LibLsp/lsp/textDocument/references.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "LibLsp/lsp/textDocument/semanticTokens.h"
#include "LibLsp/lsp/windows/MessageNotify.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/stream.h"
#include "LibLsp/JsonRpc/TcpServer.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/workspace/execute_command.h"
#include "network/uri.hpp"

// Local includes
#include "minc_api.hpp"
#include "minc_pkgmgr.h"


// Make lsDocumentUri usable as key type for std::map
bool operator<(const lsDocumentUri& lhs, const lsDocumentUri& rhs)
{
	return lhs.raw_uri_.compare(rhs.raw_uri_) < 0;
}

const std::string MINC_SERVER_ADDRESS = "127.0.0.1";
const std::string MINC_SERVER_PORT = "9333"; //TODO: Make configurable via command line

class SymbolFinder
{
	int line, column;
	const MincExpr* expr;
	const MincBlockExpr* exprScope;

public:
	const MincExpr* find(const MincBlockExpr* scope, const lsPosition& pos, const MincBlockExpr** exprScope=nullptr)
	{
		this->line = pos.line + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->column = pos.character + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->expr = nullptr;
		this->exprScope = nullptr;

		if (match(scope))
		{
			if (exprScope != nullptr) *exprScope = this->exprScope;
			return expr;
		}
		else
		{
			if (exprScope != nullptr) *exprScope = nullptr;
			return nullptr;
		}
	}

private:
	bool match(const MincExpr* expr)
	{
		// Hit-test expr
		if (expr->exprtype == MincExpr::ExprType::LIST)
		{
			MincExpr* front = ((MincListExpr*)expr)->exprs.front();
			while (front->exprtype == MincExpr::ExprType::LIST)
				front = ((MincListExpr*)front)->exprs.front();

			MincExpr* back = ((MincListExpr*)expr)->exprs.back();
			while (back->exprtype == MincExpr::ExprType::LIST)
				back = ((MincListExpr*)back)->exprs.back();

			if ((line < (int)front->loc.begin_line || (line == (int)front->loc.begin_line && column < (int)front->loc.begin_column)) || // If pos < expr start
				(line > (int)back->loc.end_line    || (line == (int)back->loc.end_line    && column > (int)back->loc.end_column))) // If pos > expr end
				return false;
		}
		else if((line < (int)expr->loc.begin_line || (line == (int)expr->loc.begin_line && column < (int)expr->loc.begin_column)) || // If pos < expr start
				(line > (int)expr->loc.end_line   || (line == (int)expr->loc.end_line   && column > (int)expr->loc.end_column))) // If pos > expr end
			return false;

		// Set hit expr
		this->expr = expr;

		// Recurse into sub-exprs
		switch (expr->exprtype)
		{
		case MincExpr::ExprType::STMT:
			//TODO: Replace linear search with binary search
			for (MincExprIter subExpr = ((MincStmt*)expr)->begin; subExpr != ((MincStmt*)expr)->end; ++subExpr)
				if (match(*subExpr)) return true;
			return true;
		case MincExpr::ExprType::LIST:
			//TODO: Replace linear search with binary search
			for (MincExpr* subExpr: ((MincListExpr*)expr)->exprs)
				if (match(subExpr)) return true;
			return true;
		case MincExpr::ExprType::CAST:
			return match(((MincCastExpr*)expr)->getSourceExpr());
		case MincExpr::ExprType::ARGOP:
			if (match(((MincArgOpExpr*)expr)->var)) return true;
			return match(((MincArgOpExpr*)expr)->args);
		case MincExpr::ExprType::ENCOP:
			return match(((MincArgOpExpr*)expr)->var);
		case MincExpr::ExprType::TEROP:
			if (match(((MincTerOpExpr*)expr)->a)) return true;
			if (match(((MincTerOpExpr*)expr)->b)) return true;
			return match(((MincTerOpExpr*)expr)->c);
		case MincExpr::ExprType::BINOP:
			if (match(((MincBinOpExpr*)expr)->a)) return true;
			return match(((MincBinOpExpr*)expr)->b);
		case MincExpr::ExprType::VARBINOP:
			return match(((MincVarBinOpExpr*)expr)->a);
		case MincExpr::ExprType::PREOP:
			return match(((MincPrefixExpr*)expr)->a);
		case MincExpr::ExprType::POSTOP:
			return match(((MincPostfixExpr*)expr)->a);
		case MincExpr::ExprType::BLOCK:
			{
				const MincBlockExpr* prevExprScope = exprScope;
				exprScope = (MincBlockExpr*)expr;
				//TODO: Replace linear search with binary search
				for (const MincStmt* stmt: ((MincBlockExpr*)expr)->builtStmts)
					if (match(stmt)) return true;
				exprScope = prevExprScope; // Reset exprScope to hierarchical parent if expr == exprScope
			}
		default:
			return true;
		}
	}
};

class SemantikTokenFinder
{
	std::vector<unsigned>* data;
	int begin_line, end_line, begin_column, end_column;
	unsigned prevLine, prevChar;

public:
	void find(const MincBlockExpr* scope, std::vector<unsigned>& data, const lsRange& range)
	{
		this->data = &data;
		this->begin_line = range.start.line + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->end_line = range.end.line + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->begin_column = range.start.character + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->end_column = range.end.character + 1; // Convert from language server coordinates (0 based) to min coordinates (1 based)
		this->prevLine = 0;
		this->prevChar = 0;

		//TODO: Only find tokens within range (maybe use SymbolFinder to pin down range)

		match(scope);
	}

private:
	void match(const MincExpr* expr)
	{
		switch (expr->exprtype)
		{
		case MincExpr::ExprType::ID:
			if (expr->resolvedKernel == &UNUSED_KERNEL)
			{
				if (expr->loc.begin_line != expr->loc.end_line)
					break; // Multiline keywords not supported

				const unsigned line = expr->loc.begin_line - 1;
				const unsigned startChar = expr->loc.begin_column - 1;
				const unsigned length = expr->loc.end_column - expr->loc.begin_column;
				const unsigned deltaLine = line - prevLine;
				const unsigned deltaStartChar = deltaLine == 0 ? startChar - prevChar : startChar;
				const unsigned tokenType = 0; // keyword
				const unsigned tokenModifiers = 0; // none
				prevLine = line;
				prevChar = startChar;
				data->push_back(deltaLine); // line
				data->push_back(deltaStartChar); // startChar
				data->push_back(length); // length
				data->push_back(tokenType); // tokenType
				data->push_back(tokenModifiers); // tokenModifiers
			}
			break;
		case MincExpr::ExprType::STMT:
			//TODO: Replace linear search with binary search
			for (MincExprIter subExpr = ((MincStmt*)expr)->begin; subExpr != ((MincStmt*)expr)->end; ++subExpr)
				match(*subExpr);
			break;
		case MincExpr::ExprType::LIST:
			//TODO: Replace linear search with binary search
			for (MincExpr* subExpr: ((MincListExpr*)expr)->exprs)
				match(subExpr);
			break;
		case MincExpr::ExprType::CAST:
			match(((MincCastExpr*)expr)->getSourceExpr());
			break;
		case MincExpr::ExprType::ARGOP:
			match(((MincArgOpExpr*)expr)->var);
			match(((MincArgOpExpr*)expr)->args);
			break;
		case MincExpr::ExprType::ENCOP:
			match(((MincArgOpExpr*)expr)->var);
			break;
		case MincExpr::ExprType::TEROP:
			match(((MincTerOpExpr*)expr)->a);
			match(((MincTerOpExpr*)expr)->b);
			match(((MincTerOpExpr*)expr)->c);
			break;
		case MincExpr::ExprType::BINOP:
			match(((MincBinOpExpr*)expr)->a);
			match(((MincBinOpExpr*)expr)->b);
			break;
		case MincExpr::ExprType::VARBINOP:
			match(((MincVarBinOpExpr*)expr)->a);
			break;
		case MincExpr::ExprType::PREOP:
			match(((MincPrefixExpr*)expr)->a);
			break;
		case MincExpr::ExprType::POSTOP:
			match(((MincPostfixExpr*)expr)->a);
			break;
		case MincExpr::ExprType::BLOCK:
			for (const MincStmt* stmt: ((MincBlockExpr*)expr)->builtStmts)
				match(stmt);
			break;
		default:
			break;
		}
	}
};

class Document
{
	std::string path, content;
	MincBlockExpr* rootBlock;
	std::mutex rootBlockMutex; // Locked for the duration of rootBlock access to avoid deletion or reassignment
	std::mutex rootBlockBuildMutex; // Locked while rootBlock is being rebuild. Dependent processes can wait for an in-progress build by locking this
	SymbolFinder symbolFinder;
	SemantikTokenFinder semantikTokenFinder;

public:
	Document(const std::string& path, const std::string& content) : path(path), content(content), rootBlock(nullptr) {}
	~Document()
	{
		rootBlockMutex.lock(); // Lock while rootBlock is being deleted
		if (rootBlock != nullptr)
			delete rootBlock;
		rootBlock = nullptr;
		rootBlockMutex.unlock();
	}
	static Document* fromUri(const lsDocumentUri& uri)
	{
		Document* document = new Document(uri.GetRawPath(), "");
		// Open source file
		std::ifstream fileStream(document->path);
		if (!fileStream.good())
		{
			delete document;
			return nullptr; //TODO: Notify error `filename + ": No such file or directory"` instead
		}

		// Read entire file to inMemoryFiles[uri]
		std::stringstream stream;
		stream << fileStream.rdbuf();
		document->content = stream.str(); //TODO: Avoid double-copy

		// Close source file
		fileStream.close();
		return document;
	}

	void applyChanges(const std::vector<lsTextDocumentContentChangeEvent>& contentChanges)
	{
		for (auto change: contentChanges)
		{
			const char* c = content.c_str();
			for (int line = change.range->start.line; line != 0; ++c)
				if (*c == '\n')
					--line;
			size_t start = c - content.c_str() + change.range->start.character;
			for (int line = change.range->end.line - change.range->start.line; line != 0; ++c)
				if (*c == '\n')
					--line;
			size_t end = c - content.c_str() + change.range->end.character;
			content.erase(start, end - start);
			content.insert(start, change.text);
			std::cout << "changed " << path << ":" << change.range->start.line << "\n";
		}
	}

	bool validate(std::vector<lsDiagnostic>& diagnostics)
	{
		std::unique_lock<std::mutex> lock(rootBlockBuildMutex); // Lock for the duration of the rebuild
		MincBlockExpr* validationRootBlock = nullptr;

		// Set current working directory to directory of document
		//TODO: Consider setting to workspace directory (passes during initialization as `rootPath`)
		const char* lastSlashPos = std::max(strrchr(path.c_str(), '/'), strrchr(path.c_str(), '\\'));
		if (lastSlashPos != nullptr)
			chdir(path.substr(0, lastSlashPos - path.c_str()).c_str());

		MincBuildtime buildtime;
		buildtime.settings.debug = true;
		buildtime.settings.maxErrors = 100;
		try {
			// Parse source code into AST
			std::istringstream fileStream(content);
			validationRootBlock = MincBlockExpr::parseStream(fileStream, MincBlockExpr::flavorFromFile(path));

			// Name root block
			std::string rootBlockName = std::max(path.c_str(), lastSlashPos + 1);
			const size_t dt = rootBlockName.rfind(".");
			if (dt != std::string::npos) rootBlockName = rootBlockName.substr(0, dt);
			validationRootBlock->name = rootBlockName;

			// Build root block
			MINC_PACKAGE_MANAGER().import(validationRootBlock); // Import package manager
			validationRootBlock->build(buildtime);
		} catch (const TooManyErrorsException& err) {
			// Ignore TooManyErrorsException
		} catch (const MincException& err) {
			lsRange range;
			if (err.loc.begin_line == 0)
			{
				range.start = lsPosition(0, 0);
				range.end = lsPosition(0, 1);
			}
			else
			{
				range.start = lsPosition(err.loc.begin_line - 1, err.loc.begin_column - 1);
				range.end = lsPosition(err.loc.end_line - 1, err.loc.end_column - 1);
			}
			diagnostics.push_back(lsDiagnostic{
				range,
				lsDiagnosticSeverity::Error,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				err.what(),
				{},
				std::nullopt,
				std::nullopt
			});
		}

		rootBlockMutex.lock(); // Lock while rootBlock is being deleted and reassigned
		if (rootBlock != nullptr)
			delete rootBlock;
		rootBlock = validationRootBlock;
		rootBlockMutex.unlock();

		for (const CompileError& err: buildtime.outputs.errors)
		{
			lsRange range;
			if (err.loc.begin_line == 0 || err.loc.filename == nullptr)
			{
				range.start = lsPosition(0, 0);
				range.end = lsPosition(0, 1);
			}
			else
			{
				range.start = lsPosition(err.loc.begin_line - 1, err.loc.begin_column - 1);
				range.end = lsPosition(err.loc.end_line - 1, err.loc.end_column - 1);
			}
			diagnostics.push_back(lsDiagnostic{
				range,
				lsDiagnosticSeverity::Error,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				err.what(),
				{},
				std::nullopt,
				std::nullopt
			});
		}
		return diagnostics.size() != 0;
	}

	bool highlightReferences(const lsPosition& pos, std::vector<lsDocumentHighlight>& highlights)
	{
		if (rootBlock == nullptr) // If rootBlock is not built or built with syntax errors
		{
			// Await in-progress build (if any)
			rootBlockBuildMutex.lock();
			rootBlockBuildMutex.unlock();
		}
		std::unique_lock<std::mutex> lock(rootBlockMutex); // Prohibit updates to rootBlock for the rest of this method
		if (rootBlock == nullptr) // If rootBlock is not built or built with syntax errors
			return false; // Give up

		const MincBlockExpr* exprScope;
		const MincExpr* expr = symbolFinder.find(rootBlock, pos, &exprScope);
		if (expr == nullptr || expr->exprtype != MincExpr::ExprType::ID)
			return false;

		const MincSymbol* symbol = exprScope->lookupSymbol(((MincIdExpr*)expr)->name);
		if (symbol != nullptr)
		{
			highlights.push_back(lsDocumentHighlight{
				lsRange(lsPosition(expr->loc.begin_line - 1, expr->loc.begin_column - 1), lsPosition(expr->loc.end_line - 1, expr->loc.end_column - 1)),
				lsDocumentHighlightKind::Text
			});
			return true;
		}

		const MincStackSymbol* stackSymbol = exprScope->lookupStackSymbol(((MincIdExpr*)expr)->name);
		if (stackSymbol != nullptr)
		{
			highlights.push_back(lsDocumentHighlight{
				lsRange(lsPosition(expr->loc.begin_line - 1, expr->loc.begin_column - 1), lsPosition(expr->loc.end_line - 1, expr->loc.end_column - 1)),
				lsDocumentHighlightKind::Text
			});
			return true;
		}

		return false;
	}

	bool resolveSemanticTokens(std::vector<unsigned>& data, const lsRange& range)
	{
		if (rootBlock == nullptr) // If rootBlock is not built or built with syntax errors
		{
			// Await in-progress build (if any)
			rootBlockBuildMutex.lock();
			rootBlockBuildMutex.unlock();
		}
		std::unique_lock<std::mutex> lock(rootBlockMutex); // Prohibit updates to rootBlock for the rest of this method
		if (rootBlock == nullptr) // If rootBlock is not built or built with syntax errors
			return false; // Give up

		semantikTokenFinder.find(rootBlock, data, range);
		return true;
	}
	bool resolveSemanticTokens(std::vector<unsigned>& data)
	{
		return resolveSemanticTokens(data, lsRange(lsPosition(0, 0), lsPosition(0x7FFFFFFF, 0x7FFFFFFF)));
	}
};

class Server
{
	lsp::ProtocolJsonHandler  protocol_json_handler;
	struct Log :public lsp::Log
	{
		void log(Level level, std::wstring&& msg)
		{
			std::cout << msg.c_str() << "\n";
		};
		void log(Level level, const std::wstring& msg)
		{
			std::cout << msg.c_str() << "\n";
		};
		void log(Level level, std::string&& msg)
		{
			std::cout << msg.c_str() << "\n";
		};
		void log(Level level, const std::string& msg)
		{
			std::cout << msg.c_str() << "\n";
		};
	} log;
	GenericEndpoint endpoint;
	lsp::TcpServer server;

	std::map<lsDocumentUri, Document*> inMemoryFiles;

public:
	Server(): endpoint(log), server(MINC_SERVER_ADDRESS, MINC_SERVER_PORT, protocol_json_handler, endpoint, log)
	{
		endpoint.method2request[td_initialize::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "initialize\n";

			auto req = reinterpret_cast<td_initialize::request*>(msg.get());
			td_initialize::response rsp;
			rsp.id = req->id;

			rsp.result.capabilities.documentHighlightProvider = {true, std::nullopt};
			rsp.result.capabilities.referencesProvider = {true, std::nullopt};

			// CodeLensOptions code_lens_options;
			// code_lens_options.resolveProvider = true;
			// rsp.result.capabilities.codeLensProvider = code_lens_options;

			lsTextDocumentSyncOptions textDocumentSyncOptions;
			textDocumentSyncOptions.openClose = true;
			textDocumentSyncOptions.change = lsTextDocumentSyncKind::Incremental;
			rsp.result.capabilities.textDocumentSync = {lsTextDocumentSyncKind::Incremental, textDocumentSyncOptions};

			SemanticTokensOptions semanticTokensOptions;
			semanticTokensOptions.legend.tokenTypes.push_back("keyword");
			semanticTokensOptions.legend.tokenTypes.push_back("type");
			semanticTokensOptions.legend.tokenTypes.push_back("class");
			semanticTokensOptions.legend.tokenModifiers.push_back("private");
			semanticTokensOptions.legend.tokenModifiers.push_back("static");
			semanticTokensOptions.full = true;
			semanticTokensOptions.range = true;
			rsp.result.capabilities.semanticTokensProvider = semanticTokensOptions;

			server.remote_end_point_->sendResponse(rsp);
			return true;
		};
		endpoint.method2notification[Notify_InitializedNotification::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "initialized\n";

			Notify_ShowMessage::notify notification;
			notification.params.message = "Server initialized";
			notification.params.type = lsMessageType::Info;
			server.remote_end_point_->sendNotification(notification);

			return true;
		};
		endpoint.method2request[td_shutdown::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "td_shutdown\n";

			server.stop();

			return true;
		};
		endpoint.method2request[td_codeLens::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			return true;
		};
		endpoint.method2request[td_highlight::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "td_highlight\n";

			auto req = reinterpret_cast<td_highlight::request*>(msg.get());
			td_highlight::response rsp;
			rsp.id = req->id;

			auto pair = inMemoryFiles.find(req->params.textDocument.uri);
			if (pair == inMemoryFiles.end())
				return false;

			std::vector<lsDocumentHighlight> highlights;
			pair->second->highlightReferences(req->params.position, highlights);
			rsp.result = highlights;
			server.remote_end_point_->sendResponse(rsp);
			return true;
		};
		endpoint.method2request[td_references::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "td_references\n";

			auto req = reinterpret_cast<td_highlight::request*>(msg.get());
			td_highlight::response rsp;
			rsp.id = req->id;

			server.remote_end_point_->sendResponse(rsp);
			return true;
		};
		endpoint.method2request[td_semanticTokensFull::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "td_semanticTokensFull\n";

			auto req = reinterpret_cast<td_semanticTokensFull::request*>(msg.get());
			td_semanticTokensFull::response rsp;
			rsp.id = req->id;

			auto pair = inMemoryFiles.find(req->params.textDocument.uri);
			if (pair == inMemoryFiles.end())
				return false;

			pair->second->resolveSemanticTokens(rsp.result.data);

			server.remote_end_point_->sendResponse(rsp);
			return true;
		};
		endpoint.method2request[td_semanticTokensRange::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "td_semanticTokensRange\n";

			auto req = reinterpret_cast<td_semanticTokensRange::request*>(msg.get());
			td_semanticTokensRange::response rsp;
			rsp.id = req->id;

			auto pair = inMemoryFiles.find(req->params.textDocument.uri);
			if (pair == inMemoryFiles.end())
			{
				return false;
			}

			pair->second->resolveSemanticTokens(rsp.result.data, req->params.range);

			server.remote_end_point_->sendResponse(rsp);
			return true;
		};
		endpoint.method2notification[Notify_TextDocumentDidOpen::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			auto notification = (Notify_TextDocumentDidOpen::notify*)msg.get();
			std::cout << "TextDocumentDidOpen\n";

			Document* document = Document::fromUri(notification->params.textDocument.uri);
			if (document == nullptr)
				return false; //TODO: Notify error `filename + ": No such file or directory"` instead
			inMemoryFiles[notification->params.textDocument.uri] = document;

			Notify_TextDocumentPublishDiagnostics::notify diagnosticsNotification;
			diagnosticsNotification.params.uri = notification->params.textDocument.uri;
			document->validate(diagnosticsNotification.params.diagnostics);

			return true;
		};
		endpoint.method2notification[Notify_TextDocumentDidClose::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			auto notification = (Notify_TextDocumentDidClose::notify*)msg.get();
			std::cout << "TextDocumentDidClose\n";

			auto pair = inMemoryFiles.find(notification->params.textDocument.uri);
			if (pair == inMemoryFiles.end())
				return false;

			delete pair->second;
			inMemoryFiles.erase(pair);

			return true;
		};
		endpoint.method2notification[Notify_TextDocumentDidChange::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			auto notification = (Notify_TextDocumentDidChange::notify*)msg.get();
			std::cout << "TextDocumentDidChange\n";

			auto pair = inMemoryFiles.find(notification->params.textDocument.uri);
			if (pair == inMemoryFiles.end())
				return false;

			pair->second->applyChanges(notification->params.contentChanges);

			Notify_TextDocumentPublishDiagnostics::notify diagnosticsNotification;
			diagnosticsNotification.params.uri = notification->params.textDocument.uri;
			pair->second->validate(diagnosticsNotification.params.diagnostics);

			server.remote_end_point_->sendNotification(diagnosticsNotification);
			return true;
		};
		endpoint.method2notification[Notify_TextDocumentDidSave::kMethodType] = [&](std::unique_ptr<LspMessage> msg)
		{
			std::cout << "TextDocumentDidSave\n";

			return true;
		};
		server.run();
	}
};

int launchLanguageServer(int argc, char** argv)
{
	Server server;

	return 0;
}
