#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace app {
using namespace domain;

namespace {

void EnsureNotEmpty(const std::string& value, const char* message) {
    if (value.empty()) {
        throw std::invalid_argument(message);
    }
}

std::string NormalizeTag(const std::string& raw_tag) {
    auto begin = raw_tag.begin();
    while (begin != raw_tag.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = raw_tag.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    std::string trimmed(begin, end);
    if (trimmed.empty()) {
        return {};
    }

    std::string result;
    result.reserve(trimmed.size());
    bool pending_space = false;
    for (unsigned char ch : trimmed) {
        if (std::isspace(ch)) {
            pending_space = true;
            continue;
        }
        if (pending_space && !result.empty()) {
            result.push_back(' ');
        }
        result.push_back(static_cast<char>(ch));
        pending_space = false;
    }
    return result;
}

std::vector<std::string> NormalizeTags(std::vector<std::string> tags) {
    std::vector<std::string> normalized;
    normalized.reserve(tags.size());
    for (auto& tag : tags) {
        auto cleaned = NormalizeTag(tag);
        if (!cleaned.empty()) {
            normalized.push_back(std::move(cleaned));
        }
    }

    std::set<std::string> unique(normalized.begin(), normalized.end());
    return std::vector<std::string>(unique.begin(), unique.end());
}

using AuthorNameMap =
    std::unordered_map<AuthorId, std::string, util::TaggedHasher<AuthorId>>;

AuthorNameMap BuildAuthorNameMap(const std::vector<Author>& authors) {
    AuthorNameMap names;
    names.reserve(authors.size());
    for (const auto& author : authors) {
        names.emplace(author.GetId(), author.GetName());
    }
    return names;
}

BookSummary ToSummary(const Book& book, const AuthorNameMap& names) {
    BookSummary summary;
    summary.id = book.GetId();
    summary.title = book.GetTitle();
    summary.publication_year = book.GetPublicationYear();
    if (auto it = names.find(book.GetAuthorId()); it != names.end()) {
        summary.author_name = it->second;
    }
    return summary;
}

void SortSummaries(std::vector<BookSummary>& books) {
    std::sort(books.begin(), books.end(), [](const BookSummary& lhs, const BookSummary& rhs) {
        if (lhs.title != rhs.title) {
            return lhs.title < rhs.title;
        }
        if (lhs.author_name != rhs.author_name) {
            return lhs.author_name < rhs.author_name;
        }
        return lhs.publication_year < rhs.publication_year;
    });
}

}  // namespace

Author UseCasesImpl::AddAuthor(const std::string& name) {
    EnsureNotEmpty(name, "Author name must not be empty");
    auto unit = unit_factory_.CreateUnitOfWork();
    Author author{AuthorId::New(), name};
    unit->Authors().Save(author);
    unit->Commit();
    return author;
}

std::vector<Author> UseCasesImpl::GetAuthors() const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto authors = unit->Authors().GetAll();
    unit->Commit();
    return authors;
}

std::optional<Author> UseCasesImpl::GetAuthorByName(const std::string& name) const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto author = unit->Authors().GetByName(name);
    unit->Commit();
    return author;
}

bool UseCasesImpl::RenameAuthor(const AuthorId& author_id, const std::string& new_name) {
    EnsureNotEmpty(new_name, "Author name must not be empty");
    auto unit = unit_factory_.CreateUnitOfWork();
    auto updated = unit->Authors().UpdateName(author_id, new_name);
    if (updated) {
        unit->Commit();
    }
    return updated;
}

bool UseCasesImpl::DeleteAuthor(const AuthorId& author_id) {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto books = unit->Books().GetByAuthor(author_id);
    for (const auto& book : books) {
        if (!unit->Books().Delete(book.GetId())) {
            return false;
        }
    }
    if (!unit->Authors().Delete(author_id)) {
        return false;
    }
    unit->Commit();
    return true;
}

Book UseCasesImpl::AddBook(AuthorId author_id, std::string title, int publication_year,
                           std::vector<std::string> tags) {
    EnsureNotEmpty(title, "Book title must not be empty");
    auto normalized_tags = NormalizeTags(std::move(tags));
    auto unit = unit_factory_.CreateUnitOfWork();
    Book book{BookId::New(), std::move(author_id), std::move(title), publication_year,
              std::move(normalized_tags)};
    unit->Books().Save(book);
    unit->Commit();
    return book;
}

std::vector<BookSummary> UseCasesImpl::GetBooks() const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto authors = unit->Authors().GetAll();
    auto books = unit->Books().GetAll();
    unit->Commit();

    auto names = BuildAuthorNameMap(authors);
    std::vector<BookSummary> summaries;
    summaries.reserve(books.size());
    for (const auto& book : books) {
        summaries.push_back(ToSummary(book, names));
    }
    SortSummaries(summaries);
    return summaries;
}

std::vector<BookSummary> UseCasesImpl::FindBooksByTitle(const std::string& title) const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto authors = unit->Authors().GetAll();
    auto books = unit->Books().GetByTitle(title);
    unit->Commit();

    auto names = BuildAuthorNameMap(authors);
    std::vector<BookSummary> summaries;
    summaries.reserve(books.size());
    for (const auto& book : books) {
        summaries.push_back(ToSummary(book, names));
    }
    SortSummaries(summaries);
    return summaries;
}

std::optional<BookDetails> UseCasesImpl::GetBookById(const BookId& id) const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto book = unit->Books().GetById(id);
    std::optional<Author> author;
    if (book) {
        author = unit->Authors().GetById(book->GetAuthorId());
    }
    unit->Commit();

    if (!book) {
        return std::nullopt;
    }
    BookDetails details;
    details.id = book->GetId();
    details.title = book->GetTitle();
    details.publication_year = book->GetPublicationYear();
    if (author) {
        details.author_name = author->GetName();
    }
    details.tags = book->GetTags();
    std::sort(details.tags.begin(), details.tags.end());
    return details;
}

bool UseCasesImpl::UpdateBook(const BookId& id, std::string title, int publication_year,
                              std::vector<std::string> tags) {
    EnsureNotEmpty(title, "Book title must not be empty");
    auto normalized_tags = NormalizeTags(std::move(tags));
    auto unit = unit_factory_.CreateUnitOfWork();
    auto current = unit->Books().GetById(id);
    if (!current) {
        return false;
    }
    Book updated{id, current->GetAuthorId(), std::move(title), publication_year,
                 std::move(normalized_tags)};
    auto result = unit->Books().Update(updated);
    if (result) {
        unit->Commit();
    }
    return result;
}

bool UseCasesImpl::DeleteBook(const BookId& id) {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto deleted = unit->Books().Delete(id);
    if (deleted) {
        unit->Commit();
    }
    return deleted;
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const AuthorId& author_id) const {
    auto unit = unit_factory_.CreateUnitOfWork();
    auto books = unit->Books().GetByAuthor(author_id);
    unit->Commit();
    std::sort(books.begin(), books.end(), [](const Book& lhs, const Book& rhs) {
        if (lhs.GetPublicationYear() != rhs.GetPublicationYear()) {
            return lhs.GetPublicationYear() < rhs.GetPublicationYear();
        }
        return lhs.GetTitle() < rhs.GetTitle();
    });
    return books;
}

}  // namespace app
