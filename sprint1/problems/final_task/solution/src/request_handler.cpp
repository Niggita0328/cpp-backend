#include "request_handler.h"

namespace http_handler {

namespace json_serializer {

namespace json = boost::json;

// Сериализуем дорогу
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

// Сериализуем здание
json::value ToJson(const model::Building& building) {
    json::object building_obj;
    building_obj["x"] = building.GetBounds().position.x;
    building_obj["y"] = building.GetBounds().position.y;
    building_obj["w"] = building.GetBounds().size.width;
    building_obj["h"] = building.GetBounds().size.height;
    return building_obj;
}

// Сериализуем офис
json::value ToJson(const model::Office& office) {
    json::object office_obj;
    office_obj["id"] = *office.GetId();
    office_obj["x"] = office.GetPosition().x;
    office_obj["y"] = office.GetPosition().y;
    office_obj["offsetX"] = office.GetOffset().dx;
    office_obj["offsetY"] = office.GetOffset().dy;
    return office_obj;
}

// Сериализуем карту (эта функция будет вызывать другие)
json::value ToJson(const model::Map& map) {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();
    
    json::array roads_array;
    for (const auto& road : map.GetRoads()) {
        roads_array.emplace_back(ToJson(road)); // <-- вызов сериализатора для дороги
    }
    map_obj["roads"] = std::move(roads_array);

    json::array buildings_array;
    for (const auto& building : map.GetBuildings()) {
        buildings_array.emplace_back(ToJson(building)); // <-- вызов сериализатора для здания
    }
    map_obj["buildings"] = std::move(buildings_array);

    json::array offices_array;
    for (const auto& office : map.GetOffices()) {
        offices_array.emplace_back(ToJson(office)); // <-- вызов сериализатора для офиса
    }
    map_obj["offices"] = std::move(offices_array);

    return map_obj;
}

} // namespace json_serializer

}  // namespace http_handler
