#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);
    // Строка без %-последовательностей
    BOOST_TEST(UrlDecode("Get /index.html HTTP/1.1"sv) == "Get /index.html HTTP/1.1"s);

    // Строка с валидными %-последовательностями, записанными в разном регистре
    BOOST_TEST(UrlDecode("Hello%20World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("%4a"sv) == "J"s);
    BOOST_TEST(UrlDecode("%4A"sv) == "J"s);
    BOOST_TEST(UrlDecode("%61%62%63%64%65%66"sv) == "abcdef"s);
    BOOST_TEST(UrlDecode("%6F%7A%7E"sv) == "oz~"s);

    // Строка с невалидными %-последовательностями (неверные символы в коде)
    BOOST_CHECK_THROW(UrlDecode("a%2g"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%ax"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%1g"sv), std::invalid_argument);

    // Строка с неполными %-последовательностями
    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("a%1"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("a%20b%1"sv), std::invalid_argument);

    // Строка с символом '+'
    BOOST_TEST(UrlDecode("a+b"sv) == "a b"s);
    BOOST_TEST(UrlDecode("a+b+c"sv) == "a b c"s);
    BOOST_TEST(UrlDecode("GET+/index.html"sv) == "GET /index.html"s);
}