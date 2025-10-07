#pragma once

#include <filesystem>
#include <fstream>
#include <boost/json.hpp>

#include "extra_data.h"
#include "model.h"

namespace keys {

    constexpr auto MAPS = "maps";
    constexpr auto ID = "id";
    constexpr auto NAME = "name";
    constexpr auto DOG_SPEED = "dogSpeed";
    constexpr auto DEFAULT_DOG_SPEED = "defaultDogSpeed";
    constexpr auto DEFAULT_BAG_CAPACITY = "defaultBagCapacity";
    constexpr auto BAG_CAPACITY = "bagCapacity";

    constexpr auto ROADS = "roads";
    constexpr auto X0 = "x0";
    constexpr auto Y0 = "y0";
    constexpr auto X1 = "x1";
    constexpr auto Y1 = "y1";

    constexpr auto BUILDINGS = "buildings";
    constexpr auto X = "x";
    constexpr auto Y = "y";
    constexpr auto WIDTH = "w";
    constexpr auto HEIGHT = "h";

    constexpr auto OFFICES = "offices";
    constexpr auto OFFSET_X = "offsetX";
    constexpr auto OFFSET_Y = "offsetY";

    constexpr auto LOOT_GENERATOR_CONFIG = "lootGeneratorConfig";
    constexpr auto PERIOD = "period";
    constexpr auto PROBABILITY = "probability";
    constexpr auto LOOT_TYPES = "lootTypes";
    constexpr auto FILE = "file";
    constexpr auto TYPE = "type";
    constexpr auto ROTATION = "rotation";
    constexpr auto COLOR = "color";
    constexpr auto SCALE = "scale";
    constexpr auto VALUE = "value";

} // namespace keys

namespace json_loader {

struct LoadedGameData {
    model::Game game;
    extra_data::MapRepository extra_data;
};

LoadedGameData LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
