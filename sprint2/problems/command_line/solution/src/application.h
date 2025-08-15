#pragma once
#include "model.h"
#include "tagged.h"
#include "logger.h"
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <optional>
#include <memory>
#include <chrono>
#include <functional>
#include <cassert>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace net = boost::asio;
namespace sys = boost::system;
using namespace std::literals;

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)}
        , timer_{strand_} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (const std::exception& e) {
                // Логируем стандартное исключение с деталями
                json::value data{{"exception", e.what()}};
                BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data)
                                         << "Ticker handler exception";
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_;
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

namespace app {

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