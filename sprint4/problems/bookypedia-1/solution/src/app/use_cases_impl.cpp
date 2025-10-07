#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

#include <stdexcept>
#include <utility>

namespace app {
using namespace domain;

namespace {

void EnsureNotEmpty(const std::string& value, const char* message) {
    if (value.empty()) {
        throw std::invalid_argument(message);
    }
}

}  // namespace

void UseCasesImpl::AddAuthor(const std::string& name) {
    EnsureNotEmpty(name, "Author name must not be empty");
    authors_.Save({AuthorId::New(), name});
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
    return authors_.GetAll();
}

void UseCasesImpl::AddBook(AuthorId author_id, std::string title, int publication_year) {
    EnsureNotEmpty(title, "Book title must not be empty");
    books_.Save({BookId::New(), std::move(author_id), std::move(title), publication_year});
}

std::vector<Book> UseCasesImpl::GetBooks() const {
    return books_.GetAll();
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const AuthorId& author_id) const {
    return books_.GetByAuthor(author_id);
}

}  // namespace app
