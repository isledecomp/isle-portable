#pragma once

#include "mxstring.h"

namespace Extensions
{
namespace Common
{

// Resolve a relative game path (e.g. "\\lego\\scripts\\isle\\isle.si")
// by trying the HD path first, then falling back to CD.
// Returns true if the file exists at either location, with the
// filesystem-mapped result in p_outPath.
bool ResolveGamePath(const char* p_relativePath, MxString& p_outPath);

} // namespace Common
} // namespace Extensions
