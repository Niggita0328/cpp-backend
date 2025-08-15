#pragma once
#include "http_server.h"
#include "model.h"
#include "application.h"
#include "json_serializer.h"
#include "logger.h"
#include <boost/json.hpp>
#include <string>
#include <filesystem>
#include <optional>
#include <regex>
#include <boost/asio/dispatch.hpp>
#include <chrono>
#include <exception>

using namespace std::literals;

namespace endpoints {
    constexpr auto MAPS = "/api/v1/maps";
    constexpr auto MAP = "/api/v1/maps/"sv;
    constexpr auto JOIN = "/api/v1/game/join";
    constexpr auto PLAYERS = "/api/v1/game/players";
    constexpr auto STATE = "/api/v1/game/state";
    constexpr auto ACTION = "/api/v1/game/player/action";
    constexpr auto TICK = "/api/v1/game/tick";
}

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace net = boost::asio;

using StringResponse = http::response<http::string_body>;

class ApiHandler {
public:
    explicit ApiHandler(app::Application& app, bool manual_tick = false);

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        net::dispatch(app_.GetStrand(),
            [this, req = std::move(req), send = std::forward<Send>(send)]() mutable {
                HandleApiRequest(std::move(req), std::forward<Send>(send));
            }
        );
    }

private:
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive, http::verb method, std::string_view content_type = "application/json"sv, std::optional<std::pair<http::field, std::string_view>> extra_header = std::nullopt);
    
    template <typename Body, typename Allocator>
    std::optional<Token> TryExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req);
    
    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    app::Application& app_;
    bool manual_tick_;
};

