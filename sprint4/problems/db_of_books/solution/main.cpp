#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <pqxx/pqxx>
#include <boost/json.hpp>

using namespace std::literals;
using pqxx::operator"" _zv;
namespace json = boost::json;

// Function to create the table if it doesn't exist
void CreateTable(pqxx::connection& conn) {
    pqxx::work w(conn);
    w.exec(
        "CREATE TABLE IF NOT EXISTS books ("
        "    id SERIAL PRIMARY KEY,"
        "    title VARCHAR(100) NOT NULL,"
        "    author VARCHAR(100) NOT NULL,"
        "    year INTEGER NOT NULL,"
        "    isbn CHAR(13) UNIQUE"
        ");"_zv
    );
    w.commit();
}

// Function to handle "add_book" action
void AddBook(pqxx::connection& conn, const json::object& payload) {
    json::object response;
    try {
        pqxx::work w(conn);
        
        const auto& title = payload.at("title").as_string();
        const auto& author = payload.at("author").as_string();
        const auto year = payload.at("year").as_int64();
        
        std::optional<std::string> isbn;
        if (payload.contains("ISBN") && !payload.at("ISBN").is_null()) {
            isbn = std::string(payload.at("ISBN").as_string());
        }
        
        w.exec_prepared("add_book_query", std::string(title), std::string(author), static_cast<int>(year), isbn);
        w.commit();
        
        response["result"] = true;
    } catch (const pqxx::sql_error&) {
        response["result"] = false;
    }
    std::cout << json::serialize(response) << std::endl;
}

// Function to handle "all_books" action
void GetAllBooks(pqxx::connection& conn) {
    json::array books_array;
    
    pqxx::read_transaction r(conn);
    
    const auto query_text = 
        "SELECT id, title, author, year, isbn FROM books "
        "ORDER BY year DESC, title ASC, author ASC, isbn ASC;"_zv;

    auto query_result = r.query<int, std::string, std::string, int, std::optional<std::string>>(query_text);
    
    for (auto const& [id, title, author, year, isbn] : query_result) {
        json::object book;
        book["id"] = id;
        book["title"] = title;
        book["author"] = author;
        book["year"] = year;
        if (isbn) {
            book["ISBN"] = *isbn;
        } else {
            book["ISBN"] = nullptr;
        }
        books_array.push_back(book);
    }
    
    std::cout << json::serialize(books_array) << std::endl;
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <conn-string>" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};

        CreateTable(conn);

        conn.prepare("add_book_query", "INSERT INTO books (title, author, year, isbn) VALUES ($1, $2, $3, $4)");

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            json::value request_val = json::parse(line);
            const json::object& request = request_val.as_object();
            const auto& action = request.at("action").as_string();

            if (action == "add_book") {
                AddBook(conn, request.at("payload").as_object());
            } else if (action == "all_books") {
                GetAllBooks(conn);
            } else if (action == "exit") {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
