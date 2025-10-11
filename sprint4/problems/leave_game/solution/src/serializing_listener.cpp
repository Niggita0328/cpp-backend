#include "serializing_listener.h"

#include <utility>

namespace serialization {

SerializingListener::SerializingListener(model::Game& game,
                                         app::Players& players,
                                         std::filesystem::path state_file,
                                         std::optional<Milliseconds> auto_save_period,
                                         ErrorHandler error_handler)
    : game_(game)
    , players_(players)
    , state_file_(std::move(state_file))
    , auto_save_period_(auto_save_period)
    , error_handler_(std::move(error_handler)) {
}

void SerializingListener::OnTick(Milliseconds delta) {
    if (!auto_save_period_) {
        return;
    }

    if (*auto_save_period_ <= Milliseconds::zero()) {
        if (DoSave()) {
            time_since_last_save_ = Milliseconds::zero();
        }
        return;
    }

    time_since_last_save_ += delta;
    if (time_since_last_save_ >= *auto_save_period_) {
        if (DoSave()) {
            time_since_last_save_ -= *auto_save_period_;
        }
    }
}

void SerializingListener::SaveState() {
    if (DoSave()) {
        time_since_last_save_ = Milliseconds::zero();
    }
}

bool SerializingListener::DoSave() {
    try {
        const auto state = CollectGameState(game_, players_);
        SaveGameState(state, state_file_);
        error_reported_ = false;
        return true;
    } catch (const std::exception& ex) {
        HandleError(ex);
    }
    return false;
}

void SerializingListener::HandleError(const std::exception& ex) {
    if (!error_reported_) {
        json::value data{{"state_file", state_file_.string()}, {"exception", ex.what()}};
        BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, data)
                                 << "state serialization failed";
        error_reported_ = true;
    }

    if (error_handler_) {
        error_handler_(ex);
    }
}

}  // namespace serialization
