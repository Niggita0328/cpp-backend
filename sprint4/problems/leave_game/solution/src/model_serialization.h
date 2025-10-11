#pragma once

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "model.h"

namespace app {
class Players;
}

namespace serialization {

struct DogRepr {
    DogRepr() = default;
    explicit DogRepr(const model::Dog& dog);

    [[nodiscard]] model::Dog Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & id;
        ar & name;
        ar & position;
        ar & speed;
        ar & direction;
        ar & score;
        ar & bag;
    }

    std::uint64_t id = 0;
    std::string name;
    model::PointD position{};
    model::Vec2D speed{};
    std::string direction;
    std::uint64_t score = 0;
    std::vector<model::Dog::BagItem> bag;
};

struct LostObjectRepr {
    LostObjectRepr() = default;
    explicit LostObjectRepr(const model::LostObject& object);

    [[nodiscard]] model::LostObject Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & id;
        ar & type;
        ar & position;
    }

    std::uint64_t id = 0;
    std::size_t type = 0;
    model::PointD position{};
};

struct SessionRepr {
    std::string map_id;
    std::vector<DogRepr> dogs;
    std::vector<LostObjectRepr> lost_objects;
    std::uint64_t next_lost_object_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & map_id;
        ar & dogs;
        ar & lost_objects;
        ar & next_lost_object_id;
    }
};

struct PlayerRepr {
    std::string token;
    std::string map_id;
    std::uint64_t dog_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & token;
        ar & map_id;
        ar & dog_id;
    }
};

struct GameState {
    std::vector<SessionRepr> sessions;
    std::vector<PlayerRepr> players;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned /*version*/) {
        ar & sessions;
        ar & players;
    }
};

GameState CollectGameState(const model::Game& game, const app::Players& players);
void ApplyGameState(const GameState& state, model::Game& game, app::Players& players);
void SaveGameState(const GameState& state, const std::filesystem::path& path);
GameState LoadGameState(const std::filesystem::path& path);

}  // namespace serialization

namespace model {

template <typename Archive>
void serialize(Archive& ar, PointD& point, const unsigned /*version*/) {
    ar & point.x;
    ar & point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, const unsigned /*version*/) {
    ar & vec.u;
    ar & vec.v;
}

template <typename Archive>
void serialize(Archive& ar, Dog::BagItem& item, const unsigned /*version*/) {
    ar & item.id;
    ar & item.type;
}

template <typename Archive>
void serialize(Archive& ar, LostObject& object, const unsigned /*version*/) {
    ar & object.id;
    ar & object.type;
    ar & object.position;
}

}  // namespace model
