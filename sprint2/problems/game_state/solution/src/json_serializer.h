#pragma once
#include "model.h"
#include "json_loader.h"
#include <boost/json.hpp>

namespace json = boost::json;

namespace json_serializer {

json::value ToJson(const model::Map& map, bool for_list = false);
json::value ToJson(const model::Building& building);
json::value ToJson(const model::Road& road);
json::value ToJson(const model::Office& office);
json::value ToJson(const model::Dog& dog);

} // namespace json_serializer