#pragma once

#include "api.h"
#include "embed/embed.h"
#include <string>

namespace bro::keys::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

Value ensureBroKeys();
void installKeysOnto(Value keysObj);
Value makeError(const std::string& msg);

} // namespace bro::keys::api
