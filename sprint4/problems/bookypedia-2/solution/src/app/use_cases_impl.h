#pragma once
#include "../domain/unit_of_work.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::UnitOfWorkFactory& unit_factory)
        : unit_factory_{unit_factory} {
    }

    domain::Author AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() const override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) const override;
    bool RenameAuthor(const domain::AuthorId& author_id, const std::string& new_name) override;
    bool DeleteAuthor(const domain::AuthorId& author_id) override;
    domain::Book AddBook(domain::AuthorId author_id, std::string title, int publication_year,
                         std::vector<std::string> tags) override;
    std::vector<BookSummary> GetBooks() const override;
    std::vector<BookSummary> FindBooksByTitle(const std::string& title) const override;
    std::optional<BookDetails> GetBookById(const domain::BookId& id) const override;
    bool UpdateBook(const domain::BookId& id, std::string title, int publication_year,
                    std::vector<std::string> tags) override;
    bool DeleteBook(const domain::BookId& id) override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override;

private:
    domain::UnitOfWorkFactory& unit_factory_;
};

}  // namespace app
