#pragma once

namespace bro::keys::api {

/// Mounts `bro.keys` onto `bro` in the current Bronze realm.
void installKeys();

} // namespace bro::keys::api

namespace brokeys::api {
using bro::keys::api::installKeys;
}

using bro::keys::api::installKeys;
