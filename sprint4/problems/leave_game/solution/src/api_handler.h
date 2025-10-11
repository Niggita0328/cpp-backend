#pragma once
#include "http_server.h"
#include "model.h"
#include "application.h"
#include "json_serializer.h"
#include "logger.h"
#include "postgres.h"
#include <boost/json.hpp>
#include <string>
#include <filesystem>
#include <optional>
#include <regex>
#include <boost/asio/dispatch.hpp>
#include <chrono>
#include <exception>
#include <charconv>
#include <limits>
#include <vector>
#include <cstdint>
#include <stdexcept>

using namespace std::literals;

namespace endpoints {
    constexpr auto MAPS = "/api/v1/maps";
    constexpr auto MAP = "/api/v1/maps/"sv;
    constexpr auto JOIN = "/api/v1/game/join";
    constexpr auto PLAYERS = "/api/v1/game/players";
    constexpr auto STATE = "/api/v1/game/state";
    constexpr auto ACTION = "/api/v1/game/player/action";
    constexpr auto TICK = "/api/v1/game/tick";
    constexpr auto RECORDS = "/api/v1/game/records";
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
    StringResponse MakeStringResponse(
        http::status status,
        std::string_view body,
        unsigned version,
        bool keep_alive,
        http::verb method,
        std::string_view content_type = "application/json"sv,
        std::optional<std::pair<http::field, std::string_view>> extra_header = std::nullopt);
    
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
    std::string_view target_view{target};
    std::string_view path = target_view;
    std::string_view query;
    if (auto pos = target_view.find('?'); pos != std::string_view::npos) {
        path = target_view.substr(0, pos);
        query = target_view.substr(pos + 1);
    }
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

