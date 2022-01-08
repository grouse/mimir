#include "assets.h"

struct {
    i32 next_id = 0;
    Array<String> folders;
    DynamicArray<Asset> assets;
} g_assets;

void init_assets(Array<String> folders)
{
    array_create(&g_assets.folders, folders.count, mem_dynamic);
    for (i32 i = 0; i < folders.count; i++) {
        g_assets.folders[i] = absolute_path(folders[i], mem_dynamic);
    }
}

String resolve_asset_path(String name)
{
    if (file_exists(name) && !is_directory(name)) {
        return absolute_path(name);
    }
    
    for (auto f : g_assets.folders) {
        String s = join_path(f, name);
        if (file_exists(s) && !is_directory(s)) return absolute_path(s);
    }
    
    return "";
}

AssetHandle find_asset_handle(String name)
{
    String path = resolve_asset_path(name);
    if (path.length == 0) return ASSET_HANDLE_INVALID;
    
    for (i32 i = 0; i < g_assets.assets.count; i++) {
        if (path_equals(g_assets.assets[i].path, path)) {
            return AssetHandle{ .index = i, .id = g_assets.assets[i].id };
        }
    }
    
    FileInfo f = read_file(path, mem_dynamic);
    ASSERT(f.data);
    
    Asset a{};
    a.id = g_assets.next_id++;
    a.path = duplicate_string(path, mem_dynamic);
    a.data = f.data;
    a.size = f.size;
    i32 i = array_add(&g_assets.assets, a);
    
    return AssetHandle{ .index = i, .id = a.id };
}

Asset* find_asset(String name)
{
    AssetHandle handle = find_asset_handle(name);
    if (handle == ASSET_HANDLE_INVALID) return nullptr;
    return &g_assets.assets[handle.index];
}

Asset* get_asset(AssetHandle handle)
{
    ASSERT(handle != ASSET_HANDLE_INVALID);
    ASSERT(g_assets.assets[handle.index].id == handle.id);
    
    return &g_assets.assets[handle.index];
}