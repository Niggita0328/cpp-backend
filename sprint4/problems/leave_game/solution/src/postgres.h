#pragma once

#include <pqxx/pqxx>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace db {

class ConnectionPool {
public:
    using ConnectionPtr = std::shared_ptr<pqxx::connection>;

    class ConnectionWrapper {
    public:
        ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept;
        ConnectionWrapper(const ConnectionWrapper&) = delete;
        ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
        ConnectionWrapper(ConnectionWrapper&&) noexcept = default;
        ConnectionWrapper& operator=(ConnectionWrapper&&) noexcept = default;
        ~ConnectionWrapper();

        pqxx::connection& operator*() const noexcept;
        pqxx::connection* operator->() const noexcept;

    private:
        ConnectionPtr conn_;
        ConnectionPool* pool_;
    };

    template <typename ConnectionFactory>
    ConnectionPool(std::size_t capacity, ConnectionFactory&& connection_factory) {
        pool_.reserve(capacity);
        for (std::size_t i = 0; i < capacity; ++i) {
            pool_.emplace_back(connection_factory());
        }
    }

    ConnectionWrapper GetConnection();

private:
    void ReturnConnection(ConnectionPtr&& conn);

    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::vector<ConnectionPtr> pool_;
    std::size_t used_connections_ = 0;
};

struct PlayerRecord {
    std::string name;
    std::int64_t score = 0;
    std::int64_t play_time_ms = 0;
};

class StatisticsRepository {
public:
    explicit StatisticsRepository(ConnectionPool& pool);

    void InitSchema();
    void SaveRetiredPlayer(std::string_view name, std::int64_t score, std::int64_t play_time_ms);
    [[nodiscard]] std::vector<PlayerRecord> FetchRecords(std::size_t start, std::size_t max_items) const;

private:
    static void EnsureStatementsPrepared(pqxx::connection& conn);

    ConnectionPool& pool_;
};

}  // namespace db

