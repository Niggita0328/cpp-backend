#include "sdk.h"
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/program_options.hpp>
#include <boost/signals2/connection.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono> 
#include <utility>
#include <optional>
#include <algorithm>
#include <cstdlib>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h" 
#include "application.h"
#include "model_serialization.h"
#include "serializing_listener.h"
#include "postgres.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace json = boost::json;
namespace http = boost::beast::http;
namespace po = boost::program_options;

namespace {

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<uint64_t> tick_period;
    bool randomize_spawn_points = false;
    std::optional<std::filesystem::path> state_file;
    std::optional<uint64_t> save_state_period;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc{"Allowed options"};
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir"), "set static files root")
        ("state-file", po::value<std::string>()->value_name("file"), "set state file path")
        ("save-state-period", po::value<uint64_t>()->value_name("milliseconds"), "set state auto-save period")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc;
        return std::nullopt;
    }

    if (!vm.count("config-file")) {
        throw std::runtime_error("Config file path is not specified");
    }
    if (!vm.count("www-root")) {
        throw std::runtime_error("Static files root is not specified");
    }

    Args args;
    args.config_file = vm.at("config-file").as<std::string>();
    args.www_root = vm.at("www-root").as<std::string>();
    if (vm.count("tick-period")) {
        args.tick_period = vm.at("tick-period").as<uint64_t>();
    }
    if (vm.count("state-file")) {
        args.state_file = std::filesystem::path{vm.at("state-file").as<std::string>()};
    }
    if (vm.count("save-state-period")) {
        args.save_state_period = vm.at("save-state-period").as<uint64_t>();
    }

    return args;
}


// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    InitBoostLog();

    try {
        auto args_opt = ParseCommandLine(argc, argv);
        if (!args_opt) {
            return EXIT_SUCCESS;
        }
        const Args& args = *args_opt;

        // 1. Загружаем карту из файла и построить модель игры
        auto loaded_data = json_loader::LoadGame(args.config_file);
        model::Game game = std::move(loaded_data.game);
        game.SetRandomizeSpawn(args.randomize_spawn_points);
        auto extra_data = std::move(loaded_data.extra_data);
        
        // 2. Инициализируем io_context
        const unsigned hw_threads = std::thread::hardware_concurrency();
        const unsigned num_threads = std::max(1u, hw_threads);
        net::io_context ioc(num_threads);
        
        // 3. Инициализируем приложение
        app::Players players; 
        app::Application app{game, players, extra_data, ioc};
        app.SetDogRetirementTimeout(loaded_data.dog_retirement_timeout);

        // 4. Инициализируем соединение с базой данных
        const char* db_url_env = std::getenv("GAME_DB_URL");
        if (!db_url_env) {
            throw std::runtime_error("GAME_DB_URL environment variable is not set");
        }
        const std::string db_url{db_url_env};
        const std::size_t pool_size = std::max<std::size_t>(1, static_cast<std::size_t>(num_threads));

        db::ConnectionPool connection_pool(pool_size, [db_url]() {
            auto conn = std::make_shared<pqxx::connection>(db_url);
            return conn;
        });
        db::StatisticsRepository stats_repo(connection_pool);
        stats_repo.InitSchema();
        app.SetStatisticsRepository(&stats_repo);
        
        // 5. Инициализируем обработчик состояний игры
        std::shared_ptr<serialization::SerializingListener> serializing_listener;
        boost::signals2::scoped_connection serializing_connection;
        bool serialization_failed = false;

        if (args.state_file) {
            const auto& state_path = *args.state_file;
            if (std::filesystem::exists(state_path)) {
                try {
                    auto saved_state = serialization::LoadGameState(state_path);
                    serialization::ApplyGameState(saved_state, game, players);
                } catch (const std::exception& ex) {
                    json::value data{{"state_file", state_path.string()}, {"exception", ex.what()}};
                    BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data)
                                            << "failed to load saved state";
                    return EXIT_FAILURE;
                }
            }

            std::optional<serialization::SerializingListener::Milliseconds> auto_save_period;
            if (args.save_state_period) {
                auto_save_period = serialization::SerializingListener::Milliseconds{*args.save_state_period};
            }

            serializing_listener = std::make_shared<serialization::SerializingListener>(
                game,
                players,
                state_path,
                auto_save_period,
                [&](const std::exception&) {
                    serialization_failed = true;
                    ioc.stop();
                });

            auto weak_listener = std::weak_ptr<serialization::SerializingListener>(serializing_listener);
            serializing_connection = boost::signals2::scoped_connection{
                app.DoOnTick([weak_listener](std::chrono::milliseconds delta) {
                    if (auto listener = weak_listener.lock()) {
                        listener->OnTick(delta);
                    }
                })
            };
        }

        // 6. Запускаем игру
        if (args.tick_period) {
            auto ticker = std::make_shared<Ticker>(
                app.GetStrand(),
                std::chrono::milliseconds{*args.tick_period},
                [&app](std::chrono::milliseconds delta){ app.Tick(delta); }
            );
            ticker->Start();
        }

        // Каталог со статическими файлами
        std::filesystem::path static_root{args.www_root};
        if (!std::filesystem::is_directory(static_root)) {
            std::cerr << "Static root is not a directory or doesn't exist" << std::endl;
            return EXIT_FAILURE;
        }

        // 7. Обработка сигналов SIGINT и SIGTERM
        std::weak_ptr<serialization::SerializingListener> serializing_listener_weak{serializing_listener};
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &app, serializing_listener_weak](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                if (auto listener = serializing_listener_weak.lock()) {
                    net::dispatch(app.GetStrand(), [listener, &ioc]() {
                        listener->SaveState();
                        ioc.stop();
                    });
                } else {
                    ioc.stop();
                }
            }
        });

        // 8. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{app, static_root, !args.tick_period.has_value()};

        // 9. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        auto logging_handler = [&handler](auto&& req, auto&& send, const auto& remote_ep) {
            using namespace std::chrono;
            auto start_time = steady_clock::now();

            json::value req_data{
                {"ip", remote_ep.address().to_string()},
                {"URI", std::string(req.target())},
                {"method", std::string(req.method_string())}
            };
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, req_data)
                                    << "request received"sv;

            auto logging_send = [send = std::forward<decltype(send)>(send), start_time](auto&& response) {
                auto end_time = steady_clock::now();
                auto resp_time_ms = duration_cast<milliseconds>(end_time - start_time);

                json::value content_type = nullptr;
                if(response.find(http::field::content_type) != response.end()) {
                    content_type = std::string(response.at(http::field::content_type));
                }

                json::value resp_data{
                    {"response_time", resp_time_ms.count()},
                    {"code", response.result_int()},
                    {"content_type", content_type}
                };
                BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, resp_data)
                                        << "response sent"sv;
                
                send(std::forward<decltype(response)>(response));
            };
            // Передаем запрос и новый обработчик для отправки ответа в основной handler
            handler(std::forward<decltype(req)>(req), logging_send);
        };
        
        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        // Сообщение о запуске сервера
        json::value start_data{{"port", port}, {"address", address.to_string()}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data)
                                << "server started"sv;

        // 10. Запускаем обработку асинхронных операций
        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        if (serialization_failed) {
            return EXIT_FAILURE;
        }

        json::value exit_data{{"code", 0}};
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exit_data)
                                << "server exited"sv;

    } catch (const std::exception& ex) {
        json::value exit_data{{"code", EXIT_FAILURE}, {"exception", ex.what()}};
        BOOST_LOG_TRIVIAL(fatal) << logging::add_value(additional_data, exit_data)
                                << "server exited"sv;
        return EXIT_FAILURE;
    }
}
