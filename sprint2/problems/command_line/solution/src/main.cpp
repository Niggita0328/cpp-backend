#include "sdk.h"
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono> 
#include <optional>

#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "logger.h" 
#include "application.h"

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
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc{"Allowed options"};
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<uint64_t>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir"), "set static files root")
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
    if (vm.count("randomize-spawn-points")) {
        args.randomize_spawn_points = true;
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
        model::Game game = json_loader::LoadGame(args.config_file);
        game.SetRandomizeSpawn(args.randomize_spawn_points);
        
        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        app::Players players; 
        app::Application app{game, players, ioc};
        
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

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{app, static_root, !args.tick_period.has_value()};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
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

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

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