#include "request_handler.h"

namespace http_handler {

RequestHandler::RequestHandler(model::Game& game, model::Players& players, fs::path static_root)
    : api_handler_{game, players}
    , static_root_{std::move(static_root)} {
}

StringResponse RequestHandler::MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive, http::verb method, std::string_view content_type) {
    StringResponse res{status, version};
    res.set(http::field::content_type, std::string(content_type));
    res.set(http::field::cache_control, "no-cache");
    res.keep_alive(keep_alive);
    
    // Всегда устанавливаем Content-Length, даже для HEAD
    res.content_length(body.size());

    // Устанавливаем тело ответа только для запросов, отличных от HEAD
    if (method != http::verb::head) {
        res.body() = std::string(body);
    }
    
    return res;
}

}  // namespace http_handler