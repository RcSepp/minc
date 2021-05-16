#include "minc_types.h"

const MincBuildtime::Settings DEFAULT_BUILD_SETTINGS = {
	false, // debug
	0, // maxErrors
};

MincBuildtime::MincBuildtime(MincBlockExpr* parentBlock)
	: parentBlock(parentBlock), settings(DEFAULT_BUILD_SETTINGS), currentRunner(nullptr), currentFileRunners(nullptr)
{
}