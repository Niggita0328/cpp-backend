#include "model.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace model {
using namespace std::literals;

namespace {

struct CollectionResult {
    double sq_distance;
    double proj_ratio;

    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }
};

CollectionResult TryCollectPoint(PointD a, PointD b, PointD c) {
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult{sq_distance, proj_ratio};
}

}  // namespace

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;
    const size_t gatherers_count = provider.GatherersCount();
    const size_t items_count = provider.ItemsCount();
    events.reserve(gatherers_count * items_count);

    for (size_t gatherer_id = 0; gatherer_id < gatherers_count; ++gatherer_id) {
        const Gatherer gatherer = provider.GetGatherer(gatherer_id);
        if (gatherer.start_pos.x == gatherer.end_pos.x && gatherer.start_pos.y == gatherer.end_pos.y) {
            continue;
        }

        for (size_t item_id = 0; item_id < items_count; ++item_id) {
            const Item item = provider.GetItem(item_id);
            const double collect_radius = gatherer.width + item.width;
            const CollectionResult result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
            if (result.IsCollected(collect_radius)) {
                events.push_back(GatheringEvent{item_id, gatherer_id, result.sq_distance, result.proj_ratio});
            }
        }
    }

    std::sort(events.begin(), events.end(), [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
        if (lhs.time < rhs.time) {
            return true;
        }
        if (lhs.time > rhs.time) {
            return false;
        }
        if (lhs.item_id < rhs.item_id) {
            return true;
        }
        if (lhs.item_id > rhs.item_id) {
            return false;
        }
        return lhs.gatherer_id < rhs.gatherer_id;
    });

    return events;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // �?�?���>�?��? �?�"��? ��� �?���'�?�?��, ��?�>�� �?�� �?�?���>�?�?�? �?�?�'���?��'�? �? unordered_map
        offices_.pop_back();
        throw;
    }
}

GameSession::GameSession(const Map* map_ptr, std::size_t bag_capacity, bool randomize_spawn) 
    : map_(map_ptr)
    , randomize_spawn_points_{randomize_spawn}
    , bag_capacity_{bag_capacity} {}

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

PointD GeneratePointOnRoad(const Road& road, std::mt19937_64& generator) {
    auto start = road.GetStart();
    auto end = road.GetEnd();

    if (road.IsHorizontal()) {
        const double min_x = std::min<double>(start.x, end.x);
        const double max_x = std::max<double>(start.x, end.x);
        std::uniform_real_distribution<double> dist(min_x, max_x);
        return {dist(generator), static_cast<double>(start.y)};
    }

    const double min_y = std::min<double>(start.y, end.y);
    const double max_y = std::max<double>(start.y, end.y);
    std::uniform_real_distribution<double> dist(min_y, max_y);
    return {static_cast<double>(start.x), dist(generator)};
}

}  // namespace


