#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

}  // namespace detail

std::ostream& operator<<(std::ostream& out, const app::BookSummary& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

std::ostream& operator<<(std::ostream& out, const domain::Book& book) {
    out << book.GetTitle() << ", " << book.GetPublicationYear();
    return out;
}

namespace {

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int index = 1;
    for (const auto& value : vector) {
        out << index++ << " " << value << std::endl;
    }
}

}  // namespace

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s,
                    std::bind(&View::AddAuthor, this, std::placeholders::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, std::placeholders::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book details"s,
                    std::bind(&View::ShowBook, this, std::placeholders::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Delete author"s,
                    std::bind(&View::DeleteAuthor, this, std::placeholders::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edit author"s,
                    std::bind(&View::EditAuthor, this, std::placeholders::_1));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s,
                    std::bind(&View::DeleteBook, this, std::placeholders::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book"s,
                    std::bind(&View::EditBook, this, std::placeholders::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(name);
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int publication_year = 0;
        if (!(cmd_input >> publication_year)) {
            throw std::runtime_error("Invalid publication year");
        }
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        if (title.empty()) {
            throw std::runtime_error("Empty title");
        }

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        std::getline(input_, author_name);
        boost::algorithm::trim(author_name);

        std::optional<domain::AuthorId> author_id;
        if (!author_name.empty()) {
            auto author = use_cases_.GetAuthorByName(author_name);
            if (author) {
                author_id = author->GetId();
            } else {
                output_ << "No author found. Do you want to add " << author_name << " (y/n)?"
                        << std::endl;
                std::string answer;
                std::getline(input_, answer);
                boost::algorithm::trim(answer);
                if (answer == "y" || answer == "Y") {
                    author_id = use_cases_.AddAuthor(author_name).GetId();
                } else {
                    throw std::runtime_error("Cancelled");
                }
            }
        } else {
            author_id = PromptAuthorSelection();
            if (!author_id) {
                return true;
            }
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        auto tags = SplitTags(tags_line);
        use_cases_.AddBook(*author_id, std::move(title), publication_year, std::move(tags));
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, FetchAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, FetchBooks());
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        output_ << "Select author:" << std::endl;
        auto authors = FetchAuthors();
        PrintVector(output_, authors);
        output_ << "Enter author # or empty line to cancel" << std::endl;
        auto author_id = PromptAuthorSelection(authors);
        if (!author_id) {
            return true;
        }
        auto books = FetchAuthorBooks(*author_id);
        PrintVector(output_, books);
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<app::BookSummary> candidates =
            title.empty() ? FetchBooks() : use_cases_.FindBooksByTitle(title);
        if (candidates.empty()) {
            return true;
        }

        domain::BookId book_id;
        if (candidates.size() == 1 && !title.empty()) {
            book_id = candidates.front().id;
        } else {
            PrintVector(output_, candidates);
            auto selection = PromptBookSelection(candidates);
            if (!selection) {
                return true;
            }
            book_id = *selection;
        }

        auto details = use_cases_.GetBookById(book_id);
        if (!details) {
            return true;
        }

        output_ << "Title: " << details->title << std::endl;
        output_ << "Author: " << details->author_name << std::endl;
        output_ << "Publication year: " << details->publication_year << std::endl;
        if (!details->tags.empty()) {
            output_ << "Tags: " << JoinTags(details->tags) << std::endl;
        }
    } catch (const std::exception&) {
        // Do nothing, command should not output anything on failure
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        domain::AuthorId author_id;
        if (!name.empty()) {
            auto author = use_cases_.GetAuthorByName(name);
            if (!author) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
            author_id = author->GetId();
        } else {
            auto authors = FetchAuthors();
            output_ << "Select author:" << std::endl;
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;
            auto selection = PromptAuthorSelection(authors);
            if (!selection) {
                return true;
            }
            author_id = *selection;
        }

        if (!use_cases_.DeleteAuthor(author_id)) {
            output_ << "Failed to delete author" << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete author" << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        domain::AuthorId author_id;
        if (!name.empty()) {
            auto author = use_cases_.GetAuthorByName(name);
            if (!author) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id = author->GetId();
        } else {
            auto authors = FetchAuthors();
            output_ << "Select author:" << std::endl;
            PrintVector(output_, authors);
            output_ << "Enter author # or empty line to cancel" << std::endl;
            auto selection = PromptAuthorSelection(authors);
            if (!selection) {
                return true;
            }
            author_id = *selection;
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty() || !use_cases_.RenameAuthor(author_id, new_name)) {
            output_ << "Failed to edit author" << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<app::BookSummary> candidates =
            title.empty() ? FetchBooks() : use_cases_.FindBooksByTitle(title);
        if (candidates.empty()) {
            return true;
        }

        domain::BookId book_id;
        if (candidates.size() == 1 && !title.empty()) {
            book_id = candidates.front().id;
        } else {
            PrintVector(output_, candidates);
            auto selection = PromptBookSelection(candidates);
            if (!selection) {
                return true;
            }
            book_id = *selection;
        }

        if (!use_cases_.DeleteBook(book_id)) {
            output_ << "Failed to delete book" << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::vector<app::BookSummary> candidates =
            title.empty() ? FetchBooks() : use_cases_.FindBooksByTitle(title);
        if (candidates.empty()) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        domain::BookId book_id;
        if (candidates.size() == 1 && !title.empty()) {
            book_id = candidates.front().id;
        } else {
            PrintVector(output_, candidates);
            auto selection = PromptBookSelection(candidates);
            if (!selection) {
                output_ << "Book not found" << std::endl;
                return true;
            }
            book_id = *selection;
        }

        auto details = use_cases_.GetBookById(book_id);
        if (!details) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        output_ << "Enter new title or empty line to use the current one (" << details->title << "):"
                << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) {
            new_title = details->title;
        }

        output_ << "Enter publication year or empty line to use the current one ("
                << details->publication_year << "):" << std::endl;
        std::string year_str;
        std::getline(input_, year_str);
        boost::algorithm::trim(year_str);
        int new_year = details->publication_year;
        if (!year_str.empty()) {
            try {
                size_t processed = 0;
                new_year = std::stoi(year_str, &processed);
                if (processed != year_str.size()) {
                    throw std::runtime_error("Invalid year");
                }
            } catch (const std::exception&) {
                output_ << "Book not found" << std::endl;
                return true;
            }
        }

        output_ << "Enter tags (current tags: " << JoinTags(details->tags) << "):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        auto tags = SplitTags(tags_line);

        if (!use_cases_.UpdateBook(book_id, std::move(new_title), new_year, std::move(tags))) {
            output_ << "Book not found" << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Book not found" << std::endl;
    }
    return true;
}

std::optional<domain::AuthorId> View::PromptAuthorSelection() const {
    auto authors = FetchAuthors();
    output_ << "Select author:" << std::endl;
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;
    return PromptAuthorSelection(authors);
}

std::optional<domain::AuthorId> View::PromptAuthorSelection(
    const std::vector<detail::AuthorInfo>& authors) const {
    if (authors.empty()) {
        return std::nullopt;
    }

    std::string input;
    if (!std::getline(input_, input) || input.empty()) {
        return std::nullopt;
    }
    int author_index = 0;
    try {
        author_index = std::stoi(input);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid author index");
    }
    --author_index;
    if (author_index < 0 || static_cast<size_t>(author_index) >= authors.size()) {
        throw std::runtime_error("Invalid author index");
    }
    return authors[author_index].id;
}

std::vector<detail::AuthorInfo> View::FetchAuthors() const {
    auto authors = use_cases_.GetAuthors();
    std::vector<detail::AuthorInfo> result;
    result.reserve(authors.size());
    for (const auto& author : authors) {
        result.push_back(detail::AuthorInfo{author.GetId(), author.GetName()});
    }
    return result;
}

std::vector<app::BookSummary> View::FetchBooks() const {
    return use_cases_.GetBooks();
}

std::vector<domain::Book> View::FetchAuthorBooks(const domain::AuthorId& author_id) const {
    return use_cases_.GetAuthorBooks(author_id);
}

std::optional<domain::BookId> View::PromptBookSelection(
    const std::vector<app::BookSummary>& books) const {
    if (books.empty()) {
        return std::nullopt;
    }
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string input;
    if (!std::getline(input_, input) || input.empty()) {
        return std::nullopt;
    }
    int book_index = 0;
    try {
        book_index = std::stoi(input);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid book index");
    }
    --book_index;
    if (book_index < 0 || static_cast<size_t>(book_index) >= books.size()) {
        throw std::runtime_error("Invalid book index");
    }
    return books[book_index].id;
}

std::vector<std::string> View::SplitTags(const std::string& line) {
    std::vector<std::string> tags;
    std::string current;
    for (char ch : line) {
        if (ch == ',') {
            tags.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    tags.push_back(current);
    return tags;
}

std::string View::JoinTags(const std::vector<std::string>& tags) {
    if (tags.empty()) {
        return ""s;
    }
    std::ostringstream out;
    bool first = true;
    for (const auto& tag : tags) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << tag;
    }
    return out.str();
}

}  // namespace ui
