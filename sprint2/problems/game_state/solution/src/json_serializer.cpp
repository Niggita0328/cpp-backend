#include "json_serializer.h"

namespace json_serializer {

json::value ToJson(const model::Road& road) {
    json::object road_obj;
    road_obj[keys::X0] = road.GetStart().x;
    road_obj[keys::Y0] = road.GetStart().y;
    if (road.IsHorizontal()) {
        road_obj[keys::X1] = road.GetEnd().x;
    } else {
        road_obj[keys::Y1] = road.GetEnd().y;
    }
    return road_obj;
}

json::value ToJson(const model::Building& building) {
    json::object building_obj;
    building_obj[keys::X] = building.GetBounds().position.x;
    building_obj[keys::Y] = building.GetBounds().position.y;
    building_obj[keys::WIDTH] = building.GetBounds().size.width;
    building_obj[keys::HEIGHT] = building.GetBounds().size.height;
    return building_obj;
}

json::value ToJson(const model::Office& office) {
    json::object office_obj;
    office_obj[keys::ID] = *office.GetId();
    office_obj[keys::X] = office.GetPosition().x;
    office_obj[keys::Y] = office.GetPosition().y;
    office_obj[keys::OFFSET_X] = office.GetOffset().dx;
    office_obj[keys::OFFSET_Y] = office.GetOffset().dy;
    return office_obj;
}

json::value ToJson(const model::Map& map, bool for_list) {
    json::object map_obj;
    map_obj[keys::ID] = *map.GetId();
    map_obj[keys::NAME] = map.GetName();
    
    if (for_list) {
        return map_obj;
    }

    if(map.GetDogSpeed() > 0) {
        map_obj[keys::DOG_SPEED] = map.GetDogSpeed();
    }
    
    json::array roads_array;
    for (const auto& road : map.GetRoads()) {
        roads_array.emplace_back(ToJson(road));
    }
    map_obj[keys::ROADS] = std::move(roads_array);

    json::array buildings_array;
    for (const auto& building : map.GetBuildings()) {
        buildings_array.emplace_back(ToJson(building));
    }
    map_obj[keys::BUILDINGS] = std::move(buildings_array);

    json::array offices_array;
    for (const auto& office : map.GetOffices()) {
        offices_array.emplace_back(ToJson(office));
    }
    map_obj[keys::OFFICES] = std::move(offices_array);

    return map_obj;
}

json::value ToJson(const model::Dog& dog) {
    json::object dog_obj;
    dog_obj["pos"] = json::array{dog.GetPosition().x, dog.GetPosition().y};
    dog_obj["speed"] = json::array{dog.GetSpeed().u, dog.GetSpeed().v};
    dog_obj["dir"] = dog.GetDirection();
    return dog_obj;
}

} // namespace json_serializer