template <typename Body, typename Allocator, typename Send>
void ApiHandler::HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    const auto version = req.version();
    const auto keep_alive = req.keep_alive();
    const std::string target(req.target());
    const auto method = req.method();

    auto bad_request = [&](std::string_view message, std::string_view code = "badRequest") {
        json::object obj;
        obj["code"] = std::string(code);
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::bad_request, json::serialize(obj), version, keep_alive, method));
    };
    auto not_found = [&](std::string_view message) {
        json::object obj;
        obj["code"] = "mapNotFound";
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::not_found, json::serialize(obj), version, keep_alive, method));
    };
    auto invalid_method = [&](std::string_view allow, std::string_view message = "Invalid method") {
        json::object obj;
        obj["code"] = "invalidMethod";
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::method_not_allowed, json::serialize(obj), version, keep_alive, method, "application/json", {{http::field::allow, allow}} ));
    };
    auto unauthorized = [&](std::string_view code, std::string_view message) {
        json::object obj;
        obj["code"] = std::string(code);
        obj["message"] = std::string(message);
        send(this->MakeStringResponse(http::status::unauthorized, json::serialize(obj), version, keep_alive, method));
    };

    auto handle_authorized = 
        [&](auto&& request, auto&& sender, auto&& action) {
        auto token_opt = TryExtractToken(request);
        if(!token_opt) {
            return unauthorized("invalidToken", "Authorization header is required");
        }
        
        app::Player* player = app_.FindByToken(*token_opt);
        if (!player) {
            return unauthorized("unknownToken", "Player token has not been found");
        }
        action(player, std::forward<decltype(request)>(request), std::forward<decltype(sender)>(sender));
    };

    if (target == endpoints::MAPS) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        json::array maps_array;
        for (const auto& map : app_.ListMaps()) {
            maps_array.push_back(json_serializer::MapToJson(map, true));
        }
        return send(this->MakeStringResponse(http::status::ok, json::serialize(maps_array), version, keep_alive, method));
    }

    if (target.starts_with(endpoints::MAP)) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        std::string_view map_id_str = target;
        map_id_str.remove_prefix((endpoints::MAP).length());
        
        const model::Map* map = app_.FindMap(model::Map::Id{std::string{map_id_str}});
        if (!map) {
            return not_found("Map not found");
        }

        return send(this->MakeStringResponse(http::status::ok, json::serialize(json_serializer::MapToJson(*map, false)), version, keep_alive, method));
    }

    if (target == endpoints::JOIN) {
        if (req.method() != http::verb::post) {
            return invalid_method("POST", "Only POST method is expected");
        }

        json::value jv;
        try {
            jv = json::parse(req.body());
        } catch (const std::exception& e) {
            json::value data{{"code", "invalidArgument"}, {"message", "Join game request parse error"}, {"exception", e.what()}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse join request body";
            return bad_request("Join game request parse error", "invalidArgument");
        }
        
        if(!jv.is_object() || !jv.as_object().contains("userName") || !jv.as_object().contains("mapId")) {
            return bad_request("Join game request parse error", "invalidArgument");
        }

        const auto& obj = jv.as_object();
        std::string user_name;
        std::string map_id_str;
        try {
            user_name = obj.at("userName").as_string().c_str();
            map_id_str = obj.at("mapId").as_string().c_str();
        } catch (const std::exception& e) {
            json::value data{{"code", "invalidArgument"}, {"message", "Join game request parse error"}, {"exception", e.what()}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse join request fields";
            return bad_request("Join game request parse error", "invalidArgument");
        }

        if (user_name.empty()) {
             return bad_request("Invalid name", "invalidArgument");
        }
        
        auto join_result = app_.JoinGame(model::Map::Id{map_id_str}, user_name);
        if(!join_result) {
            return not_found("Map not found");
        }

        json::object resp_obj;
        resp_obj["authToken"] = *join_result->token;
        resp_obj["playerId"] = *join_result->player_id;
        return send(this->MakeStringResponse(http::status::ok, json::serialize(resp_obj), version, keep_alive, method));
    }

    if (target == endpoints::PLAYERS) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        return handle_authorized(std::move(req), std::forward<Send>(send), 
            [&](app::Player* player, auto&&, auto&& sender){
                json::object players_obj;
                for (const auto& dog_ptr : player->GetSession()->GetDogs()) {
                    json::object player_info;
                    player_info["name"] = dog_ptr->GetName();
                    players_obj[std::to_string(*dog_ptr->GetId())] = player_info;
                }
                sender(this->MakeStringResponse(http::status::ok, json::serialize(players_obj), version, keep_alive, method));
            });
    }

    if (target == endpoints::STATE) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD", "Invalid method");
        }

        return handle_authorized(std::move(req), std::forward<Send>(send), 
            [&](app::Player* player, auto&&, auto&& sender){
                json::object players_obj;
                for (const auto& dog_ptr : player->GetSession()->GetDogs()) {
                    players_obj[std::to_string(*dog_ptr->GetId())] = json_serializer::DogToJson(*dog_ptr);
                }
                json::object root_obj;
                root_obj["players"] = players_obj;
                sender(this->MakeStringResponse(http::status::ok, json::serialize(root_obj), version, keep_alive, method));
            });
    }

    if (target == endpoints::ACTION) {
        if (req.method() != http::verb::post) {
            return invalid_method("POST", "Invalid method");
        }

        if (req.find(http::field::content_type) == req.end() || req.at(http::field::content_type) != "application/json") {
            return bad_request("Invalid content type", "invalidArgument");
        }

        return handle_authorized(std::move(req), std::forward<Send>(send), 
            [&](app::Player* player, auto&& request, auto&& sender){
                json::value jv;
                try {
                    jv = json::parse(request.body());
                } catch (const std::exception& e) {
                    json::value data{{"code", "invalidArgument"}, {"message", "Failed to parse action"}, {"exception", e.what()}};
                    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse action request body";
                    return bad_request("Failed to parse action", "invalidArgument");
                }
                if (!jv.is_object() || !jv.as_object().contains("move")) {
                    return bad_request("Failed to parse action", "invalidArgument");
                }
                std::string move_cmd;
                try {
                    move_cmd = jv.as_object().at("move").as_string().c_str();
                } catch(const std::exception& e) {
                    json::value data{{"code", "invalidArgument"}, {"message", "Failed to parse action"}, {"exception", e.what()}};
                    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse action request fields";
                    return bad_request("Failed to parse action", "invalidArgument");
                }

                if (move_cmd != "L" && move_cmd != "R" && move_cmd != "U" && move_cmd != "D" && move_cmd != "") {
                    return bad_request("Failed to parse action", "invalidArgument");
                }

                app_.MovePlayer(player, move_cmd);
                sender(this->MakeStringResponse(http::status::ok, "{}", version, keep_alive, method));
            });
    }

    if (target == endpoints::TICK) {
        if (!manual_tick_) {
            return bad_request("Invalid endpoint");
        }
        if (req.method() != http::verb::post) {
            return invalid_method("POST", "Invalid method");
        }
        if (req.find(http::field::content_type) == req.end() || req.at(http::field::content_type) != "application/json") {
            return bad_request("Invalid content type", "invalidArgument");
        }
        try {
            json::value jv = json::parse(req.body());
            auto delta_ms = jv.as_object().at("timeDelta").as_int64();
            app_.Tick(std::chrono::milliseconds(delta_ms));
        } catch (const std::exception& e) {
            json::value data{{"code", "invalidArgument"}, {"message", "Failed to parse tick request JSON"}, {"exception", e.what()}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse tick request";
            return bad_request("Failed to parse tick request JSON", "invalidArgument");
        }
        return send(this->MakeStringResponse(http::status::ok, "{}", version, keep_alive, method));
    }
    
    return bad_request("Bad request");
}

template <typename Body, typename Allocator>
std::optional<Token> ApiHandler::TryExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req) {
    if (req.count(http::field::authorization) == 0) {
        return std::nullopt;
    }
    
    std::string auth_header{req.at(http::field::authorization)};
    std::regex bearer_regex(R"(^Bearer\s+([0-9a-fA-F]{32})$)", std::regex::icase);
    std::smatch match;

    if (std::regex_match(auth_header, match, bearer_regex)) {
        return Token{match[1].str()};
    }

    return std::nullopt;
}

} // namespace http_handler