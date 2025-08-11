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
            map_id_str.remove_prefix(("/api/v1/maps/"sv).length()); // Используем remove_prefix для std::string_view
            
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

            json::object map_obj;
            map_obj["id"] = *map->GetId();
            map_obj["name"] = map->GetName();
            
            json::array roads_array;
            for (const auto& road : map->GetRoads()) {
                json::object road_obj;
                road_obj["x0"] = road.GetStart().x;
                road_obj["y0"] = road.GetStart().y;
                if (road.IsHorizontal()) {
                    road_obj["x1"] = road.GetEnd().x;
                } else {
                    road_obj["y1"] = road.GetEnd().y;
                }
                roads_array.emplace_back(std::move(road_obj));
            }
            map_obj["roads"] = std::move(roads_array);

            json::array buildings_array;
            for (const auto& building : map->GetBuildings()) {
                json::object building_obj;
                building_obj["x"] = building.GetBounds().position.x;
                building_obj["y"] = building.GetBounds().position.y;
                building_obj["w"] = building.GetBounds().size.width;
                building_obj["h"] = building.GetBounds().size.height;
                buildings_array.emplace_back(std::move(building_obj));
            }
            map_obj["buildings"] = std::move(buildings_array);

            json::array offices_array;
            for (const auto& office : map->GetOffices()) {
                json::object office_obj;
                office_obj["id"] = *office.GetId();
                office_obj["x"] = office.GetPosition().x;
                office_obj["y"] = office.GetPosition().y;
                office_obj["offsetX"] = office.GetOffset().dx;
                office_obj["offsetY"] = office.GetOffset().dy;
                offices_array.emplace_back(std::move(office_obj));
            }
            map_obj["offices"] = std::move(offices_array);

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