#pragma once

#include <filesystem>
#include <fstream>
#include <boost/json.hpp>
#include "model.h"

namespace json_loader {

model::Road LoadRoad(const boost::json::object& road_obj);
model::Building LoadBuilding(const boost::json::object& building_obj);
model::Office LoadOffice(const boost::json::object& office_obj);
model::Map LoadMap(const boost::json::value& map_json);
model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
