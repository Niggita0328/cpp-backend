#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/json_loader.h"
#include "../src/application.h"
#include "../src/extra_data.h"
#include "../src/json_serializer.h"

#include <boost/json.hpp>
#include <boost/asio/io_context.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <string>
#include <atomic>

using namespace std::literals;

namespace {

std::filesystem::path WriteTempConfig(std::string_view content) {
    static std::atomic_uint64_t counter{0};
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto file_name = std::string{"dogstory-test-"} + std::to_string(counter++) + ".json";
    auto temp_path = temp_dir / file_name;
    std::ofstream out(temp_path);
    REQUIRE(out.is_open());
    out << content;
    return temp_path;
}

}  // namespace

TEST_CASE("json_loader populates extra data and loot generator config") {
    static constexpr auto CONFIG_JSON = R"({
        "defaultDogSpeed": 3.0,
        "lootGeneratorConfig": {"period": 0.5, "probability": 0.75},
        "maps": [
            {
                "id": "map1",
                "name": "Map 1",
                "roads": [{"x0": 0, "y0": 0, "x1": 10}],
                "buildings": [],
                "offices": [],
                "lootTypes": [
                    {"name": "key", "file": "assets/key.obj", "type": "obj", "value": 5},
                    {"name": "wallet", "file": "assets/wallet.obj", "type": "obj", "value": 15}
                ]
            }
        ]
    })";

    const auto config_path = WriteTempConfig(CONFIG_JSON);
    auto loaded = json_loader::LoadGame(config_path);
    std::filesystem::remove(config_path);

    REQUIRE(loaded.game.GetMaps().size() == 1);

    const auto& config_opt = loaded.game.GetLootGeneratorConfig();
    REQUIRE(config_opt.has_value());
    CHECK(config_opt->period == loot_gen::LootGenerator::TimeInterval{500});
    CHECK(config_opt->probability == Catch::Approx(0.75));

    const auto* map_extra = loaded.extra_data.FindMapData("map1");
    REQUIRE(map_extra != nullptr);
    CHECK(map_extra->loot_types.size() == 2);
    CHECK(map_extra->loot_types.front().as_object().at("name").as_string() == "key"sv);
}

TEST_CASE("game session generates lost objects using loot config") {
    static constexpr auto CONFIG_JSON = R"({
        "lootGeneratorConfig": {"period": 0.1, "probability": 1.0},
        "maps": [
            {
                "id": "map1",
                "name": "Map 1",
                "roads": [{"x0": 0, "y0": 0, "x1": 10}],
                "buildings": [],
                "offices": [],
                "lootTypes": [
                    {"name": "coin", "file": "assets/coin.obj", "type": "obj", "value": 7}
                ]
            }
        ]
    })";

    const auto config_path = WriteTempConfig(CONFIG_JSON);
    auto loaded = json_loader::LoadGame(config_path);
    std::filesystem::remove(config_path);

    REQUIRE(loaded.game.GetMaps().size() == 1);
    const auto& map = loaded.game.GetMaps().front();

    boost::asio::io_context ioc;
    app::Players players;
    app::Application app{loaded.game, players, loaded.extra_data, ioc};

    auto join_result = app.JoinGame(map.GetId(), "Buddy");
    REQUIRE(join_result.has_value());

    app::Player* player = app.FindByToken(join_result->token);
    REQUIRE(player != nullptr);

    app.Tick(std::chrono::milliseconds{1000});

    const auto& lost_objects = player->GetSession()->GetLostObjects();
    REQUIRE_FALSE(lost_objects.empty());
    CHECK(lost_objects.front().type == 0);
    CHECK(lost_objects.front().position.x >= 0.0);
    CHECK(lost_objects.front().position.x <= 10.0);
}
TEST_CASE("json_loader handles bag capacity defaults") {
    static constexpr auto CONFIG_JSON = R"({
        "defaultDogSpeed": 3.0,
        "defaultBagCapacity": 5,
        "maps": [
            {
                "id": "map1",
                "name": "Map 1",
                "bagCapacity": 7,
                "roads": [{"x0": 0, "y0": 0, "x1": 10}],
                "buildings": [],
                "offices": []
            }
        ]
    })";

    const auto config_path = WriteTempConfig(CONFIG_JSON);
    auto loaded = json_loader::LoadGame(config_path);
    std::filesystem::remove(config_path);

    CHECK(loaded.game.GetDefaultBagCapacity() == 5);
    REQUIRE_FALSE(loaded.game.GetMaps().empty());
    const auto& map = loaded.game.GetMaps().front();
    REQUIRE(map.GetBagCapacity().has_value());
    CHECK(*map.GetBagCapacity() == 7);
}

TEST_CASE("player score increases after delivering loot") {
    model::Game game;
    model::Map::Id map_id{"score-map"};
    model::Map map{map_id, "Score Map"};
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 5});
    map.AddOffice(model::Office{model::Office::Id{"of1"}, {0, 0}, {0, 0}});
    map.SetLootTypeValues({25});

    game.AddMap(map);
    auto* session = game.AddSession(map_id);
    REQUIRE(session != nullptr);

    model::Dog dog{"Buddy"};
    session->AddDog(&dog);

    dog.AddToBag(1, 0);
    dog.SetPosition({0.0, 0.0});
    dog.SetSpeed({1.0, 0.0});

    session->Tick(std::chrono::milliseconds{1000});

    CHECK(dog.GetScore() == 25);
    CHECK(dog.GetBag().empty());

    auto dog_json = json_serializer::DogToJson(dog).as_object();
    REQUIRE(dog_json.contains("score"));
    CHECK(dog_json.at("score").as_uint64() == 25);
}
