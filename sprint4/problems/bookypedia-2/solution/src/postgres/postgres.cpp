#include "postgres.h"

#include <algorithm>
#include <memory>
#include <pqxx/field.hxx>
#include <pqxx/result>
#include <pqxx/result.hxx>
#include <pqxx/row.hxx>
#include <pqxx/transaction_base.hxx>
#include <pqxx/zview.hxx>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

namespace {

struct BookRowData {
    domain::BookId id;
    std::string id_str;
    domain::AuthorId author_id;
    std::string title;
    int publication_year = 0;
};

using TagsMap = std::unordered_map<std::string, std::vector<std::string>>;

std::vector<BookRowData> ParseBookRows(const pqxx::result& rows) {
    std::vector<BookRowData> books;
    books.reserve(rows.size());
    for (const auto& row : rows) {
        BookRowData data{
            domain::BookId::FromString(row["id"].c_str()),
            row["id"].c_str(),
            domain::AuthorId::FromString(row["author_id"].c_str()),
            row["title"].c_str(),
            row["publication_year"].as<int>()};
        books.push_back(std::move(data));
    }
    return books;
}

TagsMap LoadTags(pqxx::transaction_base& tx, const std::vector<BookRowData>& books) {
    TagsMap map;
    if (books.empty()) {
        return map;
    }

    std::string query{"SELECT book_id, tag FROM book_tags WHERE book_id IN ("};
    bool first = true;
    for (const auto& data : books) {
        if (!first) {
            query += ", ";
        }
        first = false;
        query += "'"s + tx.esc(data.id_str) + "'";
    }
    query += ") ORDER BY book_id ASC, tag ASC;";

    auto rows = tx.exec(query);
    for (const auto& row : rows) {
        const std::string book_id = row["book_id"].c_str();
        map[book_id].push_back(row["tag"].c_str());
    }
    return map;
}

std::vector<domain::Book> AssembleBooks(const std::vector<BookRowData>& books, const TagsMap& tags) {
    std::vector<domain::Book> result;
    result.reserve(books.size());
    for (const auto& data : books) {
        auto tags_it = tags.find(data.id_str);
        std::vector<std::string> book_tags;
        if (tags_it != tags.end()) {
            book_tags = tags_it->second;
        }
        result.emplace_back(data.id, data.author_id, data.title, data.publication_year, std::move(book_tags));
    }
    return result;
}

void ReplaceTags(pqxx::transaction_base& tx, const domain::BookId& book_id,
                 const std::vector<std::string>& tags) {
    tx.exec_params(R"(DELETE FROM book_tags WHERE book_id = $1;)"_zv, book_id.ToString());
    for (const auto& tag : tags) {
        tx.exec_params(R"(INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);)"_zv, book_id.ToString(), tag);
    }
}

class UnitOfWorkImpl : public domain::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection)
        : transaction_{std::make_unique<pqxx::work>(connection)}
        , authors_{*transaction_}
        , books_{*transaction_} {
    }

    domain::AuthorRepository& Authors() override {
        return authors_;
    }

    domain::BookRepository& Books() override {
        return books_;
    }

    void Commit() override {
        if (committed_) {
            return;
        }
        transaction_->commit();
        committed_ = true;
    }

    ~UnitOfWorkImpl() override {
        if (!committed_) {
            try {
                transaction_->abort();
            } catch (...) {
                // suppress all exceptions in destructor
            }
        }
    }

