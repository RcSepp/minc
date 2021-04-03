#pragma once

#include "LibLsp/JsonRpc/RequestInMessage.h"
#include "LibLsp/JsonRpc/lsResponseMessage.h"

struct lsSemanticTokensParams {
	/**
	 * The text document.
	 */
	lsTextDocumentIdentifier textDocument;

	MAKE_SWAP_METHOD(lsSemanticTokensParams, textDocument);
};
MAKE_REFLECT_STRUCT(lsSemanticTokensParams, textDocument);

struct lsSemanticTokensRangeParams {
	/**
	 * The text document.
	 */
	lsTextDocumentIdentifier textDocument;

	/**
	 * The range the semantic tokens are requested for.
	 */
	lsRange range;

	MAKE_SWAP_METHOD(lsSemanticTokensRangeParams, textDocument);
};
MAKE_REFLECT_STRUCT(lsSemanticTokensRangeParams, textDocument);




struct lsSemanticTokens {
	/**
	 * An optional result id. If provided and clients support delta updating
	 * the client will include the result id in the next semantic token request.
	 * A server can then instead of computing all semantic tokens again simply
	 * send a delta.
	 */
	std::optional<std::string> resultId;

	/**
	 * The actual tokens.
	 */
	std::vector<unsigned int> data;

	MAKE_SWAP_METHOD(lsSemanticTokens, resultId, data)
};
MAKE_REFLECT_STRUCT(lsSemanticTokens, resultId, data)



/**
 * The request is sent from the client to the server to resolve semantic tokens for a given file. Semantic tokens are
 * used to add additional color information to a file that depends on language specific symbol information. A
 * semantic token request usually produces a large result. The protocol therefore supports encoding tokens with
 * numbers. In addition optional support for deltas is available.
 *
 * Registration Options: SemanticTokensRegistrationOptions
 */
DEFINE_REQUEST_RESPONSE_TYPE(td_semanticTokensFull, lsSemanticTokensParams, lsSemanticTokens);
DEFINE_REQUEST_RESPONSE_TYPE(td_semanticTokensRange, lsSemanticTokensRangeParams, lsSemanticTokens);