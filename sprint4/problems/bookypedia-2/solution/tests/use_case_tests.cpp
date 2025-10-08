#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"
#include "../src/domain/unit_of_work.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        auto it = std::find_if(saved_authors.begin(), saved_authors.end(),
                               [&author](const domain::Author& stored) {
                                   return stored.GetId() == author.GetId();
                               });
        if (it != saved_authors.end()) {
            *it = author;
        } else {
            saved_authors.emplace_back(author);
        }
    }

    std::vector<domain::Author> GetAll() const override {
        return saved_authors;
    }

    std::optional<domain::Author> GetById(const domain::AuthorId& id) const override {
        auto it = std::find_if(saved_authors.begin(), saved_authors.end(),
                               [&id](const domain::Author& stored) {
                                   return stored.GetId() == id;
                               });
        if (it == saved_authors.end()) {
            return std::nullopt;
        }
        return *it;
    }

    std::optional<domain::Author> GetByName(const std::string& name) const override {
        auto it = std::find_if(saved_authors.begin(), saved_authors.end(),
                               [&name](const domain::Author& stored) {
                                   return stored.GetName() == name;
                               });
        if (it == saved_authors.end()) {
            return std::nullopt;
        }
        return *it;
    }

    bool UpdateName(const domain::AuthorId& id, const std::string& name) override {
        auto it = std::find_if(saved_authors.begin(), saved_authors.end(),
                               [&id](const domain::Author& stored) {
                                   return stored.GetId() == id;
                               });
        if (it == saved_authors.end()) {
            return false;
        }
        *it = domain::Author{id, name};
        return true;
    }

    bool Delete(const domain::AuthorId& id) override {
        auto it = std::find_if(saved_authors.begin(), saved_authors.end(),
                               [&id](const domain::Author& stored) {
                                   return stored.GetId() == id;
                               });
        if (it == saved_authors.end()) {
            return false;
        }
        saved_authors.erase(it);
        return true;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book) override {
        auto it = std::find_if(saved_books.begin(), saved_books.end(),
                               [&book](const domain::Book& stored) {
                                   return stored.GetId() == book.GetId();
                               });
        if (it != saved_books.end()) {
            *it = book;
        } else {
            saved_books.emplace_back(book);
        }
    }

    std::vector<domain::Book> GetAll() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }

    std::optional<domain::Book> GetById(const domain::BookId& book_id) const override {
        auto it = std::find_if(saved_books.begin(), saved_books.end(),
                               [&book_id](const domain::Book& stored) {
                                   return stored.GetId() == book_id;
                               });
        if (it == saved_books.end()) {
            return std::nullopt;
        }
        return *it;
    }

    std::vector<domain::Book> GetByTitle(const std::string& title) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetTitle() == title) {
                result.push_back(book);
            }
        }
        return result;
    }

    bool Update(const domain::Book& book) override {
        auto it = std::find_if(saved_books.begin(), saved_books.end(),
                               [&book](const domain::Book& stored) {
                                   return stored.GetId() == book.GetId();
                               });
        if (it == saved_books.end()) {
            return false;
        }
        *it = book;
        return true;
    }

    bool Delete(const domain::BookId& book_id) override {
        auto it = std::find_if(saved_books.begin(), saved_books.end(),
                               [&book_id](const domain::Book& stored) {
                                   return stored.GetId() == book_id;
                               });
        if (it == saved_books.end()) {
            return false;
        }
        saved_books.erase(it);
        return true;
    }
};

struct MockUnitOfWork : domain::UnitOfWork {
    MockAuthorRepository& authors;
    MockBookRepository& books;
    bool committed = false;

    MockUnitOfWork(MockAuthorRepository& authors_repo, MockBookRepository& books_repo)
        : authors(authors_repo)
        , books(books_repo) {
    }

    domain::AuthorRepository& Authors() override {
        return authors;
    }

    domain::BookRepository& Books() override {
        return books;
    }

    void Commit() override {
        committed = true;
    }
};

struct MockUnitOfWorkFactory : domain::UnitOfWorkFactory {
    MockAuthorRepository& authors;
    MockBookRepository& books;

    MockUnitOfWorkFactory(MockAuthorRepository& authors_repo, MockBookRepository& books_repo)
        : authors(authors_repo)
        , books(books_repo) {
    }

    std::unique_ptr<domain::UnitOfWork> CreateUnitOfWork() const override {
        return std::make_unique<MockUnitOfWork>(authors, books);
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
    MockUnitOfWorkFactory factory{authors, books};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{factory};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            auto author = use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() == author.GetId());
            }
        }

        WHEN("Adding a book") {
            const auto author_id = domain::AuthorId::New();
            use_cases.AddBook(author_id, "Harry Potter", 1998, {});

            THEN("book is stored with generated identifier") {
                REQUIRE(books.saved_books.size() == 1);
                const auto& book = books.saved_books.front();
                CHECK(book.GetTitle() == "Harry Potter");
                CHECK(book.GetPublicationYear() == 1998);
                CHECK(book.GetAuthorId() == author_id);
                CHECK(book.GetId() != domain::BookId{});
            }
        }

        WHEN("Adding a book with duplicate tags") {
            auto author = use_cases.AddAuthor("Jack London");
            use_cases.AddBook(author.GetId(), "White Fang", 1906,
                              {"adventure", "dog", "  gold   rush  ", "dog", "", "dogs"});

            THEN("tags are normalized") {
                REQUIRE(books.saved_books.size() == 1);
                const auto& tags = books.saved_books.front().GetTags();
                REQUIRE(tags == std::vector<std::string>{"adventure", "gold rush", "dog", "dogs"});
            }
        }
    }
}
