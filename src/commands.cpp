#include "retro_chess/commands.hpp"

#include <algorithm>
#include <cctype>

namespace retro_chess {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

}

CommandProcessor::CommandProcessor(GameSession& game) : game_(game) {}

CommandResult CommandProcessor::execute(const std::string& input) {
    const std::string line = trim(input);
    if (line.empty()) return {};

    const auto split = line.find_first_of(" \t");
    const std::string command = lower(line.substr(0, split));
    const std::string argument = split == std::string::npos ? "" : trim(line.substr(split + 1));
    std::string message;

    if (command == "help") return {CommandAction::none, help_text(), false};
    if (command == "board") return {CommandAction::none, game_.board_text(), false};
    if (command == "moves" || command == "history") {
        return {CommandAction::none, game_.moves_text(), false};
    }
    if (command == "fen") return {CommandAction::none, game_.view().fen, false};
    if (command == "status") return {CommandAction::none, game_.status_text(), false};
    if (command == "undo") {
        const bool changed = game_.undo(message);
        return {CommandAction::none, message, changed};
    }
    if (command == "restart") {
        game_.restart();
        return {CommandAction::none, "neue partie gestartet.", true};
    }
    if (command == "quit" || command == "exit") {
        return {CommandAction::quit, "spiel wird beendet.", false};
    }
    if (command == "claim") {
        const bool changed = game_.claim_draw(argument, message);
        return {CommandAction::none, message, changed};
    }
    const bool changed = game_.play(line, message);
    return {CommandAction::none, message, changed};
}

std::string help_text() {
    return "züge: SAN wie e4 oder Nf3. UCI wie e2e4.\n"
           "befehle: help. board. moves. history. undo. restart. fen. status. claim. quit.\n"
           "claim kann ein remis beanspruchen. claim <zug> prüft einen angekündigten zug.";
}

}
