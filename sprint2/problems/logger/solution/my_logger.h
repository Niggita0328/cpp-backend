#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return ss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Выведите в поток все аргументы.
    template<class... Ts>
    void Log(const Ts&... args) {
        // Захватываем мьютекс для обеспечения потокобезопасности
        std::lock_guard lock(mutex_);

        // Формируем имя файла на основе текущей временной метки
        std::string filename_by_ts = "/var/log/sample_log_"s + GetFileTimeStamp() + ".log"s;

        // Если дата изменилась или файл еще не открыт, открываем новый файл
        if (!log_file_.is_open() || filename_by_ts != current_filename_) {
            if (log_file_.is_open()) {
                log_file_.close();
            }
            current_filename_ = filename_by_ts;
            // Открываем файл в режиме дозаписи
            log_file_.open(current_filename_, std::ios::app);
        }
        
        // Выводим временную метку
        log_file_ << GetTimeStamp() << ": ";

        // Используем fold expression из C++17 для вывода всех аргументов в поток
        (log_file_ << ... << args);

        // Завершаем строку и сбрасываем буфер
        log_file_ << std::endl;
    }

    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть 
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        // Захватываем мьютекс для потокобезопасного изменения manual_ts_
        std::lock_guard lock(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;

    // Приватные поля для работы логгера
    std::mutex mutex_;
    std::ofstream log_file_;
    std::string current_filename_;
};