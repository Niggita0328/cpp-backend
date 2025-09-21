#include "htmldecode.h"

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <cctype>

static const std::vector<std::pair<std::string_view, char>> HTML_ENTITIES = {
    {"quot", '"'},
    {"apos", "'"},
    {"amp", '&'},
    {"gt", '>'},
    {"lt", '<'}
};

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.length());
    size_t i = 0;
    while (i < str.length()) {
        if (str[i] != '&') {
            result += str[i++];
            continue;
        }

        bool decoded = false;
        for (const auto& [entity, value] : HTML_ENTITIES) {
            if (i + 1 + entity.length() > str.length()) {
                continue;
            }

            std::string_view potential_entity = str.substr(i + 1, entity.length());
            
            if (potential_entity == entity) {
                result += value;
                i += 1 + entity.length();
                if (i < str.length() && str[i] == ';') {
                    i++;
                }
                decoded = true;
                break;
            }

            std::string upper_entity;
            upper_entity.reserve(entity.length());
            for (char c : entity) {
                upper_entity += std::toupper(static_cast<unsigned char>(c));
            }
            if (potential_entity == upper_entity) {
                result += value;
                i += 1 + entity.length();
                if (i < str.length() && str[i] == ';') {
                    i++;
                }
                decoded = true;
                break;
            }
        }

        if (!decoded) {
            result += str[i++];
        }
    }
    return result;
}