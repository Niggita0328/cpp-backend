#pragma once

#include "logger.h"
#include "model_serialization.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <exception>

namespace serialization {

class SerializingListener {
public:
    using Milliseconds = std::chrono::milliseconds;
    using ErrorHandler = std::function<void(const std::exception&)>;

    SerializingListener(model::Game& game,
                        app::Players& players,
                        std::filesystem::path state_file,
                        std::optional<Milliseconds> auto_save_period,
                        ErrorHandler error_handler = {});

    void OnTick(Milliseconds delta);
    void SaveState();

private:
    bool DoSave();
    void HandleError(const std::exception& ex);

    model::Game& game_;
    app::Players& players_;
    std::filesystem::path state_file_;
    std::optional<Milliseconds> auto_save_period_;
    Milliseconds time_since_last_save_{0};
    ErrorHandler error_handler_;
    bool error_reported_ = false;
};

}  // namespace serialization
