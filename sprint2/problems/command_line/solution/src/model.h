#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>
#include <random>
#include <chrono>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

namespace detail {
struct DogTag {};
}  // namespace detail

struct PointD {
    double x = 0.0, y = 0.0;
};

struct Vec2D {
    double u = 0.0, v = 0.0;
};

class Dog {
public:
    using Id = util::Tagged<uint64_t, detail::DogTag>;

    explicit Dog(std::string name) : name_{std::move(name)} {}

    const Id& GetId() const { return id_; }
    void SetId(Id id) { id_ = id; }
    const std::string& GetName() const { return name_; }
    const PointD& GetPosition() const { return pos_; }
    const Vec2D& GetSpeed() const { return speed_; }
    const std::string& GetDirection() const { return dir_; }

    void SetPosition(PointD pos) { pos_ = pos; }
    void SetSpeed(Vec2D speed) { speed_ = speed; }
    void SetDirection(std::string dir) { dir_ = std::move(dir); }

private:
    Id id_{0};
    std::string name_;
    PointD pos_{};
    Vec2D speed_{};
    std::string dir_ = "U"; // "L", "R", "U", "D"
};

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }
    
    double GetDogSpeed() const {
        return dog_speed_.value_or(0.0);
    }

    void SetDogSpeed(double speed) {
        dog_speed_ = speed;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    std::optional<double> dog_speed_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class GameSession {
public:
    explicit GameSession(const Map* map_ptr, bool randomize_spawn = false);
    
    const Map* GetMap() const { return map_; }
    const std::vector<Dog*>& GetDogs() const { return dogs_; }

    void AddDog(Dog* dog);
    void Tick(std::chrono::milliseconds delta);

private:
    const Map* map_;
    std::vector<Dog*> dogs_;
    bool randomize_spawn_points_;
    std::mt19937_64 generator_{std::random_device{}()};
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);
    
    void SetDefaultDogSpeed(double speed) {
        default_dog_speed_ = speed;
    }

    double GetDefaultDogSpeed() const {
        return default_dog_speed_;
    }

    void SetRandomizeSpawn(bool randomize) {
        randomize_spawn_points_ = randomize;
    }

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept;
    GameSession* FindSession(const Map::Id& id);
    GameSession* AddSession(const Map::Id& id);
    void Tick(std::chrono::milliseconds delta);

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using SessionIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    double default_dog_speed_ = 1.0;
    bool randomize_spawn_points_ = false;
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    
    std::vector<GameSession> sessions_;
    SessionIdToIndex session_id_to_index_;
};

}  // namespace model