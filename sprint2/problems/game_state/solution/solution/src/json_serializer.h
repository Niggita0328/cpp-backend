#pragma once
#include "model.h"
#include <boost/json.hpp>

namespace json = boost::json;

namespace json_serializer {

// Параметр for_list определяет, нужно ли генерировать краткое (для списков) или полное представление карты.
json::value ToJson(const model::Map& map, bool for_list = false);
json::value ToJson(const model::Building& building);
json::value ToJson(const model::Road& road);
json::value ToJson(const model::Office& office);

} // namespace json_serializer