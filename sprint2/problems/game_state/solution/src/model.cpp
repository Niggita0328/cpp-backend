#include "model.h"

#include <stdexcept>
#include <random>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

GameSession::GameSession(const Map* map_ptr) : map_(map_ptr) {}

void GameSession::AddDog(Dog* dog) {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        dog->SetPosition({0.0, 0.0});
    } else {
        std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
        const auto& random_road = roads[road_dist(generator_)];

        std::uniform_real_distribution<double> pos_dist(0.0, 1.0);
        double factor = pos_dist(generator_);
        
        PointD pos;
        pos.x = random_road.GetStart().x + factor * (random_road.GetEnd().x - random_road.GetStart().x);
        pos.y = random_road.GetStart().y + factor * (random_road.GetEnd().y - random_road.GetStart().y);
        dog->SetPosition(pos);
    }
    
    dog->SetSpeed({0.0, 0.0});
    dog->SetDirection("U");

    dogs_.push_back(dog);
}


void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::FindSession(const Map::Id& id) {
    if (auto it = session_id_to_index_.find(id); it != session_id_to_index_.end()) {
        return &sessions_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::AddSession(const Map::Id& id) {
    const Map* map = FindMap(id);
    if (!map) {
        return nullptr;
    }
    const size_t index = sessions_.size();
    auto& session = sessions_.emplace_back(map);
    session_id_to_index_[id] = index;
    return &session;
}

}  // namespace model