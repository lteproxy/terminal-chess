#include "retro_chess/gui.hpp"
#include "retro_chess/commands.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <unistd.h>

namespace retro_chess {
namespace {

struct Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a = 255;
};

constexpr Color background{10, 17, 19};
constexpr Color panel{15, 27, 29};
constexpr Color line{43, 74, 70};
constexpr Color mint{133, 229, 183};
constexpr Color amber{245, 177, 83};
constexpr Color pale{221, 232, 211};
constexpr Color muted{111, 146, 133};
constexpr Color light_square{125, 159, 129};
constexpr Color dark_square{37, 76, 68};

void set_color(SDL_Renderer* renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill(SDL_Renderer* renderer, int x, int y, int width, int height, Color color) {
    set_color(renderer, color);
    SDL_Rect rect{x, y, width, height};
    SDL_RenderFillRect(renderer, &rect);
}

void outline(SDL_Renderer* renderer, int x, int y, int width, int height, Color color) {
    set_color(renderer, color);
    SDL_Rect rect{x, y, width, height};
    SDL_RenderDrawRect(renderer, &rect);
}

std::string ascii_text(std::string text) {
    const std::array<std::pair<std::string, std::string>, 4> replacements{{
        {"ä", "ae"}, {"ö", "oe"}, {"ü", "ue"}, {"ß", "ss"}
    }};
    for (const auto& [from, to] : replacements) {
        std::size_t position = 0;
        while ((position = text.find(from, position)) != std::string::npos) {
            text.replace(position, from.size(), to);
            position += to.size();
        }
    }
    return text;
}

std::array<std::uint8_t, 7> glyph(char input) {
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(input)));
    static const std::unordered_map<char, std::array<std::uint8_t, 7>> map{
        {'a', {14,17,17,31,17,17,17}}, {'b', {30,17,17,30,17,17,30}},
        {'c', {14,17,16,16,16,17,14}}, {'d', {30,17,17,17,17,17,30}},
        {'e', {31,16,16,30,16,16,31}}, {'f', {31,16,16,30,16,16,16}},
        {'g', {14,17,16,23,17,17,15}}, {'h', {17,17,17,31,17,17,17}},
        {'i', {14,4,4,4,4,4,14}}, {'j', {7,2,2,2,18,18,12}},
        {'k', {17,18,20,24,20,18,17}}, {'l', {16,16,16,16,16,16,31}},
        {'m', {17,27,21,21,17,17,17}}, {'n', {17,25,21,19,17,17,17}},
        {'o', {14,17,17,17,17,17,14}}, {'p', {30,17,17,30,16,16,16}},
        {'q', {14,17,17,17,21,18,13}}, {'r', {30,17,17,30,20,18,17}},
        {'s', {15,16,16,14,1,1,30}}, {'t', {31,4,4,4,4,4,4}},
        {'u', {17,17,17,17,17,17,14}}, {'v', {17,17,17,17,17,10,4}},
        {'w', {17,17,17,21,21,21,10}}, {'x', {17,17,10,4,10,17,17}},
        {'y', {17,17,10,4,4,4,4}}, {'z', {31,1,2,4,8,16,31}},
        {'0', {14,17,19,21,25,17,14}}, {'1', {4,12,4,4,4,4,14}},
        {'2', {14,17,1,2,4,8,31}}, {'3', {30,1,1,14,1,1,30}},
        {'4', {2,6,10,18,31,2,2}}, {'5', {31,16,16,30,1,1,30}},
        {'6', {14,16,16,30,17,17,14}}, {'7', {31,1,2,4,8,8,8}},
        {'8', {14,17,17,14,17,17,14}}, {'9', {14,17,17,15,1,1,14}},
        {'.', {0,0,0,0,0,12,12}}, {',', {0,0,0,0,4,4,8}},
        {':', {0,12,12,0,12,12,0}}, {';', {0,12,12,0,4,4,8}},
        {'-', {0,0,0,31,0,0,0}}, {'+', {0,4,4,31,4,4,0}},
        {'/', {1,2,2,4,8,8,16}}, {'?', {14,17,1,2,4,0,4}},
        {'!', {4,4,4,4,4,0,4}}, {'#', {10,31,10,10,31,10,0}},
        {'=', {0,31,0,31,0,0,0}}, {'_', {0,0,0,0,0,0,31}},
        {' ', {0,0,0,0,0,0,0}}, {'<', {2,4,8,16,8,4,2}},
        {'>', {8,4,2,1,2,4,8}}, {'(', {2,4,8,8,8,4,2}},
        {')', {8,4,2,2,2,4,8}}, {'|', {4,4,4,4,4,4,4}},
        {'\'', {4,4,8,0,0,0,0}}
    };
    const auto found = map.find(c);
    return found == map.end() ? map.at('?') : found->second;
}

