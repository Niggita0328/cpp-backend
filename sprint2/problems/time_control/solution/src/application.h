#pragma once
#include "model.h"
#include "tagged.h"
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <optional>
#include <memory>
#include <chrono>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

namespace net = boost::asio;

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

namespace app {

// Forward declaration
class Players;

class Player {
public:
    Player(model::GameSession* session, model::Dog* dog, Token token);
    const Token& GetToken() const;
    model::Dog::Id GetId() const;
    const std::string& GetName() const;
    model::GameSession* GetSession();
    model::Dog* GetDog();

private:
    model::Dog* dog_;
    model::GameSession* session_;
    Token token_;
};

class Players {
public:
    Player* Add(std::unique_ptr<model::Dog> dog, model::GameSession& session);
    Player* FindByToken(const Token& token);
    std::vector<model::Dog> GetDogs() const;

private:
    std::vector<std::unique_ptr<model::Dog>> dogs_;
    std::vector<std::unique_ptr<Player>> players_;
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


struct JoinGameResult {
    Token token;
    model::Dog::Id player_id;
};

class Application {
public:
    explicit Application(model::Game& game, Players& players, net::io_context& ioc);

    const std::vector<model::Map>& ListMaps() const;
    const model::Map* FindMap(const model::Map::Id& id) const;

    std::optional<JoinGameResult> JoinGame(const model::Map::Id& map_id, const std::string& user_name);
    Player* FindByToken(const Token& token);
    void MovePlayer(Player* player, const std::string& move_cmd);
    void Tick(std::chrono::milliseconds delta);

    auto& GetStrand() { return strand_; }

private:
    model::Game& game_;
    Players& players_;
    net::strand<net::io_context::executor_type> strand_;
};

} // namespace app