#pragma once
#include "lsRange.h"
#include "lsTextEdit.h"
#include "lsDocumentUri.h"

#include "lsResponseError.h"
#include "location_type.h"

#include "LibLsp/JsonRpc/RequestInMessage.h"
#include "LibLsp/JsonRpc/lsResponseMessage.h"

enum class lsDiagnosticSeverity {
  // Reports an error.
  Error = 1,
  // Reports a warning.
  Warning = 2,
  // Reports an information.
  Information = 3,
  // Reports a hint.
  Hint = 4
};
MAKE_REFLECT_TYPE_PROXY(lsDiagnosticSeverity);
enum class DiagnosticTag :uint8_t {

	/**
	 * Unused or unnecessary code.
	 *
	 * Clients are allowed to render diagnostics with this tag faded out instead of having
	 * an error squiggle.
	 */
	Unnecessary=(1),

	/**
	 * Deprecated or obsolete code.
	 *
	 * Clients are allowed to rendered diagnostics with this tag strike through.
	 */
	 Deprecated=(2),
};
MAKE_REFLECT_TYPE_PROXY(DiagnosticTag);



/**
 * Represents a related message and source code location for a diagnostic. This should be
 * used to point to code locations that cause or related to a diagnostics, e.g when duplicating
 * a symbol in a scope.
 *
 * Since 3.7.0
 */

struct DiagnosticRelatedInformation {
	/**
	 * The location of this related diagnostic information.
	 */

    lsLocation location;

	/**
	 * The message of this related diagnostic information.
	 */

	 std::string message;

	 MAKE_SWAP_METHOD(DiagnosticRelatedInformation, location, message)
};
MAKE_REFLECT_STRUCT(DiagnosticRelatedInformation, location, message)

struct DiagnosticCodeDescription {
	std::string href;
	MAKE_SWAP_METHOD(DiagnosticCodeDescription, href)
};
MAKE_REFLECT_STRUCT(DiagnosticCodeDescription, href)
struct lsDiagnostic {
  // The range at which the message applies.
  lsRange range;

  // The diagnostic's severity. Can be omitted. If omitted it is up to the
  // client to interpret diagnostics as error, warning, info or hint.
  optional<lsDiagnosticSeverity> severity;

  // The diagnostic's code. Can be omitted.
  optional<  std::pair<optional<std::string>, optional<int>> >  code;

  optional<DiagnosticCodeDescription> codeDescription;
  // A human-readable string describing the source of this
  // diagnostic, e.g. 'typescript' or 'super lint'.
  optional < std::string >source ;

  // The diagnostic's message.
  std::string message;

  // Non-serialized set of fixits.
  std::vector<lsTextEdit> fixits_;


  optional<std::vector<DiagnosticTag>> tags;
	

  /**
 * An array of related diagnostic information, e.g. when symbol-names within a scope collide
 * all definitions can be marked via this property.
 *
 * Since 3.7.0
 */
  optional<std::vector<DiagnosticRelatedInformation>> relatedInformation;

	
  bool operator==(const lsDiagnostic& rhs) const;
  bool operator!=(const lsDiagnostic& rhs) const;

  MAKE_SWAP_METHOD(lsDiagnostic, range, severity,  code, codeDescription, source, message, tags)
};
MAKE_REFLECT_STRUCT(lsDiagnostic, range, severity, code, codeDescription, source, message, tags)



struct Rsp_Error : ResponseError<lsResponseError, Rsp_Error> {
	

	MAKE_SWAP_METHOD(Rsp_Error, jsonrpc, id, error);
};
MAKE_REFLECT_STRUCT(Rsp_Error, jsonrpc, id, error);