int draw_text(SDL_Renderer* renderer, std::string text, int x, int y, int scale, Color color, int max_width = 0) {
    text = ascii_text(std::move(text));
    const int start_x = x;
    const int advance = 6 * scale;
    set_color(renderer, color);
    for (const char c : text) {
        if (c == '\n' || (max_width > 0 && x + 5 * scale > start_x + max_width)) {
            x = start_x;
            y += 9 * scale;
            if (c == '\n') continue;
        }
        const auto rows = glyph(c);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (rows[static_cast<std::size_t>(row)] & (1U << (4 - column))) {
                    SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        x += advance;
    }
    return y + 7 * scale;
}

void piece_shape(SDL_Renderer* renderer, char piece, int x, int y, int size) {
    if (piece == '.') return;
    const bool white = std::isupper(static_cast<unsigned char>(piece));
    const Color face = white ? pale : Color{30, 43, 39};
    const Color shade = white ? Color{63, 95, 79} : Color{8, 15, 14};
    const Color edge = white ? Color{242, 209, 137} : mint;
    const int u = std::max(2, size / 18);
    const int cx = x + size / 2;
    const int base_y = y + size - 3 * u;
    auto block = [&](int px, int py, int w, int h, Color color) {
        fill(renderer, px, py, w, h, color);
    };
    block(x + size / 4, base_y, size / 2, 2 * u, shade);
    block(x + size / 5, base_y + 2 * u, size * 3 / 5, u, edge);

    const char type = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
    if (type == 'p') {
        block(cx - 2 * u, y + size / 4, 4 * u, 4 * u, shade);
        block(cx - u, y + size / 4 + u, 2 * u, 2 * u, face);
        block(cx - 3 * u, y + size / 2, 6 * u, size / 4, edge);
        block(cx - 2 * u, y + size / 2 + u, 4 * u, size / 5, face);
    } else if (type == 'r') {
        block(cx - 4 * u, y + size / 4, 8 * u, 3 * u, edge);
        block(cx - 3 * u, y + size / 4 + 3 * u, 6 * u, size / 3, face);
        block(cx - 4 * u, y + size / 2 + 2 * u, 8 * u, size / 5, shade);
    } else if (type == 'n') {
        block(cx - 3 * u, y + size / 4, 5 * u, 4 * u, edge);
        block(cx, y + size / 4 - u, 3 * u, 3 * u, face);
        block(cx + u, y + size / 2, u, u, shade);
        block(cx - 2 * u, y + size / 2 + 2 * u, 5 * u, 2 * u, face);
    } else if (type == 'b') {
        block(cx - u, y + size / 5, 2 * u, 2 * u, edge);
        block(cx - 2 * u, y + size / 5 + 2 * u, 4 * u, 5 * u, face);
        block(cx - u, y + size / 3, 2 * u, 3 * u, shade);
        block(cx - 3 * u, y + size / 2 + u, 6 * u, u, edge);
    } else if (type == 'q') {
        block(cx - 4 * u, y + size / 4 + 2 * u, 8 * u, 2 * u, edge);
        block(cx - 3 * u, y + size / 3 + 4 * u, 6 * u, 4 * u, face);
        block(cx - 4 * u, y + size / 5, 2 * u, 2 * u, edge);
        block(cx - u, y + size / 6, 2 * u, 2 * u, edge);
        block(cx + 2 * u, y + size / 5, 2 * u, 2 * u, edge);
    } else {
        block(cx - 3 * u, y + size / 4 + 2 * u, 6 * u, 3 * u, edge);
        block(cx - 2 * u, y + size / 3 + 4 * u, 4 * u, 3 * u, face);
        block(cx - u, y + size / 6, 2 * u, 4 * u, edge);
        block(cx - 3 * u, y + size / 6 + u, 6 * u, 2 * u, edge);
    }
    outline(renderer, cx - size / 3, y + size / 5, 2 * size / 3, size * 3 / 5, shade);
}

void draw_piece_list(SDL_Renderer* renderer, const std::vector<char>& pieces, int x, int y, int cell) {
    int shown = 0;
    for (const char piece : pieces) {
        if (shown >= 12) break;
        piece_shape(renderer, piece, x + shown * cell, y, cell);
        ++shown;
    }
    if (pieces.size() > 12) draw_text(renderer, "+", x + 12 * cell, y + cell / 3, 2, amber);
}

void draw_board(SDL_Renderer* renderer, const GameView& view, int x, int y, int size, float animation) {
    const int cell = size / 8;
    const std::string& uci = view.history.empty() ? std::string{} : view.history.back().uci;
    int last_from = -1;
    int last_to = -1;
    if (uci.size() >= 4 && chess::Square::is_valid_string_sq(std::string_view(uci).substr(0, 2)) &&
        chess::Square::is_valid_string_sq(std::string_view(uci).substr(2, 2))) {
        last_from = chess::Square(std::string_view(uci).substr(0, 2)).index();
        last_to = chess::Square(std::string_view(uci).substr(2, 2)).index();
    }

    for (int rank = 7; rank >= 0; --rank) {
        for (int file = 0; file < 8; ++file) {
            const int index = rank * 8 + file;
            const int px = x + file * cell;
            const int py = y + (7 - rank) * cell;
            const bool pale_square = (file + rank) % 2 == 1;
            Color square = pale_square ? light_square : dark_square;
            if (index == last_from || index == last_to) {
                square = pale_square ? Color{179, 160, 91} : Color{105, 119, 62};
            }
            fill(renderer, px, py, cell, cell, square);
            if (file == 0) draw_text(renderer, std::to_string(rank + 1), px + 4, py + 4, 1, pale);
            if (rank == 0) draw_text(renderer, std::string(1, static_cast<char>('a' + file)), px + cell - 11, py + cell - 13, 1, pale);
            if (!(animation >= 0.0F && animation < 1.0F && index == last_to)) {
                piece_shape(renderer, view.pieces[static_cast<std::size_t>(index)], px + cell / 8, py + cell / 8, cell * 3 / 4);
            }
        }
    }
    if (animation >= 0.0F && animation < 1.0F && last_from >= 0 && last_to >= 0) {
        const auto from_file = last_from % 8;
        const auto from_rank = last_from / 8;
        const auto to_file = last_to % 8;
        const auto to_rank = last_to / 8;
        const float start_x = static_cast<float>(x + from_file * cell + cell / 8);
        const float start_y = static_cast<float>(y + (7 - from_rank) * cell + cell / 8);
        const float end_x = static_cast<float>(x + to_file * cell + cell / 8);
        const float end_y = static_cast<float>(y + (7 - to_rank) * cell + cell / 8);
        const int moving_x = static_cast<int>(start_x + (end_x - start_x) * animation);
        const int moving_y = static_cast<int>(start_y + (end_y - start_y) * animation);
        piece_shape(renderer, view.pieces[static_cast<std::size_t>(last_to)], moving_x, moving_y, cell * 3 / 4);
    }
    outline(renderer, x - 3, y - 3, size + 6, size + 6, mint);
}

void draw_ui(SDL_Renderer* renderer, const GameView& view, int width, int height, float animation) {
    fill(renderer, 0, 0, width, height, background);
    draw_text(renderer, "terminal chess", 34, 24, 3, mint);
    draw_text(renderer, "retro system // local match", 36, 58, 1, muted);
    fill(renderer, 32, 82, width - 64, 2, line);

    const int margin = 42;
    const int panel_width = std::max(260, std::min(380, width / 3));
    const int board_size = std::max(280, std::min(height - 160, width - panel_width - margin * 3));
    const int board_x = margin;
    const int board_y = std::max(102, (height - board_size) / 2 + 18);
    draw_board(renderer, view, board_x, board_y, board_size, animation);

    const int panel_x = board_x + board_size + 34;
    const int panel_y = board_y;
    const int actual_panel_width = width - panel_x - margin;
    fill(renderer, panel_x, panel_y, actual_panel_width, board_size, panel);
    outline(renderer, panel_x, panel_y, actual_panel_width, board_size, line);

    int y = panel_y + 18;
    draw_text(renderer, "game status", panel_x + 16, y, 1, muted);
    y += 22;
    draw_text(renderer, view.status, panel_x + 16, y, 2, view.in_check ? amber : mint, actual_panel_width - 28);
    y += view.status.size() > 24 ? 42 : 25;

    draw_text(renderer, "move " + std::to_string(view.fullmove_number), panel_x + 16, y, 1, pale);
    y += 20;
    draw_text(renderer, "last move  " + (view.last_move.empty() ? std::string("none") : view.last_move), panel_x + 16, y, 1, amber);
    y += 25;
    fill(renderer, panel_x + 16, y, actual_panel_width - 32, 1, line);
    y += 12;

    draw_text(renderer, "captured by white", panel_x + 16, y, 1, muted);
    y += 15;
    const int capture_cell = std::max(18, std::min(28, actual_panel_width / 10));
    draw_piece_list(renderer, view.captured_by_white, panel_x + 12, y, capture_cell);
    y += capture_cell + 12;
    draw_text(renderer, "captured by black", panel_x + 16, y, 1, muted);
    y += 15;
    draw_piece_list(renderer, view.captured_by_black, panel_x + 12, y, capture_cell);
    y += capture_cell + 8;
    fill(renderer, panel_x + 16, y, actual_panel_width - 32, 1, line);
    y += 12;

    draw_text(renderer, "move history", panel_x + 16, y, 1, muted);
    y += 18;
    const std::size_t max_lines = static_cast<std::size_t>(std::max(1, (panel_y + board_size - 100 - y) / 12));
    const std::size_t begin = view.history.size() > max_lines ? view.history.size() - max_lines : 0;
    for (std::size_t i = begin; i < view.history.size(); ++i) {
        const std::string label = std::to_string(i / 2 + 1) + (i % 2 == 0 ? ". " : "... ") + view.history[i].san;
        draw_text(renderer, label, panel_x + 16, y, 1, i + 1 == view.history.size() ? amber : pale);
        y += 11;
    }

    draw_text(renderer, "type moves and commands in the terminal", 35, height - 32, 1, muted);
}

bool handle_input(GameSession& game, CommandProcessor& commands, std::string& pending, bool& input_open) {
    pollfd descriptor{STDIN_FILENO, POLLIN, 0};
    if (!input_open || poll(&descriptor, 1, 0) <= 0 || !(descriptor.revents & (POLLIN | POLLHUP))) return false;

    char buffer[512];
    const auto bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (bytes <= 0) {
        input_open = false;
        return false;
    }

    for (ssize_t i = 0; i < bytes; ++i) {
        if (buffer[i] != '\n' && buffer[i] != '\r') {
            pending.push_back(buffer[i]);
            continue;
        }
        if (pending.empty()) continue;
        const auto result = commands.execute(pending);
        pending.clear();
        if (!result.output.empty()) std::cout << result.output << '\n';
        if (result.changed) std::cout << game.status_text() << '\n';
        if (result.action == CommandAction::quit) return true;
        std::cout << (game.view().white_to_move ? "weiß" : "schwarz") << " am zug > " << std::flush;
    }
    return false;
}

}

