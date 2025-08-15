#pragma once
#include "model.h"
#include "json_loader.h"
#include <boost/json.hpp>

namespace json = boost::json;

namespace json_serializer {

json::value MapToJson(const model::Map& map, bool for_list = false);
json::value BuildingToJson(const model::Building& building);
json::value RoadToJson(const model::Road& road);
json::value OfficeToJson(const model::Office& office);
json::value DogToJson(const model::Dog& dog);

} // namespace json_serializer