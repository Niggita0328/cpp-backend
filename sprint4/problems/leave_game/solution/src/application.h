#pragma once
#include "model.h"
#include "tagged.h"
#include "extra_data.h"
#include "logger.h"
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <optional>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/connection.hpp>
#include <cassert>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace net = boost::asio;
namespace sys = boost::system;
using namespace std::literals;

namespace db {
class StatisticsRepository;
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , timer_{strand_}
        , handler_{std::move(handler)} {
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
    const model::GameSession* GetSession() const;
    model::Dog* GetDog();
    const model::Dog* GetDog() const;
    void ResetTimers() noexcept;
    bool AdvanceTime(std::chrono::milliseconds delta, std::chrono::milliseconds retirement_timeout);
    std::chrono::milliseconds GetPlayTime() const noexcept;
    std::chrono::milliseconds GetAccumulatedInactivity() const noexcept;

private:
    model::Dog* dog_;
    model::GameSession* session_;
    Token token_;
    std::chrono::milliseconds play_time_{0};
    std::chrono::milliseconds inactivity_time_{0};
};

class Players {
public:
    Player* Add(std::unique_ptr<model::Dog> dog, model::GameSession& session);
    Player* AddExisting(std::unique_ptr<model::Dog> dog, model::GameSession& session, Token token);
    Player* FindByToken(const Token& token);
    std::vector<model::Dog> GetDogs() const;
    const std::vector<std::unique_ptr<Player>>& GetAllPlayers() const noexcept;
    void Clear();
    void AddStrayDog(std::unique_ptr<model::Dog> dog);
    void Remove(Player* player);

private:
    std::vector<std::unique_ptr<model::Dog>> dogs_;
    std::vector<std::unique_ptr<Player>> players_;
    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
    std::uint64_t next_dog_id_ = 0;

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
    using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds delta)>;

    explicit Application(model::Game& game, Players& players, extra_data::MapRepository& extra_data, net::io_context& ioc);

    const std::vector<model::Map>& ListMaps() const;
    const model::Map* FindMap(const model::Map::Id& id) const;
    const extra_data::MapData* GetMapExtra(const model::Map::Id& id) const;

    std::optional<JoinGameResult> JoinGame(const model::Map::Id& map_id, const std::string& user_name);
    Player* FindByToken(const Token& token);
    void MovePlayer(Player* player, const std::string& move_cmd);
    void Tick(std::chrono::milliseconds delta);
    [[nodiscard]] boost::signals2::connection DoOnTick(const TickSignal::slot_type& handler);

    auto& GetStrand() { return strand_; }
    void SetDogRetirementTimeout(std::chrono::milliseconds timeout) noexcept;
    std::chrono::milliseconds GetDogRetirementTimeout() const noexcept { return dog_retirement_timeout_; }
    void SetStatisticsRepository(db::StatisticsRepository* repository) noexcept { stats_repo_ = repository; }
    db::StatisticsRepository* GetStatisticsRepository() const noexcept { return stats_repo_; }

private:
    void UpdatePlayersActivity(std::chrono::milliseconds delta);
    void RetirePlayer(Player* player);

    model::Game& game_;
    Players& players_;
    extra_data::MapRepository& extra_data_;
    net::strand<net::io_context::executor_type> strand_;
    TickSignal tick_signal_;
    std::chrono::milliseconds dog_retirement_timeout_{std::chrono::minutes(1)};
    db::StatisticsRepository* stats_repo_ = nullptr;
};

} // namespace app
#include <cstdint>
