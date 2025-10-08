#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

struct BookSummary {
    domain::BookId id;
    std::string title;
    std::string author_name;
    int publication_year = 0;
};

struct BookDetails {
    domain::BookId id;
    std::string title;
    std::string author_name;
    int publication_year = 0;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual domain::Author AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() const = 0;
    virtual std::optional<domain::Author> GetAuthorByName(const std::string& name) const = 0;
    virtual bool RenameAuthor(const domain::AuthorId& author_id, const std::string& new_name) = 0;
    virtual bool DeleteAuthor(const domain::AuthorId& author_id) = 0;
    virtual domain::Book AddBook(domain::AuthorId author_id, std::string title, int publication_year,
                                 std::vector<std::string> tags) = 0;
    virtual std::vector<BookSummary> GetBooks() const = 0;
    virtual std::vector<BookSummary> FindBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<BookDetails> GetBookById(const domain::BookId& id) const = 0;
    virtual bool UpdateBook(const domain::BookId& id, std::string title, int publication_year,
                            std::vector<std::string> tags) = 0;
    virtual bool DeleteBook(const domain::BookId& id) = 0;
    virtual std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