    if (path == endpoints::MAPS) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        json::array maps_array;
        for (const auto& map : app_.ListMaps()) {
            maps_array.push_back(json_serializer::MapToJson(map, true));
        }
        return send(this->MakeStringResponse(http::status::ok, json::serialize(maps_array), version, keep_alive, method));
    }

    if (path.starts_with(endpoints::MAP)) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        std::string_view map_id_str = path;
        map_id_str.remove_prefix((endpoints::MAP).length());
        
        const model::Map* map = app_.FindMap(model::Map::Id{std::string{map_id_str}});
        if (!map) {
            return not_found("Map not found");
        }

        json::value map_json = json_serializer::MapToJson(*map, false);
        if (const auto* extra = app_.GetMapExtra(map->GetId())) {
            map_json.as_object()["lootTypes"] = extra->loot_types;
        }

        return send(this->MakeStringResponse(http::status::ok, json::serialize(map_json), version, keep_alive, method));
    }

    if (path == endpoints::JOIN) {
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

    if (path == endpoints::PLAYERS) {
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

    if (path == endpoints::STATE) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD", "Invalid method");
        }

        return handle_authorized(std::move(req), std::forward<Send>(send), 
            [&](app::Player* player, auto&&, auto&& sender){
                json::object players_obj;
                for (const auto& dog_ptr : player->GetSession()->GetDogs()) {
                    players_obj[std::to_string(*dog_ptr->GetId())] = json_serializer::DogToJson(*dog_ptr);
                }

                json::object lost_objects_obj;
                for (const auto& lost_object : player->GetSession()->GetLostObjects()) {
                    lost_objects_obj[std::to_string(lost_object.id)] = json_serializer::LostObjectToJson(lost_object);
                }

                json::object root_obj;
                root_obj["players"] = players_obj;
                root_obj["lostObjects"] = std::move(lost_objects_obj);
                sender(this->MakeStringResponse(http::status::ok, json::serialize(root_obj), version, keep_alive, method));
            });
    }

    if (path == endpoints::ACTION) {
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

    if (path == endpoints::TICK) {
        if (req.method() != http::verb::post) {
            return invalid_method("POST", "Invalid method");
        }
        if (req.find(http::field::content_type) == req.end() || req.at(http::field::content_type) != "application/json") {
            return bad_request("Invalid content type", "invalidArgument");
        }
        std::chrono::milliseconds delta{0};
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object()) {
                throw std::invalid_argument("tick payload must be an object");
            }
            const auto& obj = jv.as_object();
            const auto it = obj.find("timeDelta");
            if (it == obj.end()) {
                throw std::invalid_argument("timeDelta field is missing");
            }
            auto delta_ms = it->value().as_int64();
            if (delta_ms < 0) {
                return bad_request("timeDelta must be non-negative", "invalidArgument");
            }
            delta = std::chrono::milliseconds(delta_ms);
        } catch (const std::exception& e) {
            json::value data{{"code", "invalidArgument"}, {"message", "Failed to parse tick request JSON"}, {"exception", e.what()}};
            BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "Failed to parse tick request";
            return bad_request("Failed to parse tick request JSON", "invalidArgument");
        }
        app_.Tick(delta);
        if (!manual_tick_) {
            json::value data{
                {"deltaMs", delta.count()},
                {"manualTickerEnabled", false}
            };
            BOOST_LOG_TRIVIAL(debug) << logging::add_value(additional_data, data)
                                     << "Manual tick accepted while auto ticker is running";
        }
        return send(this->MakeStringResponse(http::status::ok, "{}", version, keep_alive, method));
    }
    
    if (path == endpoints::RECORDS) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return invalid_method("GET, HEAD");
        }

        auto parse_unsigned = [](std::string_view value, std::size_t& out) -> bool {
            if (value.empty()) {
                return false;
            }
            std::uint64_t parsed = 0;
            const char* begin = value.data();
            const char* end = begin + value.size();
            auto [ptr, ec] = std::from_chars(begin, end, parsed);
            if (ec != std::errc{} || ptr != end) {
                return false;
            }
            if (parsed > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                return false;
            }
            out = static_cast<std::size_t>(parsed);
            return true;
        };

        std::size_t start = 0;
        std::size_t max_items = 100;

        if (!query.empty()) {
            std::size_t pos = 0;
            while (pos < query.size()) {
                const auto amp = query.find('&', pos);
                const std::size_t token_len = (amp == std::string_view::npos) ? query.size() - pos : amp - pos;
                std::string_view token = query.substr(pos, token_len);
                pos = (amp == std::string_view::npos) ? query.size() : amp + 1;
                if (token.empty()) {
                    continue;
                }
                const auto eq_pos = token.find('=');
                if (eq_pos == std::string_view::npos) {
                    return bad_request("Invalid query parameter", "invalidArgument");
                }
                const std::string_view key = token.substr(0, eq_pos);
                const std::string_view value = token.substr(eq_pos + 1);
                if (key == "start") {
                    if (!parse_unsigned(value, start)) {
                        return bad_request("Invalid start value", "invalidArgument");
                    }
                } else if (key == "maxItems") {
                    if (!parse_unsigned(value, max_items)) {
                        return bad_request("Invalid maxItems value", "invalidArgument");
                    }
                }
            }
        }

        if (max_items > 100) {
            return bad_request("maxItems must be less than or equal to 100", "invalidArgument");
        }

        auto* repository = app_.GetStatisticsRepository();
        if (!repository) {
            json::object obj;
            obj["code"] = "databaseUnavailable";
            obj["message"] = "Statistics repository is not initialized";
            return send(this->MakeStringResponse(http::status::internal_server_error, json::serialize(obj), version, keep_alive, method));
        }

        std::vector<db::PlayerRecord> records;
        try {
            records = repository->FetchRecords(start, max_items);
        } catch (const std::exception& e) {
            json::object obj;
            obj["code"] = "databaseError";
            obj["message"] = e.what();
            return send(this->MakeStringResponse(http::status::internal_server_error, json::serialize(obj), version, keep_alive, method));
        }

        json::array records_array;
        records_array.reserve(records.size());
        for (const auto& record : records) {
            json::object entry;
            entry["name"] = record.name;
            entry["score"] = record.score;
            entry["playTime"] = static_cast<double>(record.play_time_ms) / 1000.0;
            records_array.push_back(std::move(entry));
        }

        return send(this->MakeStringResponse(http::status::ok, json::serialize(records_array), version, keep_alive, method));
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
