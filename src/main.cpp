#include "retro_chess/commands.hpp"
#include "retro_chess/gui.hpp"

#include <iostream>
#include <string>

namespace {

void print_prompt(const retro_chess::GameView& view) {
    std::cout << "zug " << view.fullmove_number << " | "
              << (view.white_to_move ? "weiß" : "schwarz") << " am zug > " << std::flush;
}

int run_terminal(retro_chess::GameSession& game) {
    retro_chess::CommandProcessor commands(game);
    std::cout << "terminal chess. gib züge oder help ein.\n";
    std::cout << retro_chess::help_text() << '\n';
    std::string line;
    print_prompt(game.view());
    while (std::getline(std::cin, line)) {
        const auto result = commands.execute(line);
        if (!result.output.empty()) std::cout << result.output << '\n';
        if (result.changed) std::cout << game.status_text() << '\n';
        if (result.action == retro_chess::CommandAction::quit) break;
        print_prompt(game.view());
    }
    return 0;
}

}

int main(int argc, char** argv) {
    bool terminal_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--terminal-only") {
            terminal_only = true;
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "nutzung: terminal-chess [--terminal-only]\n";
            return 0;
        } else {
            std::cerr << "unbekannte option: " << argument << '\n';
            return 2;
        }
    }

    try {
        retro_chess::GameSession game;
        return terminal_only ? run_terminal(game) : retro_chess::run_gui(game);
    } catch (const std::exception& error) {
        std::cerr << "spiel konnte nicht starten: " << error.what() << '\n';
        return 1;
    }
}
