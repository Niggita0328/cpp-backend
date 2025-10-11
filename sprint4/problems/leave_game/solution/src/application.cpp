#include "application.h"
#include "postgres.h"
#include <algorithm>
#include <limits>
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

const model::GameSession* Player::GetSession() const {
    return session_;
}

model::Dog* Player::GetDog() {
    return dog_;
}

const model::Dog* Player::GetDog() const {
    return dog_;
}

void Player::ResetTimers() noexcept {
    play_time_ = std::chrono::milliseconds::zero();
    inactivity_time_ = std::chrono::milliseconds::zero();
}

bool Player::AdvanceTime(std::chrono::milliseconds delta, std::chrono::milliseconds retirement_timeout) {
    play_time_ += delta;
    const auto speed = dog_->GetSpeed();
    if (speed.u == 0.0 && speed.v == 0.0) {
        inactivity_time_ += delta;
        return inactivity_time_ >= retirement_timeout;
    }
    inactivity_time_ = std::chrono::milliseconds::zero();
    return false;
}

std::chrono::milliseconds Player::GetPlayTime() const noexcept {
    return play_time_;
}

std::chrono::milliseconds Player::GetAccumulatedInactivity() const noexcept {
    return inactivity_time_;
}


Player* Players::Add(std::unique_ptr<model::Dog> dog, model::GameSession& session) {
    Token token = GenerateToken();
    
    model::Dog* dog_raw_ptr = dog.get();
    dog_raw_ptr->SetId(model::Dog::Id{next_dog_id_++});
    dogs_.emplace_back(std::move(dog));

    auto player = std::make_unique<Player>(&session, dog_raw_ptr, token);
    Player* player_raw_ptr = player.get();
    player_raw_ptr->ResetTimers();
    players_.emplace_back(std::move(player));
    
    token_to_player_.emplace(player_raw_ptr->GetToken(), player_raw_ptr);
    return player_raw_ptr;
}

Player* Players::AddExisting(std::unique_ptr<model::Dog> dog, model::GameSession& session, Token token) {
    model::Dog* dog_raw_ptr = dog.get();
    dogs_.emplace_back(std::move(dog));

    if (*dog_raw_ptr->GetId() >= next_dog_id_) {
        next_dog_id_ = *dog_raw_ptr->GetId() + 1;
    }

    auto player = std::make_unique<Player>(&session, dog_raw_ptr, std::move(token));
    Player* player_raw_ptr = player.get();
    player_raw_ptr->ResetTimers();
    players_.emplace_back(std::move(player));

    token_to_player_.emplace(player_raw_ptr->GetToken(), player_raw_ptr);
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

const std::vector<std::unique_ptr<Player>>& Players::GetAllPlayers() const noexcept {
    return players_;
}

void Players::Clear() {
    token_to_player_.clear();
    players_.clear();
    dogs_.clear();
    next_dog_id_ = 0;
}

void Players::AddStrayDog(std::unique_ptr<model::Dog> dog) {
    dogs_.emplace_back(std::move(dog));
}

void Players::Remove(Player* player) {
    if (!player) {
        return;
    }
    token_to_player_.erase(player->GetToken());
    auto player_it = std::find_if(players_.begin(), players_.end(),
        [player](const auto& ptr) { return ptr.get() == player; });
    if (player_it != players_.end()) {
        players_.erase(player_it);
    }

    auto dog_ptr = player->GetDog();
    auto dog_it = std::find_if(dogs_.begin(), dogs_.end(),
        [dog_ptr](const auto& ptr) { return ptr.get() == dog_ptr; });
    if (dog_it != dogs_.end()) {
        dogs_.erase(dog_it);
    }
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
    UpdatePlayersActivity(delta);
    game_.Tick(delta);
    tick_signal_(delta);
}

boost::signals2::connection Application::DoOnTick(const TickSignal::slot_type& handler) {
    return tick_signal_.connect(handler);
}

void Application::SetDogRetirementTimeout(std::chrono::milliseconds timeout) noexcept {
    if (timeout.count() < 0) {
        dog_retirement_timeout_ = std::chrono::milliseconds::zero();
    } else {
        dog_retirement_timeout_ = timeout;
    }
}

void Application::UpdatePlayersActivity(std::chrono::milliseconds delta) {
    if (players_.GetAllPlayers().empty()) {
        return;
    }

    std::vector<Player*> retiring_players;
    retiring_players.reserve(players_.GetAllPlayers().size());
    for (const auto& player_ptr : players_.GetAllPlayers()) {
        if (!player_ptr) {
            continue;
        }
        if (player_ptr->AdvanceTime(delta, dog_retirement_timeout_)) {
            retiring_players.push_back(player_ptr.get());
        }
    }

    for (auto* player : retiring_players) {
        RetirePlayer(player);
    }
}

void Application::RetirePlayer(Player* player) {
    if (!player) {
        return;
    }

    model::Dog* dog = player->GetDog();
    model::GameSession* session = player->GetSession();

    std::int64_t play_time_ms = player->GetPlayTime().count();
    if (play_time_ms < 0) {
        play_time_ms = 0;
    }
    const auto max_play_time = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    if (play_time_ms > max_play_time) {
        play_time_ms = max_play_time;
    }

    if (dog && stats_repo_) {
        const std::string& name = dog->GetName();
        std::uint64_t raw_score = dog->GetScore();
        const auto max_int = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        std::int64_t score = static_cast<std::int64_t>(std::min(raw_score, max_int));

        try {
            std::string stored_name = name.substr(0, std::min<std::size_t>(name.size(), 100));
            stats_repo_->SaveRetiredPlayer(stored_name, score, play_time_ms);
        } catch (const std::exception& ex) {
            json::value data{{"player", name}, {"exception", ex.what()}};
            BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data)
                                     << "Failed to persist retired player stats";
        }
    }

    if (session && dog) {
        session->RemoveDog(dog);
    }

    if (dog) {
        json::value data{
            {"player", dog->GetName()},
            {"score", static_cast<std::int64_t>(std::min(dog->GetScore(),
                static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())))},
            {"playTimeMs", play_time_ms}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                                << "Player retired due to inactivity";
    }

    players_.Remove(player);
}

} // namespace app