void GameSession::Tick(std::chrono::milliseconds delta) {
    const double delta_s = static_cast<double>(delta.count()) / 1000.0;

    struct MovementData {
        Dog* dog = nullptr;
        PointD start;
        PointD end;
    };

    std::vector<MovementData> movements;
    movements.reserve(dogs_.size());

    constexpr double kPlayerRadius = 0.6 / 2.0;
    constexpr double kOfficeRadius = 0.5 / 2.0;

    for (auto& dog : dogs_) {
        const PointD start_pos = dog->GetPosition();
        MovementData movement{dog, start_pos, start_pos};

        const auto speed = dog->GetSpeed();
        if (speed.u == 0.0 && speed.v == 0.0) {
            movements.push_back(movement);
            continue;
        }

        PointD end_pos_estimated{start_pos.x + speed.u * delta_s, start_pos.y + speed.v * delta_s};

        std::vector<const Road*> current_roads;
        current_roads.reserve(map_->GetRoads().size());
        for (const auto& road : map_->GetRoads()) {
            if (IsOnRoad(start_pos, GetRoadBorders(road))) {
                current_roads.push_back(&road);
            }
        }

        if (current_roads.empty()) {
            dog->SetSpeed({0.0, 0.0});
            movements.push_back(movement);
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
        movement.end = final_pos;

        auto is_close = [](double a, double b) {
            return std::abs(a - b) < 1e-9;
        };
        auto points_are_close = [&](const PointD& p1, const PointD& p2) {
            return is_close(p1.x, p2.x) && is_close(p1.y, p2.y);
        };

        if (!points_are_close(final_pos, end_pos_estimated)) {
            dog->SetSpeed({0.0, 0.0});
        }

        movements.push_back(movement);
    }

    std::vector<Gatherer> gatherers;
    std::vector<Dog*> gatherer_dogs;
    gatherers.reserve(movements.size());
    gatherer_dogs.reserve(movements.size());

    for (const auto& movement : movements) {
        if (movement.start.x == movement.end.x && movement.start.y == movement.end.y) {
            continue;
        }
        gatherers.push_back(Gatherer{movement.start, movement.end, kPlayerRadius});
        gatherer_dogs.push_back(movement.dog);
    }

    enum class ItemKind { LostObject, Office };

    struct ItemInfo {
        ItemKind kind;
        size_t index;
    };

    std::vector<Item> items;
    std::vector<ItemInfo> item_infos;
    const auto items_capacity = lost_objects_.size() + map_->GetOffices().size();
    items.reserve(items_capacity);
    item_infos.reserve(items_capacity);

    for (size_t i = 0; i < lost_objects_.size(); ++i) {
        items.push_back(Item{lost_objects_[i].position, 0.0});
        item_infos.push_back(ItemInfo{ItemKind::LostObject, i});
    }
    for (size_t i = 0; i < map_->GetOffices().size(); ++i) {
        const auto& office = map_->GetOffices()[i];
        PointD office_point{static_cast<double>(office.GetPosition().x + office.GetOffset().dx),
                            static_cast<double>(office.GetPosition().y + office.GetOffset().dy)};
        items.push_back(Item{office_point, kOfficeRadius});
        item_infos.push_back(ItemInfo{ItemKind::Office, i});
    }

    if (!items.empty() && !gatherers.empty()) {
        class SessionItemProvider : public ItemGathererProvider {
        public:
            SessionItemProvider(const std::vector<Item>& items, const std::vector<Gatherer>& gatherers)
                : items_(items)
                , gatherers_(gatherers) {
            }

            size_t ItemsCount() const override {
                return items_.size();
            }

            Item GetItem(size_t idx) const override {
                return items_.at(idx);
            }

            size_t GatherersCount() const override {
                return gatherers_.size();
            }

            Gatherer GetGatherer(size_t idx) const override {
                return gatherers_.at(idx);
            }

        private:
            const std::vector<Item>& items_;
            const std::vector<Gatherer>& gatherers_;
        };

        SessionItemProvider provider(items, gatherers);
        const auto events = FindGatherEvents(provider);

        std::vector<bool> collected(lost_objects_.size(), false);

        for (const auto& event : events) {
            Dog* dog = gatherer_dogs[event.gatherer_id];
            const ItemInfo& info = item_infos[event.item_id];
            if (info.kind == ItemKind::LostObject) {
                if (collected[info.index]) {
                    continue;
                }
                if (dog->IsBagFull(bag_capacity_)) {
                    continue;
                }
                const auto& lost_object = lost_objects_[info.index];
                dog->AddToBag(lost_object.id, lost_object.type);
                collected[info.index] = true;
            } else {
                if (!dog->GetBag().empty()) {
                    dog->ClearBag();
                }
            }
        }

        if (std::any_of(collected.begin(), collected.end(), [](bool value) { return value; })) {
            const auto collected_count = static_cast<size_t>(std::count(collected.begin(), collected.end(), true));
            std::vector<LostObject> remaining;
            remaining.reserve(lost_objects_.size() - collected_count);
            for (size_t i = 0; i < lost_objects_.size(); ++i) {
                if (!collected[i]) {
                    remaining.push_back(lost_objects_[i]);
                }
            }
            lost_objects_ = std::move(remaining);
        }
    }

    if (loot_generator_ && loot_types_count_ > 0) {
        const unsigned generated = loot_generator_->Generate(
            delta,
            static_cast<unsigned>(lost_objects_.size()),
            static_cast<unsigned>(dogs_.size()));
        if (generated > 0) {
            SpawnLostObjects(generated);
        }
    }
}


void GameSession::SetLootGeneratorConfig(LootGeneratorConfig config) {
    loot_generator_.emplace(config.period, config.probability);
}

void GameSession::SetLootTypesCount(std::size_t count) noexcept {
    loot_types_count_ = count;
}

const std::vector<LostObject>& GameSession::GetLostObjects() const noexcept {
    return lost_objects_;
}

void GameSession::SpawnLostObjects(unsigned count) {
    if (!map_ || map_->GetRoads().empty() || loot_types_count_ == 0) {
        return;
    }

    std::uniform_int_distribution<std::size_t> road_dist(0, map_->GetRoads().size() - 1);
    std::uniform_int_distribution<std::size_t> type_dist(0, loot_types_count_ - 1);

    for (unsigned i = 0; i < count; ++i) {
        const auto& road = map_->GetRoads()[road_dist(generator_)];
        PointD position = GeneratePointOnRoad(road, generator_);

        LostObject object;
        object.id = next_lost_object_id_++;
        object.type = type_dist(generator_);
        object.position = position;
        lost_objects_.push_back(object);
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
    const std::size_t bag_capacity = map->GetBagCapacity().value_or(default_bag_capacity_);
    auto& session = sessions_.emplace_back(map, bag_capacity, randomize_spawn_points_);
    session_id_to_index_[id] = index;
    if (loot_generator_config_) {
        session.SetLootGeneratorConfig(*loot_generator_config_);
    }
    return &session;
}

void Game::SetLootGeneratorConfig(LootGeneratorConfig config) {
    loot_generator_config_ = config;
    for (auto& session : sessions_) {
        session.SetLootGeneratorConfig(config);
    }
}

const std::optional<LootGeneratorConfig>& Game::GetLootGeneratorConfig() const noexcept {
    return loot_generator_config_;
}

void Game::Tick(std::chrono::milliseconds delta) {
    for (auto& session : sessions_) {
        session.Tick(delta);
    }
}

}  // namespace model


