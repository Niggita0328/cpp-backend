#include "request_handler.h"
#include <map>

namespace http_handler {

namespace json_serializer {
json::value ToJson(const model::Road& road) {
    json::object road_obj;
    road_obj["x0"] = road.GetStart().x;
    road_obj["y0"] = road.GetStart().y;
    if (road.IsHorizontal()) {
        road_obj["x1"] = road.GetEnd().x;
    } else {
        road_obj["y1"] = road.GetEnd().y;
    }
    return road_obj;
}

json::value ToJson(const model::Building& building) {
    json::object building_obj;
    building_obj["x"] = building.GetBounds().position.x;
    building_obj["y"] = building.GetBounds().position.y;
    building_obj["w"] = building.GetBounds().size.width;
    building_obj["h"] = building.GetBounds().size.height;
    return building_obj;
}

json::value ToJson(const model::Office& office) {
    json::object office_obj;
    office_obj["id"] = *office.GetId();
    office_obj["x"] = office.GetPosition().x;
    office_obj["y"] = office.GetPosition().y;
    office_obj["offsetX"] = office.GetOffset().dx;
    office_obj["offsetY"] = office.GetOffset().dy;
    return office_obj;
}

json::value ToJson(const model::Map& map) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    
    json::array roads_array;
    for (const auto& road : map.GetRoads()) {
        roads_array.emplace_back(ToJson(road));
    }
    map_obj["roads"] = std::move(roads_array);

    json::array buildings_array;
    for (const auto& building : map.GetBuildings()) {
        buildings_array.emplace_back(ToJson(building));
    }
    map_obj["buildings"] = std::move(buildings_array);

    json::array offices_array;
    for (const auto& office : map.GetOffices()) {
        offices_array.emplace_back(ToJson(office));
    }
    map_obj["offices"] = std::move(offices_array);

    return map_obj;
}

} // namespace json_serializer

RequestHandler::RequestHandler(model::Game& game, fs::path static_root)
    : game_{game}
    , static_root_{std::move(static_root)} {
}

}  // namespace http_handler