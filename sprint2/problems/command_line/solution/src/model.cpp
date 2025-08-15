#include "model.h"

#include <stdexcept>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>

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

GameSession::GameSession(const Map* map_ptr, bool randomize_spawn) 
    : map_(map_ptr), randomize_spawn_points_{randomize_spawn} {}

void GameSession::AddDog(Dog* dog) {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        dog->SetPosition({0.0, 0.0});
    } else {
        if (randomize_spawn_points_) {
            std::uniform_int_distribution<size_t> road_idx_dist(0, roads.size() - 1);
            const auto& road = roads.at(road_idx_dist(generator_));
            
            std::uniform_real_distribution<> dist(0.0, 1.0);
            double t = dist(generator_);
            
            PointD pos;
            pos.x = road.GetStart().x + t * (road.GetEnd().x - road.GetStart().x);
            pos.y = road.GetStart().y + t * (road.GetEnd().y - road.GetStart().y);
            dog->SetPosition(pos);
        } else {
            const auto& first_road = roads.front();
            PointD pos;
            pos.x = first_road.GetStart().x;
            pos.y = first_road.GetStart().y;
            dog->SetPosition(pos);
        }
    }
    
    dog->SetSpeed({0.0, 0.0});
    dog->SetDirection("U");

    dogs_.push_back(dog);
}

namespace {

std::pair<PointD, PointD> GetRoadBorders(const Road& road) {
    constexpr double ROAD_HALF_WIDTH = 0.4;
    auto start = road.GetStart();
    auto end = road.GetEnd();

    double x_min = std::min(static_cast<double>(start.x), static_cast<double>(end.x));
    double x_max = std::max(static_cast<double>(start.x), static_cast<double>(end.x));
    double y_min = std::min(static_cast<double>(start.y), static_cast<double>(end.y));
    double y_max = std::max(static_cast<double>(start.y), static_cast<double>(end.y));
    
    return {
        {x_min - ROAD_HALF_WIDTH, y_min - ROAD_HALF_WIDTH},
        {x_max + ROAD_HALF_WIDTH, y_max + ROAD_HALF_WIDTH}
    };
}


bool IsOnRoad(const PointD& pos, const std::pair<PointD, PointD>& borders) {
    return pos.x >= borders.first.x && pos.x <= borders.second.x &&
           pos.y >= borders.first.y && pos.y <= borders.second.y;
}

}

void GameSession::Tick(std::chrono::milliseconds delta) {
    const double delta_s = static_cast<double>(delta.count()) / 1000.0;

    for (auto& dog : dogs_) {
        if (dog->GetSpeed().u == 0.0 && dog->GetSpeed().v == 0.0) {
            continue;
        }

        const auto start_pos = dog->GetPosition();
        const auto speed = dog->GetSpeed();
        PointD end_pos_estimated{start_pos.x + speed.u * delta_s, start_pos.y + speed.v * delta_s};

        std::vector<const Road*> current_roads;
        for (const auto& road : map_->GetRoads()) {
            if (IsOnRoad(start_pos, GetRoadBorders(road))) {
                current_roads.push_back(&road);
            }
        }
        
        if (current_roads.empty()) {
            dog->SetSpeed({0.0, 0.0});
            continue;
        }

        PointD final_pos;
        if (current_roads.size() == 1) {
            auto borders = GetRoadBorders(*current_roads.front());
            final_pos.x = std::clamp(end_pos_estimated.x, borders.first.x, borders.second.x);
            final_pos.y = std::clamp(end_pos_estimated.y, borders.first.y, borders.second.y);
        } else {
            final_pos = start_pos;
            double max_dist_sq = -1.0;

            for (const auto* road : current_roads) {
                auto borders = GetRoadBorders(*road);
                PointD bounded_pos;
                bounded_pos.x = std::clamp(end_pos_estimated.x, borders.first.x, borders.second.x);
                bounded_pos.y = std::clamp(end_pos_estimated.y, borders.first.y, borders.second.y);

                double dist_sq = std::pow(bounded_pos.x - start_pos.x, 2) + std::pow(bounded_pos.y - start_pos.y, 2);

                if (dist_sq > max_dist_sq) {
                    max_dist_sq = dist_sq;
                    final_pos = bounded_pos;
                }
            }
        }
        
        dog->SetPosition(final_pos);
        
        auto is_close = [](double a, double b) {
            return std::abs(a - b) < 1e-9;
        };
        auto points_are_close = [&](const PointD& p1, const PointD& p2) {
            return is_close(p1.x, p2.x) && is_close(p1.y, p2.y);
        };

        if (!points_are_close(final_pos, end_pos_estimated)) {
            dog->SetSpeed({0.0, 0.0});
        }
    }
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
    auto& session = sessions_.emplace_back(map, randomize_spawn_points_);
    session_id_to_index_[id] = index;
    return &session;
}

void Game::Tick(std::chrono::milliseconds delta) {
    for (auto& session : sessions_) {
        session.Tick(delta);
    }
}

}  // namespace model