#include "postgres.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cassert>
#include <stdexcept>
#include <string_view>

namespace db {

namespace {

constexpr std::string_view kDuplicatePreparedStatement = "42P05";

void PrepareStatement(pqxx::connection& conn, const char* name, const char* query) {
    try {
        conn.prepare(name, query);
    } catch (const pqxx::sql_error& e) {
        if (e.sqlstate() != kDuplicatePreparedStatement) {
            throw;
        }
    }
}

}  // namespace

ConnectionPool::ConnectionWrapper::ConnectionWrapper(ConnectionPtr&& conn, ConnectionPool& pool) noexcept
    : conn_{std::move(conn)}
    , pool_{&pool} {
}

ConnectionPool::ConnectionWrapper::~ConnectionWrapper() {
    if (conn_) {
        pool_->ReturnConnection(std::move(conn_));
    }
}

pqxx::connection& ConnectionPool::ConnectionWrapper::operator*() const noexcept {
    assert(conn_);
    return *conn_;
}

pqxx::connection* ConnectionPool::ConnectionWrapper::operator->() const noexcept {
    assert(conn_);
    return conn_.get();
}

ConnectionPool::ConnectionWrapper ConnectionPool::GetConnection() {
    std::unique_lock lock{mutex_};
    cond_var_.wait(lock, [this] {
        return used_connections_ < pool_.size();
    });

    return ConnectionWrapper{std::move(pool_[used_connections_++]), *this};
}

void ConnectionPool::ReturnConnection(ConnectionPtr&& conn) {
    {
        std::lock_guard guard{mutex_};
        assert(used_connections_ != 0);
        pool_[--used_connections_] = std::move(conn);
    }
    cond_var_.notify_one();
}

StatisticsRepository::StatisticsRepository(ConnectionPool& pool)
    : pool_{pool} {
}

void StatisticsRepository::EnsureStatementsPrepared(pqxx::connection& conn) {
    PrepareStatement(
        conn,
        "insert_retired_player",
        "INSERT INTO retired_players (id, name, score, play_time_ms) "
        "VALUES ($1::uuid, $2, $3, $4)"
    );

    PrepareStatement(
        conn,
        "select_retired_players",
        "SELECT name, score, play_time_ms "
        "FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "OFFSET $1 LIMIT $2"
    );
}

void StatisticsRepository::InitSchema() {
    auto conn = pool_.GetConnection();
    pqxx::work tx{*conn};

    tx.exec(
        "CREATE TABLE IF NOT EXISTS retired_players ("
        "id UUID PRIMARY KEY,"
        "name VARCHAR(100) NOT NULL,"
        "score INTEGER NOT NULL,"
        "play_time_ms INTEGER NOT NULL)"
    );

    tx.exec(
        "CREATE INDEX IF NOT EXISTS retired_players_order_idx "
        "ON retired_players (score DESC, play_time_ms ASC, name ASC)"
    );

    tx.commit();

    EnsureStatementsPrepared(*conn);
}

void StatisticsRepository::SaveRetiredPlayer(std::string_view name, std::int64_t score, std::int64_t play_time_ms) {
    auto conn = pool_.GetConnection();
    EnsureStatementsPrepared(*conn);

    boost::uuids::uuid id = boost::uuids::random_generator()();
    std::string id_str = boost::uuids::to_string(id);

    pqxx::work tx{*conn};
    const auto stored_score = static_cast<int>(score);
    const auto stored_time = static_cast<int>(play_time_ms);
    tx.exec_prepared("insert_retired_player", id_str, std::string{name}, stored_score, stored_time);
    tx.commit();
}

std::vector<PlayerRecord> StatisticsRepository::FetchRecords(std::size_t start, std::size_t max_items) const {
    auto conn = pool_.GetConnection();
    EnsureStatementsPrepared(*conn);

    pqxx::read_transaction tx{*conn};
    auto result = tx.exec_prepared("select_retired_players", static_cast<std::int64_t>(start), static_cast<std::int64_t>(max_items));

    std::vector<PlayerRecord> records;
    records.reserve(result.size());
    for (const auto& row : result) {
        PlayerRecord record;
        record.name = row[0].c_str();
        record.score = row[1].as<std::int64_t>();
        record.play_time_ms = row[2].as<std::int64_t>();
        records.push_back(std::move(record));
    }
    return records;
}

}  // namespace db
