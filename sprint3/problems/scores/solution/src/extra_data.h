#pragma once

#include <boost/json.hpp>

#include <string>
#include <unordered_map>

namespace extra_data {

struct MapData {
    std::string id;
    boost::json::array loot_types;
};

class Repository {
public:
    void Clear() noexcept;
    void SetMapData(MapData data);
    const MapData* FindMapData(const std::string& id) const noexcept;
    const boost::json::array* FindLootTypes(const std::string& id) const noexcept;
    std::size_t GetLootTypesCount(const std::string& id) const noexcept;

private:
    std::unordered_map<std::string, MapData> maps_;
};

}  // namespace extra_data
