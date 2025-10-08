#pragma once
#include <optional>
#include <string>
#include <vector>

#include "author.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year,
         std::vector<std::string> tags = {})
        : id_(std::move(id))
        , author_id_(std::move(author_id))
        , title_(std::move(title))
        , publication_year_(publication_year)
        , tags_(std::move(tags)) {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

    const std::vector<std::string>& GetTags() const noexcept {
        return tags_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_ = 0;
    std::vector<std::string> tags_;
};

class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> GetAll() const = 0;
    virtual std::vector<Book> GetByAuthor(const AuthorId& author_id) const = 0;
    virtual std::optional<Book> GetById(const BookId& book_id) const = 0;
    virtual std::vector<Book> GetByTitle(const std::string& title) const = 0;
    virtual bool Update(const Book& book) = 0;
    virtual bool Delete(const BookId& book_id) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
