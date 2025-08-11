#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    if (ec) {
        std::cerr << "Write error: " << ec.message() << std::endl;
        return false;
    }
    return true;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        std::cout << "Game started!" << std::endl;
        bool my_turn = my_initiative;

        while (!IsGameEnded()) {
            if (my_turn) {
                // Наш ход
                PrintFields();
                std::pair<int, int> move;
                // Цикл для получения корректного хода от пользователя
                while (true) {
                    std::cout << "Your turn. Enter coordinates (e.g., A1): ";
                    std::string line;
                    std::getline(std::cin, line);
                    if (std::cin.eof() || std::cin.fail()) {
                        return; // Выход при ошибке или конце ввода
                    }
                    auto parsed_move = ParseMove(line);
                    if (parsed_move) {
                        move = *parsed_move;
                        break;
                    }
                    std::cout << "Invalid coordinates. Try again." << std::endl;
                }

                // Отправляем ход
                if (!WriteExact(socket, MoveToString(move))) {
                    break; // Прерываем игру при ошибке отправки
                }

                // Читаем результат
                auto response = ReadExact<1>(socket);
                if (!response) {
                    std::cout << "Connection lost." << std::endl;
                    break;
                }

                auto result = static_cast<SeabattleField::ShotResult>((*response)[0]);
                
                // Обновляем поле противника и выводим результат
                switch (result) {
                    case SeabattleField::ShotResult::MISS:
                        std::cout << "-> MISS" << std::endl;
                        other_field_.MarkMiss(move.first, move.second);
                        my_turn = false; // Передаем ход
                        break;
                    case SeabattleField::ShotResult::HIT:
                        std::cout << "-> HIT!" << std::endl;
                        other_field_.MarkHit(move.first, move.second);
                        break;
                    case SeabattleField::ShotResult::KILL:
                        std::cout << "-> KILL!!!" << std::endl;
                        other_field_.MarkKill(move.first, move.second);
                        break;
                }

            } else {
                // Ход противника
                std::cout << "Waiting for opponent's move..." << std::endl;
                auto move_str = ReadExact<2>(socket);
                if (!move_str) {
                    std::cout << "Connection lost." << std::endl;
                    break;
                }
                
                auto move = *ParseMove(*move_str);
                
                // Выполняем выстрел по нашему полю
                auto result = my_field_.Shoot(move.first, move.second);
                
                // Отправляем результат
                char result_char = static_cast<char>(result);
                if (!WriteExact(socket, {&result_char, 1})) {
                    break;
                }
                
                std::cout << "Opponent shoots at " << *move_str;
                switch (result) {
                    case SeabattleField::ShotResult::MISS:
                        std::cout << ". MISS" << std::endl;
                        my_turn = true; // Забираем ход
                        break;
                    case SeabattleField::ShotResult::HIT:
                        std::cout << ". HIT!" << std::endl;
                        break;
                    case SeabattleField::ShotResult::KILL:
                        std::cout << ". KILL!!!" << std::endl;
                        break;
                }
            }
        }
        
        // Определение победителя
        PrintFields();
        if (my_field_.IsLoser()) {
            std::cout << "You lose." << std::endl;
        } else if (other_field_.IsLoser()) {
            std::cout << "You win!" << std::endl;
        } else {
            std::cout << "Game interrupted." << std::endl;
        }
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = toupper(sv[0]) - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 >= static_cast<int>(SeabattleField::field_size)) return std::nullopt;
        if (p2 < 0 || p2 >= static_cast<int>(SeabattleField::field_size)) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first + 'A'), static_cast<char>(move.second + '1')};
        return {buff, 2};
    }

    void PrintFields() const {
        std::cout << "My field:" << "                " << "Opponent's field:" << std::endl;
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);
    
    net::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Waiting for connection on port " << port << "..." << std::endl;

    try {
        tcp::socket socket = acceptor.accept();
        std::cout << "Client connected." << std::endl;
        agent.StartGame(socket, false); // Сервер ходит вторым
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);
    
    net::io_context io_context;
    tcp::socket socket(io_context);

    try {
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(ip_str, std::to_string(port));
        std::cout << "Connecting to " << ip_str << ":" << port << "..." << std::endl;
        net::connect(socket, endpoints);
        std::cout << "Connected to server." << std::endl;
        agent.StartGame(socket, true); // Клиент ходит первым
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: " << std::endl;
        std::cout << "  server: " << argv[0] << " <seed> <port>" << std::endl;
        std::cout << "  client: " << argv[0] << " <seed> <ip> <port>" << std::endl;
        return 1;
    }

    try {
        std::mt19937 engine(std::stoi(argv[1]));
        SeabattleField fieldL = SeabattleField::GetRandomField(engine);

        if (argc == 3) {
            unsigned short port = std::stoi(argv[2]);
            StartServer(fieldL, port);
        } else if (argc == 4) {
            std::string ip = argv[2];
            unsigned short port = std::stoi(argv[3]);
            StartClient(fieldL, ip, port);
        }
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}