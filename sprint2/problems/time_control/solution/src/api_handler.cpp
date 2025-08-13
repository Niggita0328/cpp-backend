#include "api_handler.h"

namespace http_handler {

ApiHandler::ApiHandler(app::Application& app)
    : app_{app} {
}

StringResponse ApiHandler::MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive, http::verb method, std::string_view content_type, std::optional<std::pair<http::field, std::string_view>> extra_header) {
    StringResponse res{status, version};
    res.set(http::field::content_type, std::string(content_type));
    res.set(http::field::cache_control, "no-cache");
    if(extra_header) {
        res.set(extra_header->first, std::string(extra_header->second));
    }
    res.content_length(body.size());
    res.keep_alive(keep_alive);

    if (method != http::verb::head) {
        res.body() = std::string(body);
    }
    
    return res;
}

}  // namespace http_handler