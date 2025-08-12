#include "json_loader.h"

namespace json_loader {

model::Road LoadRoad(const boost::json::object& road_obj) {
    model::Coord x0 = road_obj.at("x0").as_int64();
    model::Coord y0 = road_obj.at("y0").as_int64();
    if (road_obj.contains("x1")) {
        model::Coord x1 = road_obj.at("x1").as_int64();
        return model::Road{model::Road::HORIZONTAL, {x0, y0}, x1};
    } else {
        model::Coord y1 = road_obj.at("y1").as_int64();
        return model::Road{model::Road::VERTICAL, {x0, y0}, y1};
    }
}

model::Building LoadBuilding(const boost::json::object& building_obj) {
    model::Coord x = building_obj.at("x").as_int64();
    model::Coord y = building_obj.at("y").as_int64();
    model::Dimension w = building_obj.at("w").as_int64();
    model::Dimension h = building_obj.at("h").as_int64();
    return model::Building{model::Rectangle{{x, y}, {w, h}}};
}

model::Office LoadOffice(const boost::json::object& office_obj) {
    model::Office::Id office_id{std::string(office_obj.at("id").as_string())};
    model::Coord x = office_obj.at("x").as_int64();
    model::Coord y = office_obj.at("y").as_int64();
    model::Dimension offset_x = office_obj.at("offsetX").as_int64();
    model::Dimension offset_y = office_obj.at("offsetY").as_int64();
    return model::Office{office_id, {x, y}, {offset_x, offset_y}};
}

model::Map LoadMap(const boost::json::value& map_json) {
    const auto& map_obj = map_json.as_object();

    model::Map::Id id{std::string(map_obj.at("id").as_string())};
    std::string name(map_obj.at("name").as_string());
    model::Map map{id, name};

    for (const auto& road_json : map_obj.at("roads").as_array()) {
        map.AddRoad(LoadRoad(road_json.as_object()));
    }

    for (const auto& building_json : map_obj.at("buildings").as_array()) {
        map.AddBuilding(LoadBuilding(building_json.as_object()));
    }

    for (const auto& office_json : map_obj.at("offices").as_array()) {
        map.AddOffice(LoadOffice(office_json.as_object()));
    }

    return map;
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file_stream{json_path};
    if (!file_stream) {
        throw std::runtime_error("Failed to open file: " + json_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
    auto root = boost::json::parse(content);
    
    model::Game game;
    for (const auto& map_json : root.as_object().at("maps").as_array()) {
        game.AddMap(LoadMap(map_json));
    }
    
    return game;
}

}  // namespace json_loader
