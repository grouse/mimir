#ifndef ASSETS_H
#define ASSETS_H

#include "platform.h"
#include "string.h"
#include "array.h"
#include "gfx_opengl.h"

struct Asset;

struct AssetHandle {
    i32 index;
    i32 gen;

    bool operator!=(const AssetHandle &rhs) const { return index != rhs.index || gen != rhs.gen; }
    bool operator==(const AssetHandle &rhs) const { return index == rhs.index && gen == rhs.gen; }

    operator bool() const { return index != 0 || gen != 0; }
};

constexpr AssetHandle ASSET_HANDLE_INVALID = { 0, 0 };

typedef void* (*asset_load_t)(AssetHandle handle, void *existing, String identifier, u8 *data, i32 size);
typedef bool  (*asset_save_t)(AssetHandle handle, StringBuilder *stream, void *data);

struct AssetTypesDesc {
    struct {
        String ext;
        i32 type_id;
        asset_load_t load_proc;
        asset_save_t save_proc;
    } types[10];
};

struct Asset {
    i32 gen;

    u64 last_saved;
    u64 last_modified;

    String path;
    String identifier;

    i32 type_id;
    void *data;
};

void init_assets(Array<String> folders, const AssetTypesDesc &desc);

AssetHandle gen_asset_handle();

void create_asset(AssetHandle handle, Asset asset);
AssetHandle create_asset(String path, i32 type_id, void *data);
AssetHandle create_asset(Asset asset);

bool load_asset(AssetHandle handle, u8 *contents, i32 size);
AssetHandle load_asset(String path, u8 *contents, i32 size);

bool asset_path_used(String path);
bool ensure_loaded(AssetHandle handle);
bool is_asset_loaded(String path);

void remove_asset(AssetHandle handle);
void restore_removed_asset(AssetHandle handle);
AssetHandle restore_removed_asset(String path);

AssetHandle find_loaded_asset(String path);
Asset* find_asset_by_path(String path);
AssetHandle find_asset_handle(String filename);
Asset* get_asset(AssetHandle handle);

String get_asset_path(AssetHandle handle);

void dirty_asset(AssetHandle handle);
void save_dirty_assets();
Array<AssetHandle> get_unsaved_assets(Allocator mem);

String resolve_asset_path(String path, Allocator mem);
String normalise_asset_path(String path, Allocator mem = tl_scratch_arena());

Array<String> list_asset_files(Allocator mem);
Array<String> list_asset_files(String ext, Allocator mem);

template<typename T>
T* find_asset(String path)
{
    Asset *asset = find_asset_by_path(path);
    if (!asset) return nullptr;
    if (asset->type_id != typeid(T)) {
        LOG_ERROR("mismatching asset type id for asset: '%.*s', got %d expected %d", STRFMT(asset->path), typeid(T), asset->type_id);
        return nullptr;
    }

    return (T*)asset->data;
}

template<typename T>
T* get_asset(AssetHandle handle)
{
    Asset *asset = get_asset(handle);
    if (!asset) return nullptr;
    if (asset->type_id != typeid(T)) {
        LOG_ERROR("mismatching asset type id for asset: '%.*s', got %d expected %d", STRFMT(asset->path), typeid(T), asset->type_id);
        return nullptr;
    }
    return (T*)asset->data;
}

#endif // ASSETS_H
