#include "postgres.h"

#include <pqxx/result.hxx>
#include <pqxx/row.hxx>
#include <pqxx/zview.hxx>
#include <string>
#include <vector>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec(R"(
SELECT id, name FROM authors ORDER BY name ASC;
)"_zv);

    std::vector<domain::Author> authors;
    authors.reserve(rows.size());
    for (pqxx::result::size_type i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const auto id = domain::AuthorId::FromString(row["id"].c_str());
        std::string name = row["name"].c_str();
        authors.emplace_back(id, std::move(name));
    }
    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year)
VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec(R"(
SELECT id, author_id, title, publication_year
FROM books
ORDER BY title ASC;
)"_zv);

    std::vector<domain::Book> books;
    books.reserve(rows.size());
    for (pqxx::result::size_type i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const auto id = domain::BookId::FromString(row["id"].c_str());
        const auto author_id = domain::AuthorId::FromString(row["author_id"].c_str());
        std::string title = row["title"].c_str();
        int publication_year = row["publication_year"].as<int>();
        books.emplace_back(id, author_id, std::move(title), publication_year);
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& author_id) const {
    pqxx::read_transaction tx{connection_};
    auto rows = tx.exec_params(
        R"(
SELECT id, author_id, title, publication_year
FROM books
WHERE author_id = $1
ORDER BY publication_year ASC, title ASC;
)"_zv,
        author_id.ToString());

    std::vector<domain::Book> books;
    books.reserve(rows.size());
    for (pqxx::result::size_type i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const auto id = domain::BookId::FromString(row["id"].c_str());
        const auto row_author_id = domain::AuthorId::FromString(row["author_id"].c_str());
        std::string title = row["title"].c_str();
        int publication_year = row["publication_year"].as<int>();
        books.emplace_back(id, row_author_id, std::move(title), publication_year);
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id),
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);

    work.commit();
}

}  // namespace postgres
