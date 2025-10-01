#include "json_loader.h"

#include <chrono>
#include <sstream>

namespace json_loader {
namespace {

model::Road LoadRoad(const boost::json::object& road_obj) {
    model::Coord x0 = road_obj.at(keys::X0).as_int64();
    model::Coord y0 = road_obj.at(keys::Y0).as_int64();
    if (road_obj.contains(keys::X1)) {
        model::Coord x1 = road_obj.at(keys::X1).as_int64();
        return model::Road{model::Road::HORIZONTAL, {x0, y0}, x1};
    } else {
        model::Coord y1 = road_obj.at(keys::Y1).as_int64();
        return model::Road{model::Road::VERTICAL, {x0, y0}, y1};
    }
}

model::Building LoadBuilding(const boost::json::object& building_obj) {
    model::Coord x = building_obj.at(keys::X).as_int64();
    model::Coord y = building_obj.at(keys::Y).as_int64();
    model::Dimension w = building_obj.at(keys::WIDTH).as_int64();
    model::Dimension h = building_obj.at(keys::HEIGHT).as_int64();
    return model::Building{model::Rectangle{{x, y}, {w, h}}};
}

model::Office LoadOffice(const boost::json::object& office_obj) {
    model::Office::Id office_id{std::string(office_obj.at(keys::ID).as_string())};
    model::Coord x = office_obj.at(keys::X).as_int64();
    model::Coord y = office_obj.at(keys::Y).as_int64();
    model::Dimension offset_x = office_obj.at(keys::OFFSET_X).as_int64();
    model::Dimension offset_y = office_obj.at(keys::OFFSET_Y).as_int64();
    return model::Office{office_id, {x, y}, {offset_x, offset_y}};
}

extra_data::MapData BuildExtraDataForMap(const std::string& id, const boost::json::object& map_obj) {
    extra_data::MapData map_data;
    map_data.id = id;
    if (const auto* loot_it = map_obj.if_contains(keys::LOOT_TYPES)) {
        map_data.loot_types = loot_it->as_array();
    }
    return map_data;
}

model::Map LoadMap(const boost::json::value& map_json, extra_data::Repository& extra_repo) {
    const auto& map_obj = map_json.as_object();

    std::string id_str(map_obj.at(keys::ID).as_string());
    model::Map::Id id{id_str};
    std::string name(map_obj.at(keys::NAME).as_string());
    model::Map map{id, name};

    if (map_obj.contains(keys::DOG_SPEED)) {
        map.SetDogSpeed(map_obj.at(keys::DOG_SPEED).as_double());
    }
    if (map_obj.contains(keys::BAG_CAPACITY)) {
        map.SetBagCapacity(static_cast<std::size_t>(map_obj.at(keys::BAG_CAPACITY).as_int64()));
    }

    for (const auto& road_json : map_obj.at(keys::ROADS).as_array()) {
        map.AddRoad(LoadRoad(road_json.as_object()));
    }

    for (const auto& building_json : map_obj.at(keys::BUILDINGS).as_array()) {
        map.AddBuilding(LoadBuilding(building_json.as_object()));
    }

    for (const auto& office_json : map_obj.at(keys::OFFICES).as_array()) {
        map.AddOffice(LoadOffice(office_json.as_object()));
    }

    extra_repo.SetMapData(BuildExtraDataForMap(id_str, map_obj));

    return map;
}

}  // namespace

LoadedGameData LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file_stream{json_path};
    if (!file_stream) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
    auto root = boost::json::parse(content);
    const auto& root_obj = root.as_object();

    LoadedGameData data;
    auto& game = data.game;
    auto& extra_repo = data.extra_data;

    if (root_obj.contains(keys::DEFAULT_DOG_SPEED)) {
        game.SetDefaultDogSpeed(root_obj.at(keys::DEFAULT_DOG_SPEED).as_double());
    }
    if (root_obj.contains(keys::DEFAULT_BAG_CAPACITY)) {
        game.SetDefaultBagCapacity(static_cast<std::size_t>(root_obj.at(keys::DEFAULT_BAG_CAPACITY).as_int64()));
    } else {
        game.SetDefaultBagCapacity(3);
    }

    if (root_obj.contains(keys::LOOT_GENERATOR_CONFIG)) {
        const auto& generator_obj = root_obj.at(keys::LOOT_GENERATOR_CONFIG).as_object();
        model::LootGeneratorConfig config;
        const double period_seconds = generator_obj.at(keys::PERIOD).as_double();
        config.period = std::chrono::duration_cast<loot_gen::LootGenerator::TimeInterval>(
            std::chrono::duration<double>(period_seconds));
        config.probability = generator_obj.at(keys::PROBABILITY).as_double();
        game.SetLootGeneratorConfig(config);
    }

    for (const auto& map_json : root_obj.at(keys::MAPS).as_array()) {
        game.AddMap(LoadMap(map_json, extra_repo));
    }

    return data;
}

}  // namespace json_loader