private:
    std::unique_ptr<pqxx::work> transaction_;
    AuthorRepositoryImpl authors_;
    BookRepositoryImpl books_;
    bool committed_ = false;
};

}  // namespace

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    tx_.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;
)"_zv,
        author.GetId().ToString(), author.GetName());
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    auto rows = tx_.exec(R"(
SELECT id, name FROM authors ORDER BY name ASC;
)"_zv);

    std::vector<domain::Author> authors;
    authors.reserve(rows.size());
    for (const auto& row : rows) {
        authors.emplace_back(domain::AuthorId::FromString(row["id"].c_str()), row["name"].c_str());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetById(const domain::AuthorId& id) const {
    auto rows = tx_.exec_params(R"(
SELECT id, name FROM authors WHERE id = $1;
)"_zv,
                                   id.ToString());
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& row = rows.front();
    return domain::Author{domain::AuthorId::FromString(row["id"].c_str()), row["name"].c_str()};
}

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    auto rows = tx_.exec_params(R"(
SELECT id, name FROM authors WHERE name = $1;
)"_zv,
                                   name);
    if (rows.empty()) {
        return std::nullopt;
    }
    const auto& row = rows.front();
    return domain::Author{domain::AuthorId::FromString(row["id"].c_str()), row["name"].c_str()};
}

bool AuthorRepositoryImpl::UpdateName(const domain::AuthorId& id, const std::string& name) {
    auto res = tx_.exec_params(
        R"(
UPDATE authors SET name = $2 WHERE id = $1;
)"_zv,
        id.ToString(), name);
    return res.affected_rows() > 0;
}

bool AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    auto res = tx_.exec_params(
        R"(
DELETE FROM authors WHERE id = $1;
)"_zv,
        id.ToString());
    return res.affected_rows() > 0;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    tx_.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year)
VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE
SET author_id = EXCLUDED.author_id,
    title = EXCLUDED.title,
    publication_year = EXCLUDED.publication_year;
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    ReplaceTags(tx_, book.GetId(), book.GetTags());
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    auto rows = tx_.exec(R"(
SELECT id, author_id, title, publication_year
FROM books;
)"_zv);
    auto parsed = ParseBookRows(rows);
    auto tags = LoadTags(tx_, parsed);
    return AssembleBooks(parsed, tags);
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& author_id) const {
    auto rows = tx_.exec_params(
        R"(
SELECT id, author_id, title, publication_year
FROM books
WHERE author_id = $1;
)"_zv,
        author_id.ToString());
    auto parsed = ParseBookRows(rows);
    auto tags = LoadTags(tx_, parsed);
    return AssembleBooks(parsed, tags);
}

std::optional<domain::Book> BookRepositoryImpl::GetById(const domain::BookId& book_id) const {
    auto rows = tx_.exec_params(
        R"(
SELECT id, author_id, title, publication_year
FROM books
WHERE id = $1;
)"_zv,
        book_id.ToString());
    if (rows.empty()) {
        return std::nullopt;
    }
    auto parsed = ParseBookRows(rows);
    auto tags = LoadTags(tx_, parsed);
    auto books = AssembleBooks(parsed, tags);
    return books.front();
}

std::vector<domain::Book> BookRepositoryImpl::GetByTitle(const std::string& title) const {
    auto rows = tx_.exec_params(
        R"(
SELECT id, author_id, title, publication_year
FROM books
WHERE title = $1;
)"_zv,
        title);
    auto parsed = ParseBookRows(rows);
    auto tags = LoadTags(tx_, parsed);
    return AssembleBooks(parsed, tags);
}

bool BookRepositoryImpl::Update(const domain::Book& book) {
    auto res = tx_.exec_params(
        R"(
UPDATE books
SET author_id = $2, title = $3, publication_year = $4
WHERE id = $1;
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(), book.GetPublicationYear());
    if (res.affected_rows() == 0) {
        return false;
    }
    ReplaceTags(tx_, book.GetId(), book.GetTags());
    return true;
}

bool BookRepositoryImpl::Delete(const domain::BookId& book_id) {
    tx_.exec_params(
        R"(
DELETE FROM book_tags WHERE book_id = $1;
)"_zv,
        book_id.ToString());
    auto res = tx_.exec_params(
        R"(
DELETE FROM books WHERE id = $1;
)"_zv,
        book_id.ToString());
    return res.affected_rows() > 0;
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
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    CONSTRAINT book_tags_pk PRIMARY KEY (book_id, tag)
);
)"_zv);
    work.commit();
}

std::unique_ptr<domain::UnitOfWork> Database::CreateUnitOfWork() const {
    return std::make_unique<UnitOfWorkImpl>(connection_);
}

}  // namespace postgres
