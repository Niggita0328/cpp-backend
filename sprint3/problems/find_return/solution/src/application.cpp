#include "application.h"
#include <random>
#include <utility>

namespace app {

Player::Player(model::GameSession* session, model::Dog* dog, Token token)
    : dog_{dog}, session_{session}, token_{std::move(token)} {}

const Token& Player::GetToken() const {
    return token_;
}

model::Dog::Id Player::GetId() const {
    return dog_->GetId();
}

const std::string& Player::GetName() const {
    return dog_->GetName();
}

model::GameSession* Player::GetSession() {
    return session_;
}

model::Dog* Player::GetDog() {
    return dog_;
}


Player* Players::Add(std::unique_ptr<model::Dog> dog, model::GameSession& session) {
    Token token = GenerateToken();
    
    model::Dog* dog_raw_ptr = dog.get();
    dog_raw_ptr->SetId(model::Dog::Id{dogs_.size()});
    dogs_.emplace_back(std::move(dog));

    auto player = std::make_unique<Player>(&session, dog_raw_ptr, token);
    Player* player_raw_ptr = player.get();
    players_.emplace_back(std::move(player));
    
    token_to_player_[token] = player_raw_ptr;
    return player_raw_ptr;
}

Player* Players::FindByToken(const Token& token) {
    if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
        return it->second;
    }
    return nullptr;
}

Token Players::GenerateToken() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << generator1_() << std::setw(16) << generator2_();
    return Token{ss.str()};
}

std::vector<model::Dog> Players::GetDogs() const {
    std::vector<model::Dog> dogs;
    dogs.reserve(dogs_.size());
    for(const auto& d_ptr : dogs_){
        dogs.push_back(*d_ptr);
    }
    return dogs;
}


Application::Application(model::Game& game, Players& players, extra_data::MapRepository& extra_data, net::io_context& ioc)
    : game_{game}, players_{players}, extra_data_{extra_data}, strand_{net::make_strand(ioc)} {}

const std::vector<model::Map>& Application::ListMaps() const {
    return game_.GetMaps();
}

const model::Map* Application::FindMap(const model::Map::Id& id) const {
    return game_.FindMap(id);
}

const extra_data::MapData* Application::GetMapExtra(const model::Map::Id& id) const {
    return extra_data_.FindMapData(*id);
}

std::optional<JoinGameResult> Application::JoinGame(const model::Map::Id& map_id, const std::string& user_name) {
    const model::Map* map = FindMap(map_id);
    if (!map) {
        return std::nullopt;
    }

    model::GameSession* session = game_.FindSession(map_id);
    if (!session) {
        session = game_.AddSession(map_id);
        if (!session) {
            return std::nullopt;
        }
    }

    session->SetLootTypeValues(map->GetLootTypeValues());

    auto dog = std::make_unique<model::Dog>(user_name);
    session->AddDog(dog.get());
    
    Player* player = players_.Add(std::move(dog), *session);

    return JoinGameResult{player->GetToken(), player->GetId()};
}

Player* Application::FindByToken(const Token& token) {
    return players_.FindByToken(token);
}

void Application::MovePlayer(Player* player, const std::string& move_cmd) {
    assert(player);
    model::Dog* dog = player->GetDog();
    assert(dog);
    model::GameSession* session = player->GetSession();
    assert(session);
    const auto* map = session->GetMap();
    assert(map);

    double speed_val = map->GetDogSpeed();
    if (speed_val == 0.0) {
        speed_val = game_.GetDefaultDogSpeed();
    }

    model::Vec2D speed{0.0, 0.0};
    std::string direction = dog->GetDirection();

    if (move_cmd == "L") {
        speed.u = -speed_val;
        direction = "L";
    } else if (move_cmd == "R") {
        speed.u = speed_val;
        direction = "R";
    } else if (move_cmd == "U") {
        speed.v = -speed_val;
        direction = "U";
    } else if (move_cmd == "D") {
        speed.v = speed_val;
        direction = "D";
    }

    dog->SetSpeed(speed);
    dog->SetDirection(direction);
}

void Application::Tick(std::chrono::milliseconds delta) {
    game_.Tick(delta);
}

} // namespace app