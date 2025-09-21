#include "urlencode.h"
#include <iomanip>
#include <sstream>
#include <string_view>

std::string UrlEncode(std::string_view str) {
    std::ostringstream encoded_stream;
    encoded_stream << std::hex << std::uppercase;

    for (const char c : str) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (c == ' ') {
            encoded_stream << '+';
        } else if (uc < 32 || uc >= 128 || std::string_view("!#$&'()*+,/:;=?@[]").find(c) != std::string_view::npos) {
            encoded_stream << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(uc);
        } else {
            encoded_stream << c;
        }
    }
    return encoded_stream.str();
}