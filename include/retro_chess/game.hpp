#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "chess.hpp"

namespace retro_chess {

enum class Outcome {
    playing,
    white_wins,
    black_wins,
    draw
};

struct MoveRecord {
    chess::Move move;
    std::string san;
    std::string uci;
    char captured = '.';
};

struct GameView {
    std::array<char, 64> pieces{};
    std::vector<MoveRecord> history;
    std::vector<char> captured_by_white;
    std::vector<char> captured_by_black;
    std::string last_move;
    std::string status;
    std::string fen;
    Outcome outcome = Outcome::playing;
    bool white_to_move = true;
    bool in_check = false;
    bool can_claim_draw = false;
    int fullmove_number = 1;
};

class GameSession {
public:
    GameSession();
    explicit GameSession(std::string_view fen);

    bool play(const std::string& notation, std::string& message);
    bool undo(std::string& message);
    void restart();
    bool claim_draw(const std::string& announced_move, std::string& message);
    GameView view() const;
    std::string board_text() const;
    std::string moves_text() const;
    std::string status_text() const;

private:
    chess::Board board_;
    std::vector<MoveRecord> history_;
    std::vector<char> captured_by_white_;
    std::vector<char> captured_by_black_;
    Outcome outcome_ = Outcome::playing;
    std::string result_reason_;

    bool parse_move(const std::string& notation, chess::Move& move, std::string& error) const;
    void update_outcome();
    void clear_result();
};

}
