#include "players.h"

namespace model {

Token Players::GenerateToken() {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << generator1_() << std::setw(16) << generator2_();
    return Token{ss.str()};
}

std::pair<Token, model::Dog::Id> Players::Add(model::Dog& dog, GameSession& session) {
    dog.id = model::Dog::Id{dogs_.size()};
    dogs_.emplace_back(std::move(dog));
    
    sessions_.emplace_back(std::move(session));

    Token token = GenerateToken();
    players_.emplace_back(&sessions_.back(), token);

    token_to_player_[token] = &players_.back();
    
    return {token, dogs_.back().id};
}

Player* Players::FindByToken(const Token& token) {
    if (token_to_player_.contains(token)) {
        return token_to_player_.at(token);
    }
    return nullptr;
}

const std::vector<model::Dog>& Players::GetDogs() const {
    return dogs_;
}

const std::vector<GameSession>& Players::GetSessions() const {
    return sessions_;
}

} // namespace model