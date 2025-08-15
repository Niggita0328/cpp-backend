#pragma once

#include <filesystem>
#include <fstream>
#include <boost/json.hpp>
#include "model.h"

namespace keys {

    constexpr auto MAPS = "maps";
    constexpr auto ID = "id";
    constexpr auto NAME = "name";
    constexpr auto DOG_SPEED = "dogSpeed";
    constexpr auto DEFAULT_DOG_SPEED = "defaultDogSpeed";
    
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
} // namespace keys

namespace json_loader {

    model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader