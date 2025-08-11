#include "audio.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Используем псевдонимы для пространств имен для краткости
namespace net = boost::asio;
using net::ip::udp;
using namespace std::literals;

// Предварительные объявления функций
void StartServer(uint16_t port);
void StartClient(uint16_t port);

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }

    try {
        const std::string mode = argv[1];
        // Преобразуем строковый порт в число
        const uint16_t port = static_cast<uint16_t>(std::stoul(argv[2]));

        if (mode == "server") {
            StartServer(port);
        } else if (mode == "client") {
            StartClient(port);
        } else {
            std::cerr << "Invalid mode. Use 'client' or 'server'." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

void StartServer(uint16_t port) {
    std::cout << "Starting server on port " << port << "..." << std::endl;

    // Инициализируем плеер для 8-битного моно звука
    Player player(ma_format_u8, 1);
    const int frame_size = player.GetFrameSize();
    // Максимальный размер буфера соответствует максимальному числу фреймов
    const size_t max_buffer_bytes = 65000 * frame_size;

    net::io_context io_context;
    // Создаем сокет для прослушивания UDP-сообщений на указанном порту
    udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

    while (true) {
        try {
            std::vector<char> recv_buf(max_buffer_bytes);
            udp::endpoint remote_endpoint;

            std::cout << "Waiting for a message..." << std::endl;

            // Блокирующая операция получения данных
            size_t received_bytes = socket.receive_from(net::buffer(recv_buf), remote_endpoint);

            std::cout << "Received " << received_bytes << " bytes from " << remote_endpoint << ". Playing..." << std::endl;

            if (received_bytes > 0) {
                // Вычисляем количество фреймов на основе полученных байт и размера фрейма
                size_t frames = received_bytes / frame_size;
                player.PlayBuffer(recv_buf.data(), frames, 1.5s);
                std::cout << "Playing done." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Server loop error: " << e.what() << std::endl;
        }
    }
}

void StartClient(uint16_t port) {
    std::cout << "Starting client..." << std::endl;

    // Инициализируем рекордер для 8-битного моно звука
    Recorder recorder(ma_format_u8, 1);
    const int frame_size = recorder.GetFrameSize();

    net::io_context io_context;
    // Создаем UDP-сокет для отправки данных
    udp::socket socket(io_context, udp::v4());

    std::string line; // Для чтения пользовательского ввода

    while (true) {
        try {
            std::cout << "Press Enter to record message..." << std::endl;
            std::getline(std::cin, line);

            // Прекращаем работу, если ввод завершен (например, через Ctrl+D)
            if (std::cin.eof() || std::cin.fail()) {
                break;
            }

            std::cout << "Recording for 1.5s..." << std::endl;
            // Записываем звук
            auto rec_result = recorder.Record(65000, 1.5s);
            std::cout << "Recording done (" << rec_result.frames << " frames)." << std::endl;

            if (rec_result.frames == 0) {
                std::cout << "Nothing recorded, skipping." << std::endl << std::endl;
                continue;
            }

            std::string server_ip;
            std::cout << "Enter server IP address: ";
            std::getline(std::cin, server_ip);

            if (std::cin.eof() || std::cin.fail()) {
                break;
            }

            boost::system::error_code ec;
            // Преобразуем строку с IP-адресом в объект адреса
            auto address = net::ip::make_address(server_ip, ec);
            if (ec) {
                std::cerr << "Invalid IP address: " << server_ip << std::endl << std::endl;
                continue;
            }

            // Создаем конечную endpoint сервера
            udp::endpoint remote_endpoint(address, port);

            // Вычисляем точное количество байт для отправки
            size_t bytes_to_send = rec_result.frames * frame_size;

            std::cout << "Sending " << bytes_to_send << " bytes to " << remote_endpoint << "..." << std::endl;
            // Отправляем данные
            socket.send_to(net::buffer(rec_result.data.data(), bytes_to_send), remote_endpoint);
            std::cout << "Message sent." << std::endl << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Client loop error: " << e.what() << std::endl;
        }
    }
}