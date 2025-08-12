#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>
#include <string> // <-- Добавлено для std::string

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;


namespace json_serializer {
    // Только объявления!
    json::value ToJson(const model::Road& road);
    json::value ToJson(const model::Building& building);
    json::value ToJson(const model::Office& office);
    json::value ToJson(const model::Map& map);
}


class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto verb = req.method();
        const std::string target(req.target());
        const auto version = req.version();
        const auto keep_alive = req.keep_alive();

        if (verb == http::verb::get && target == "/api/v1/maps"sv) {
            json::array maps_array;
            for (const auto& map : game_.GetMaps()) {
                json::object map_obj;
                map_obj["id"] = *map.GetId();
                map_obj["name"] = map.GetName();
                maps_array.emplace_back(std::move(map_obj));
            }
            http::response<http::string_body> res{http::status::ok, version};
            res.set(http::field::content_type, "application/json");
            res.body() = json::serialize(maps_array);
            res.content_length(res.body().size());
            res.keep_alive(keep_alive);
            return send(std::move(res));
        }

        if (verb == http::verb::get && target.starts_with("/api/v1/maps/"sv)) {
            std::string_view map_id_str = target;
            map_id_str.remove_prefix(("/api/v1/maps/"sv).length());
            
            model::Map::Id map_id{std::string{map_id_str}};

            const model::Map* map = game_.FindMap(map_id);
            if (!map) {
                json::object obj;
                obj["code"] = "mapNotFound";
                obj["message"] = "Map not found";
                http::response<http::string_body> res{http::status::not_found, version};
                res.set(http::field::content_type, "application/json");
                res.body() = json::serialize(obj);
                res.content_length(res.body().size());
                res.keep_alive(keep_alive);
                return send(std::move(res));
            }

            // Сериализация теперь вынесена в отдельные функции
            json::value map_obj = json_serializer::ToJson(*map);

            http::response<http::string_body> res{http::status::ok, version};
            res.set(http::field::content_type, "application/json");
            res.body() = json::serialize(map_obj);
            res.content_length(res.body().size());
            res.keep_alive(keep_alive);
            return send(std::move(res));
        }
        
        json::object obj;
        obj["code"] = "badRequest";
        obj["message"] = "Bad request";
        http::response<http::string_body> res{http::status::bad_request, version};
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(obj);
        res.content_length(res.body().size());
        res.keep_alive(keep_alive);
        return send(std::move(res));
    }

private:
    model::Game& game_;
};

}  // namespace http_handler