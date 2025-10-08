#pragma once
#include <memory>
#include <optional>
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
#include "../domain/unit_of_work.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::transaction_base& tx)
        : tx_{tx} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() const override;
    std::optional<domain::Author> GetById(const domain::AuthorId& id) const override;
    std::optional<domain::Author> GetByName(const std::string& name) const override;
    bool UpdateName(const domain::AuthorId& id, const std::string& name) override;
    bool Delete(const domain::AuthorId& id) override;

private:
    pqxx::transaction_base& tx_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::transaction_base& tx)
        : tx_{tx} {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) const override;
    std::optional<domain::Book> GetById(const domain::BookId& book_id) const override;
    std::vector<domain::Book> GetByTitle(const std::string& title) const override;
    bool Update(const domain::Book& book) override;
    bool Delete(const domain::BookId& book_id) override;

private:
    pqxx::transaction_base& tx_;
};

class Database : public domain::UnitOfWorkFactory {
public:
    explicit Database(pqxx::connection connection);

    std::unique_ptr<domain::UnitOfWork> CreateUnitOfWork() const override;

private:
    mutable pqxx::connection connection_;
};

}  // namespace postgres
