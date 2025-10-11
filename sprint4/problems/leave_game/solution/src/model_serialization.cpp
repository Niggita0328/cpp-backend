#include "model_serialization.h"

#include "application.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>

namespace serialization {

DogRepr::DogRepr(const model::Dog& dog)
    : id(*dog.GetId())
    , name(dog.GetName())
    , position(dog.GetPosition())
    , speed(dog.GetSpeed())
    , direction(dog.GetDirection())
    , score(dog.GetScore())
    , bag(dog.GetBag()) {
}

model::Dog DogRepr::Restore() const {
    model::Dog dog{name};
    dog.SetId(model::Dog::Id{id});
    dog.SetPosition(position);
    dog.SetSpeed(speed);
    dog.SetDirection(direction);
    if (score > 0) {
        dog.AddScore(score);
    }
    for (const auto& item : bag) {
        dog.AddToBag(item.id, item.type);
    }
    return dog;
}

LostObjectRepr::LostObjectRepr(const model::LostObject& object)
    : id(object.id)
    , type(object.type)
    , position(object.position) {
}

model::LostObject LostObjectRepr::Restore() const {
    return model::LostObject{id, type, position};
}

GameState CollectGameState(const model::Game& game, const app::Players& players) {
    GameState state;

    const auto& sessions = game.GetSessions();
    const auto& index = game.GetSessionIdToIndex();
    state.sessions.reserve(index.size());

    for (const auto& [map_id, session_idx] : index) {
        if (session_idx >= sessions.size()) {
            continue;
        }
        const auto& session = *sessions.at(session_idx);
        SessionRepr session_repr;
        session_repr.map_id = *map_id;
        session_repr.next_lost_object_id = session.GetNextLostObjectId();
        for (auto* dog : session.GetDogs()) {
            if (dog) {
                session_repr.dogs.emplace_back(*dog);
            }
        }
        for (const auto& lost_object : session.GetLostObjects()) {
            session_repr.lost_objects.emplace_back(lost_object);
        }
        state.sessions.push_back(std::move(session_repr));
    }

    state.players.reserve(players.GetAllPlayers().size());
    for (const auto& player_ptr : players.GetAllPlayers()) {
        if (!player_ptr) {
            continue;
        }
        const auto* session = player_ptr->GetSession();
        const auto* dog = player_ptr->GetDog();
        if (!session || !dog || !session->GetMap()) {
            continue;
        }
        PlayerRepr player_repr;
        player_repr.token = *player_ptr->GetToken();
        player_repr.map_id = *session->GetMap()->GetId();
        player_repr.dog_id = *dog->GetId();
        state.players.push_back(std::move(player_repr));
    }

    return state;
}

void ApplyGameState(const GameState& state, model::Game& game, app::Players& players) {
    players.Clear();
    game.ClearSessions();

    std::unordered_map<std::string, model::GameSession*> sessions_by_id;
    sessions_by_id.reserve(state.sessions.size());

    for (const auto& session_repr : state.sessions) {
        model::Map::Id map_id{session_repr.map_id};
        auto* session = game.AddSession(map_id);
        if (!session) {
            throw std::runtime_error("Unknown map id in saved state: " + session_repr.map_id);
        }

        std::vector<model::LostObject> lost_objects;
        lost_objects.reserve(session_repr.lost_objects.size());
        for (const auto& lost_repr : session_repr.lost_objects) {
            lost_objects.emplace_back(lost_repr.Restore());
        }
        session->SetLostObjects(std::move(lost_objects), session_repr.next_lost_object_id);

        sessions_by_id.emplace(session_repr.map_id, session);
    }

    std::unordered_map<std::string, std::unordered_map<std::uint64_t, DogRepr>> dogs_by_map;
    for (const auto& session_repr : state.sessions) {
        auto& dog_map = dogs_by_map[session_repr.map_id];
        for (const auto& dog_repr : session_repr.dogs) {
            dog_map.emplace(dog_repr.id, dog_repr);
        }
    }

    for (const auto& player_repr : state.players) {
        auto session_it = sessions_by_id.find(player_repr.map_id);
        if (session_it == sessions_by_id.end()) {
            throw std::runtime_error("Session for player not found in saved state: " + player_repr.map_id);
        }
        auto& dog_map = dogs_by_map[player_repr.map_id];
        auto dog_it = dog_map.find(player_repr.dog_id);
        if (dog_it == dog_map.end()) {
            throw std::runtime_error("Dog for player not found in saved state: " + std::to_string(player_repr.dog_id));
        }

        model::Dog restored = dog_it->second.Restore();
        auto dog_ptr = std::make_unique<model::Dog>(std::move(restored));
        auto* session = session_it->second;
        session->RestoreDog(dog_ptr.get());
        players.AddExisting(std::move(dog_ptr), *session, Token{player_repr.token});
        dog_map.erase(dog_it);
    }

    for (auto& [map_id, dog_map] : dogs_by_map) {
        auto session_it = sessions_by_id.find(map_id);
        if (session_it == sessions_by_id.end()) {
            continue;
        }
        auto* session = session_it->second;
        for (auto& [dog_id, dog_repr] : dog_map) {
            model::Dog restored = dog_repr.Restore();
            auto dog_ptr = std::make_unique<model::Dog>(std::move(restored));
            session->RestoreDog(dog_ptr.get());
            players.AddStrayDog(std::move(dog_ptr));
        }
    }
}

void SaveGameState(const GameState& state, const std::filesystem::path& path) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    auto temp_path = path;
    temp_path += ".tmp";

    std::ofstream output(temp_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to open state file for writing: " + temp_path.string());
    }

    boost::archive::text_oarchive archive(output);
    archive << state;
    output.close();

    std::filesystem::rename(temp_path, path);
}

GameState LoadGameState(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open state file for reading: " + path.string());
    }

    boost::archive::text_iarchive archive(input);
    GameState state;
    archive >> state;
    return state;
}

}  // namespace serialization
