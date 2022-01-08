#ifndef ASSETS_H
#define ASSETS_H

struct AssetHandle {
    i32 index;
    i32 id;
    
    bool operator!=(const AssetHandle &rhs)
    {
        return index != rhs.index || id != rhs.id;
    }
    
    bool operator==(const AssetHandle &rhs)
    {
        return index == rhs.index && id == rhs.id;
    }
};

#define ASSET_HANDLE_INVALID AssetHandle{ -1, -1 }

struct Asset {
    i32 id;
    String path;
    u8 *data;
    i64 size;
};

void init_assets(Array<String> folders);

AssetHandle find_asset_handle(String name);
Asset* find_asset(String name);
Asset* get_asset(AssetHandle handle);

#endif // ASSETS_H