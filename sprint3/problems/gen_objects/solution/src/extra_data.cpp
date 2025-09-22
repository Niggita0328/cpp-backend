#include "extra_data.h"

namespace extra_data {

void Repository::Clear() noexcept {
    maps_.clear();
}

void Repository::SetMapData(MapData data) {
    std::string key = data.id;
    maps_.insert_or_assign(std::move(key), std::move(data));
}

const MapData* Repository::FindMapData(const std::string& id) const noexcept {
    if (auto it = maps_.find(id); it != maps_.end()) {
        return &it->second;
    }
    return nullptr;
}

const boost::json::array* Repository::FindLootTypes(const std::string& id) const noexcept {
    if (auto* map_data = FindMapData(id)) {
        return &map_data->loot_types;
    }
    return nullptr;
}

std::size_t Repository::GetLootTypesCount(const std::string& id) const noexcept {
    if (auto* loot = FindLootTypes(id)) {
        return loot->size();
    }
    return 0;
}

}  // namespace extra_data
