#pragma once
#include "model.h"
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

namespace model {

class GameSession {
public:
    GameSession(const Map::Id& map_id, model::Dog* dog) : map_id_(map_id), dog_(dog) {}
    const Map::Id& GetMapId() const { return map_id_; }
    model::Dog* GetDog() { return dog_; }
private:
    model::Dog* dog_;
    Map::Id map_id_;
};

class Player {
public:
    Player(GameSession* session, Token token) : session_(session), token_(token) {}
    GameSession* GetSession() { return session_; }
    const Token& GetToken() const { return token_; }
private:
    GameSession* session_;
    Token token_;
};

class Players {
public:
    std::pair<Token, model::Dog::Id> Add(model::Dog& dog, GameSession& session);
    Player* FindByToken(const Token& token);
    const std::vector<model::Dog>& GetDogs() const;
    const std::vector<GameSession>& GetSessions() const;

private:
    std::vector<model::Dog> dogs_;
    std::vector<GameSession> sessions_;
    std::vector<Player> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;

    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    Token GenerateToken();
};

} // namespace model