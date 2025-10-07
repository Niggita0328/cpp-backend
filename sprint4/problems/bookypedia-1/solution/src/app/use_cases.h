#pragma once

#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() const = 0;
    virtual void AddBook(domain::AuthorId author_id, std::string title, int publication_year) = 0;
    virtual std::vector<domain::Book> GetBooks() const = 0;
    virtual std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
