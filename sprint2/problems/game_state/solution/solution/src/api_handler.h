#pragma once
#include "http_server.h"
#include "model.h"
#include "players.h"
#include "json_serializer.h"
#include <boost/json.hpp>
#include <string>
#include <filesystem>
#include <optional>
#include <regex>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
using namespace std::literals;

using StringResponse = http::response<http::string_body>;

class ApiHandler {
public:
    explicit ApiHandler(model::Game& game, model::Players& players);

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        this->HandleApiRequest(std::move(req), std::forward<Send>(send));
    }

private:
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive, std::string_view content_type = "application/json"sv, std::optional<std::pair<http::field, std::string_view>> extra_header = std::nullopt);
    
    template <typename Body, typename Allocator>
    std::optional<Token> TryExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req);
    
    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    model::Game& game_;
    model::Players& players_;
};

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    const auto version = req.version();
    const auto keep_alive = req.keep_alive();
    const std::string target(req.target());

    auto bad_request = [&](std::string_view message, std::string_view code = "badRequest") {
        json::object obj;
        obj["code"] = std::string(code);
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::bad_request, json::serialize(obj), version, keep_alive));
    };
    auto not_found = [&](std::string_view message) {
        json::object obj;
        obj["code"] = "mapNotFound";
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::not_found, json::serialize(obj), version, keep_alive));
    };
    auto invalid_method = [&](std::string_view allow, std::string_view message = "Invalid method") {
        json::object obj;
        obj["code"] = "invalidMethod";
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::method_not_allowed, json::serialize(obj), version, keep_alive, "application/json", {{http::field::allow, allow}} ));
    };
    auto unauthorized = [&](std::string_view code, std::string_view message) {
        json::object obj;
        obj["code"] = std::string(code);
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::unauthorized, json::serialize(obj), version, keep_alive));
    };

    if (target == "/api/v1/maps"sv) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) return invalid_method("GET, HEAD");
        
        json::array maps_array;
        for (const auto& map : game_.GetMaps()) {
            maps_array.push_back(json_serializer::ToJson(map, true));
        }
        return send(this->MakeStringResponse(http::status::ok, json::serialize(maps_array), version, keep_alive));
    }

    if (target.starts_with("/api/v1/maps/"sv)) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) return invalid_method("GET, HEAD");
        
        std::string_view map_id_str = target;
        map_id_str.remove_prefix(("/api/v1/maps/"sv).length());
        
        const model::Map* map = game_.FindMap(model::Map::Id{std::string{map_id_str}});
        if (!map) return not_found("Map not found");

        return send(this->MakeStringResponse(http::status::ok, json::serialize(json_serializer::ToJson(*map, false)), version, keep_alive));
    }

    if (target == "/api/v1/game/join") {
        if (req.method() != http::verb::post) {
            return invalid_method("POST", "Only POST method is expected");
        }
        json::value jv;
        try {
            jv = json::parse(req.body());
        } catch (...) {
            return bad_request("Join game request parse error", "invalidArgument");
        }
        
        if(!jv.is_object() || !jv.as_object().contains("userName") || !jv.as_object().contains("mapId")) {
            return bad_request("Join game request parse error", "invalidArgument");
        }

        const auto& obj = jv.as_object();
        std::string user_name;
        std::string map_id;
        try {
            user_name = obj.at("userName").as_string().c_str();
            map_id = obj.at("mapId").as_string().c_str();
        } catch (...) {
            return bad_request("Join game request parse error", "invalidArgument");
        }

        if (user_name.empty()) {
             return bad_request("Invalid name", "invalidArgument");
        }
        const model::Map* map = game_.FindMap(model::Map::Id{map_id});
        if (!map) return not_found("Map not found");
        
        model::Dog dog{ .name = user_name };
        model::GameSession session(map->GetId(), &dog);
        auto [token, player_id] = players_.Add(dog, session);

        json::object resp_obj;
        resp_obj["authToken"] = *token;
        resp_obj["playerId"] = *player_id;
        return send(this->MakeStringResponse(http::status::ok, json::serialize(resp_obj), version, keep_alive));
    }

    if (target == "/api/v1/game/players") {
        if (req.method() != http::verb::get && req.method() != http::verb::head) return invalid_method("GET, HEAD");
        
        auto token_opt = TryExtractToken(req);
        if(!token_opt) {
            return unauthorized("invalidToken", "Authorization header is missing");
        }
        
        model::Player* player = players_.FindByToken(*token_opt);
        if (!player) {
            return unauthorized("unknownToken", "Player token has not been found");
        }
        
        json::object players_obj;
        for (const auto& dog : players_.GetDogs()) {
            json::object player_info;
            player_info["name"] = dog.name;
            players_obj[std::to_string(*dog.id)] = player_info;
        }
        
        return send(this->MakeStringResponse(http::status::ok, json::serialize(players_obj), version, keep_alive));
    }
    
    return bad_request("Bad request");
}

template <typename Body, typename Allocator>
std::optional<Token> ApiHandler::TryExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req) {
    if (req.count(http::field::authorization) == 0) {
        return std::nullopt;
    }
    
    std::string auth_header{req.at(http::field::authorization)};
    std::regex bearer_regex(R"(Bearer\s+([0-9a-fA-F]{32}))", std::regex::icase);
    std::smatch match;

    if (std::regex_match(auth_header, match, bearer_regex)) {
        return Token{match[1].str()};
    }

    return std::nullopt;
}

} // namespace http_handler