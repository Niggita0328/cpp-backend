#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "../app/use_cases.h"
#include "../domain/author.h"
#include "../domain/book.h"

namespace menu {
class Menu;
}

namespace app {
class UseCases;
}

namespace ui {
namespace detail {

struct AuthorInfo {
    domain::AuthorId id;
    std::string name;
};

}  // namespace detail

class View {
public:
    View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output);

private:
    bool AddAuthor(std::istream& cmd_input) const;
    bool AddBook(std::istream& cmd_input) const;
    bool ShowAuthors() const;
    bool ShowBooks() const;
    bool ShowAuthorBooks() const;
    bool ShowBook(std::istream& cmd_input) const;
    bool DeleteAuthor(std::istream& cmd_input) const;
    bool EditAuthor(std::istream& cmd_input) const;
    bool DeleteBook(std::istream& cmd_input) const;
    bool EditBook(std::istream& cmd_input) const;

    std::optional<domain::AuthorId> PromptAuthorSelection() const;
    std::optional<domain::AuthorId> PromptAuthorSelection(const std::vector<detail::AuthorInfo>& authors) const;
    std::vector<detail::AuthorInfo> FetchAuthors() const;
    std::vector<app::BookSummary> FetchBooks() const;
    std::vector<domain::Book> FetchAuthorBooks(const domain::AuthorId& author_id) const;
    std::optional<domain::BookId> PromptBookSelection(const std::vector<app::BookSummary>& books) const;
    static std::vector<std::string> SplitTags(const std::string& line);
    static std::string JoinTags(const std::vector<std::string>& tags);

    menu::Menu& menu_;
    app::UseCases& use_cases_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace ui
