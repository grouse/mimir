#ifndef ASSETS_PUBLIC_H
#define ASSETS_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern void *load_string_asset(AssetHandle, void *existing, String, u8 *data, i32 size);

#endif // ASSETS_PUBLIC_H
