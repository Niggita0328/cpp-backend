#include "urldecode.h"

#include <stdexcept>
#include <string>
#include <cctype>

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 >= str.length()) {
                throw std::invalid_argument("Incomplete percent-encoding sequence");
            }

            char high = str[i + 1];
            char low = str[i + 2];

            if (!isxdigit(static_cast<unsigned char>(high)) || !isxdigit(static_cast<unsigned char>(low))) {
                throw std::invalid_argument("Invalid characters in percent-encoding sequence");
            }

            std::string hex_str;
            hex_str += high;
            hex_str += low;
            
            try {
                int value = std::stoi(hex_str, nullptr, 16);
                result += static_cast<char>(value);
            } catch (const std::exception& e) {
                 throw std::invalid_argument("Invalid percent-encoding sequence");
            }

            i += 2;
        } else {
            result += str[i];
        }
    }

    return result;
}