int run_gui(GameSession& game) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "sdl konnte nicht gestartet werden: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("terminal chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1180, 820, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::cerr << "fenster konnte nicht erstellt werden: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 900, 640);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::cerr << "renderer konnte nicht erstellt werden: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    std::cout << "terminal chess gestartet. spielzüge und befehle hier eingeben.\n";
    std::cout << help_text() << '\n';
    std::cout << "weiß am zug > " << std::flush;

    CommandProcessor commands(game);
    std::string pending;
    bool input_open = true;
    bool running = true;
    std::size_t previous_move_count = 0;
    std::string previous_last_move;
    Uint32 animation_started = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        if (handle_input(game, commands, pending, input_open)) running = false;

        const auto current_view = game.view();
        if (current_view.history.size() > previous_move_count && !current_view.history.empty()) {
            animation_started = SDL_GetTicks();
        }
        if (current_view.history.size() != previous_move_count || current_view.last_move != previous_last_move) {
            previous_move_count = current_view.history.size();
            previous_last_move = current_view.last_move;
        }
        const Uint32 elapsed = SDL_GetTicks() - animation_started;
        const float animation = animation_started != 0 && elapsed < 260 ? static_cast<float>(elapsed) / 260.0F : 1.0F;

        int width = 0;
        int height = 0;
        SDL_GetRendererOutputSize(renderer, &width, &height);
        draw_ui(renderer, current_view, width, height, animation);
        set_color(renderer, Color{4, 8, 8, 24});
        for (int y = 0; y < height; y += 5) {
            SDL_Rect scanline{0, y, width, 1};
            SDL_RenderFillRect(renderer, &scanline);
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

}
