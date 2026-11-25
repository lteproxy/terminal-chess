#pragma once

#include <string>

#include "retro_chess/game.hpp"

namespace retro_chess {

enum class CommandAction {
    none,
    quit
};

struct CommandResult {
    CommandAction action = CommandAction::none;
    std::string output;
    bool changed = false;
};

class CommandProcessor {
public:
    explicit CommandProcessor(GameSession& game);
    CommandResult execute(const std::string& input);

private:
    GameSession& game_;
};

std::string help_text();

}
