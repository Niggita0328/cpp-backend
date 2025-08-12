#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <string_view>
#include <string>
#include <filesystem>
#include <algorithm>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace fs = std::filesystem;
using namespace std::literals;

namespace json_serializer {
    json::value ToJson(const model::Road& road);
    json::value ToJson(const model::Building& building);
    json::value ToJson(const model::Office& office);
    json::value ToJson(const model::Map& map);
}

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_root);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto verb = req.method();
        const std::string target{req.target()};
        const auto version = req.version();
        const auto keep_alive = req.keep_alive();

        if (target.starts_with("/api/")) {
            return this->HandleApiRequest(std::move(req), std::forward<Send>(send));
        }

        if (verb == http::verb::get || verb == http::verb::head) {
            return this->HandleFileRequest(std::move(req), std::forward<Send>(send));
        }
        
        return send(this->MakeStringResponse(http::status::method_not_allowed, "Invalid method", version, keep_alive));
    }

private:
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned version, bool keep_alive, std::string_view content_type = "text/plain"sv) {
        StringResponse res{status, version};
        res.set(http::field::content_type, std::string(content_type));
        res.set(http::field::cache_control, "no-cache");
        res.body() = std::string(body);
        res.content_length(res.body().size());
        res.keep_alive(keep_alive);
        return res;
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);
    
    template <typename Body, typename Allocator, typename Send>
    void HandleFileRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    model::Game& game_;
    fs::path static_root_;
};

template <typename Body, typename Allocator, typename Send>
void RequestHandler::HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    const auto version = req.version();
    const auto keep_alive = req.keep_alive();
    const std::string target(req.target());

    if (req.method() == http::verb::get && target == "/api/v1/maps"sv) {
        json::array maps_array;
        for (const auto& map : game_.GetMaps()) {
            json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();
            maps_array.emplace_back(std::move(map_obj));
        }
        return send(this->MakeStringResponse(http::status::ok, json::serialize(maps_array), version, keep_alive, "application/json"));
    }

    if (req.method() == http::verb::get && target.starts_with("/api/v1/maps/"sv)) {
        std::string_view map_id_str = target;
        map_id_str.remove_prefix(("/api/v1/maps/"sv).length());
        
        model::Map::Id map_id{std::string{map_id_str}};

        const model::Map* map = game_.FindMap(map_id);
        if (!map) {
            json::object obj;
            obj["code"] = "mapNotFound";
            obj["message"] = "Map not found";
            return send(this->MakeStringResponse(http::status::not_found, json::serialize(obj), version, keep_alive, "application/json"));
        }

        json::value map_obj = json_serializer::ToJson(*map);
        return send(this->MakeStringResponse(http::status::ok, json::serialize(map_obj), version, keep_alive, "application/json"));
    }
    
    json::object obj;
    obj["code"] = "badRequest";
    obj["message"] = "Bad request";
    return send(this->MakeStringResponse(http::status::bad_request, json::serialize(obj), version, keep_alive, "application/json"));
}

template <typename Body, typename Allocator, typename Send>
void RequestHandler::HandleFileRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    const auto version = req.version();
    const auto keep_alive = req.keep_alive();

    auto url_decode = [](const std::string_view& encoded) -> std::string {
        std::string decoded;
        decoded.reserve(encoded.length());
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                try {
                    std::string hex = std::string(encoded.substr(i + 1, 2));
                    char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                    decoded += c;
                    i += 2;
                } catch (...) {
                    decoded += encoded[i];
                }
            } else if (encoded[i] == '+') {
                decoded += ' ';
            } else {
                decoded += encoded[i];
            }
        }
        return decoded;
    };

    const std::string target_str{req.target()};
    std::string decoded_path = url_decode(target_str);
    
    if (decoded_path.find("..") != std::string::npos) {
        return send(this->MakeStringResponse(http::status::bad_request, "Bad Request", version, keep_alive));
    }

    if (decoded_path.ends_with('/')) {
        decoded_path += "index.html";
    }
    
    fs::path file_path = static_root_ / decoded_path.substr(1);

    auto is_subpath = [](fs::path path, fs::path base) {
        path = fs::weakly_canonical(path);
        base = fs::weakly_canonical(base);
        for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
            if (p == path.end() || *p != *b) {
                return false;
            }
        }
        return true;
    };

    if (!is_subpath(file_path, static_root_)) {
        return send(this->MakeStringResponse(http::status::bad_request, "Bad Request", version, keep_alive));
    }

    if (fs::is_directory(file_path)) {
        file_path /= "index.html";
    }

    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        return send(this->MakeStringResponse(http::status::not_found, "File not found", version, keep_alive, "text/plain"));
    }
    
    auto get_mime_type = [](const fs::path& path) -> std::string_view {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        
        if (ext == ".htm" || ext == ".html") return "text/html";
        if (ext == ".css") return "text/css";
        if (ext == ".txt") return "text/plain";
        if (ext == ".js") return "text/javascript";
        if (ext == ".json") return "application/json";
        if (ext == ".xml") return "application/xml";
        if (ext == ".png") return "image/png";
        if (ext == ".jpg" || ext == ".jpe" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif") return "image/gif";
        if (ext == ".bmp") return "image/bmp";
        if (ext == ".ico") return "image/vnd.microsoft.icon";
        if (ext == ".tiff" || ext == ".tif") return "image/tiff";
        if (ext == ".svg" || ext == ".svgz") return "image/svg+xml";
        if (ext == ".mp3") return "audio/mpeg";

        return "application/octet-stream";
    };

    beast::error_code ec;
    FileResponse res{http::status::ok, version};
    res.set(http::field::content_type, std::string(get_mime_type(file_path)));
    res.set(http::field::cache_control, "no-cache");
    res.keep_alive(keep_alive);
    
    if (req.method() == http::verb::head) {
        res.content_length(fs::file_size(file_path, ec));
        if (ec) {
            return send(this->MakeStringResponse(http::status::internal_server_error, "Failed to get file size", version, keep_alive));
        }
    } else { // GET
        http::file_body::value_type file;
        file.open(file_path.c_str(), beast::file_mode::read, ec);
        if (ec) {
            return send(this->MakeStringResponse(http::status::internal_server_error, "Failed to open file", version, keep_alive));
        }
        res.body() = std::move(file);
        res.prepare_payload();
    }
    return send(std::move(res));
}

}  // namespace http_handler