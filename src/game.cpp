#include "retro_chess/game.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace retro_chess {
namespace {

char piece_char(chess::Piece piece) {
    return static_cast<std::string>(piece)[0];
}

bool looks_like_uci(const std::string& text) {
    return (text.size() == 4 || text.size() == 5) &&
           chess::Square::is_valid_string_sq(std::string_view(text).substr(0, 2)) &&
           chess::Square::is_valid_string_sq(std::string_view(text).substr(2, 2));
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

}

GameSession::GameSession() = default;

GameSession::GameSession(std::string_view fen) {
    if (!board_.setFen(fen)) throw std::invalid_argument("fen is not valid");
    update_outcome();
}

bool GameSession::parse_move(const std::string& notation, chess::Move& move, std::string& error) const {
    try {
        if (looks_like_uci(notation)) {
            move = chess::uci::uciToMove(board_, lower(notation));
        } else {
            move = chess::uci::parseSan(board_, notation);
        }
    } catch (const std::exception&) {
        error = "das sieht nicht nach einem gültigen zug aus. nutze SAN wie Nf3 oder UCI wie g1f3.";
        return false;
    }

    if (move == chess::Move::NO_MOVE || !board_.isLegal(move)) {
        error = "dieser zug ist in der aktuellen stellung nicht erlaubt.";
        return false;
    }

    return true;
}

bool GameSession::play(const std::string& notation, std::string& message) {
    if (outcome_ != Outcome::playing) {
        message = "das spiel ist vorbei. nutze restart für eine neue partie.";
        return false;
    }

    chess::Move move;
    if (!parse_move(notation, move, message)) return false;

    const auto mover = board_.sideToMove();
    const auto captured = board_.at(move.to());
    const auto san = chess::uci::moveToSan(board_, move);
    const auto uci = chess::uci::moveToUci(move);
    board_.makeMove(move);
    char captured_char = '.';
    if (move.typeOf() == chess::Move::ENPASSANT) {
        captured_char = mover == chess::Color::WHITE ? 'p' : 'P';
    } else if (captured != chess::Piece::NONE && captured.color() != mover) {
        captured_char = piece_char(captured);
    }
    history_.push_back({move, san, uci, captured_char});

    if (captured_char != '.') {
        (mover == chess::Color::WHITE ? captured_by_white_ : captured_by_black_).push_back(captured_char);
    }

    update_outcome();
    message = "zug " + san + " gespielt.";
    return true;
}

bool GameSession::undo(std::string& message) {
    if (history_.empty()) {
        message = "es gibt keinen zug zum zurücknehmen.";
        return false;
    }

    const auto last = history_.back();
    const auto mover = ~board_.sideToMove();
    board_.unmakeMove(last.move);
    history_.pop_back();

    auto& captures = mover == chess::Color::WHITE ? captured_by_white_ : captured_by_black_;
    if (last.captured != '.' && !captures.empty()) captures.pop_back();

    clear_result();
    update_outcome();
    message = "letzten zug zurückgenommen.";
    return true;
}

void GameSession::restart() {
    board_ = chess::Board{};
    history_.clear();
    captured_by_white_.clear();
    captured_by_black_.clear();
    clear_result();
}

bool GameSession::claim_draw(const std::string& announced_move, std::string& message) {
    if (outcome_ != Outcome::playing) {
        message = "das spiel ist bereits vorbei.";
        return false;
    }

    chess::Board candidate = board_;
    if (!announced_move.empty()) {
        chess::Move move;
        if (!parse_move(announced_move, move, message)) return false;
        candidate.makeMove(move);
        chess::Movelist legal_moves;
        chess::movegen::legalmoves(legal_moves, candidate);
        if (legal_moves.empty() && candidate.inCheck()) {
            message = "der angekündigte zug wäre schachmatt und kann kein remis beanspruchen.";
            return false;
        }
    }

    const bool repetition = candidate.isRepetition(2);
    const bool fifty_moves = candidate.halfMoveClock() >= 100;
    if (!repetition && !fifty_moves) {
        message = "ein remis kann in dieser stellung nicht beansprucht werden.";
        return false;
    }

    outcome_ = Outcome::draw;
    result_reason_ = repetition ? "remis durch dreifache stellungswiederholung" : "remis nach der 50-züge-regel";
    message = result_reason_ + ".";
    return true;
}

void GameSession::clear_result() {
    outcome_ = Outcome::playing;
    result_reason_.clear();
}

void GameSession::update_outcome() {
    chess::Movelist legal_moves;
    chess::movegen::legalmoves(legal_moves, board_);
    if (legal_moves.empty()) {
        if (board_.inCheck()) {
            outcome_ = board_.sideToMove() == chess::Color::WHITE ? Outcome::black_wins : Outcome::white_wins;
            result_reason_ = "schachmatt";
        } else {
            outcome_ = Outcome::draw;
            result_reason_ = "patt";
        }
        return;
    }

    if (board_.isInsufficientMaterial()) {
        outcome_ = Outcome::draw;
        result_reason_ = "remis durch unzureichendes material";
    } else if (board_.isRepetition(4)) {
        outcome_ = Outcome::draw;
        result_reason_ = "remis durch fünffache stellungswiederholung";
    } else if (board_.halfMoveClock() >= 150) {
        outcome_ = Outcome::draw;
        result_reason_ = "remis nach der 75-züge-regel";
    }
}

GameView GameSession::view() const {
    GameView result;
    for (int index = 0; index < 64; ++index) {
        result.pieces[static_cast<std::size_t>(index)] = piece_char(board_.at(chess::Square(index)));
    }
    result.history = history_;
    result.captured_by_white = captured_by_white_;
    result.captured_by_black = captured_by_black_;
    result.last_move = history_.empty() ? "" : history_.back().san;
    result.outcome = outcome_;
    result.white_to_move = board_.sideToMove() == chess::Color::WHITE;
    result.in_check = board_.inCheck();
    result.can_claim_draw = outcome_ == Outcome::playing && (board_.isRepetition(2) || board_.halfMoveClock() >= 100);
    result.fullmove_number = static_cast<int>(board_.fullMoveNumber());
    result.fen = board_.getFen();
    result.status = status_text();
    return result;
}

std::string GameSession::board_text() const {
    std::ostringstream output;
    for (int rank = 7; rank >= 0; --rank) {
        output << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            output << piece_char(board_.at(chess::Square(rank * 8 + file))) << ' ';
        }
        output << '\n';
    }
    output << "\n   a b c d e f g h";
    return output.str();
}

std::string GameSession::moves_text() const {
    if (history_.empty()) return "bisher keine züge.";
    std::ostringstream output;
    for (std::size_t i = 0; i < history_.size(); ++i) {
        if (i % 2 == 0) output << i / 2 + 1 << ". ";
        output << history_[i].san;
        output << (i % 2 == 0 ? ' ' : '\n');
    }
    return output.str();
}

std::string GameSession::status_text() const {
    if (outcome_ == Outcome::white_wins) return "weiß gewinnt durch " + result_reason_;
    if (outcome_ == Outcome::black_wins) return "schwarz gewinnt durch " + result_reason_;
    if (outcome_ == Outcome::draw) return result_reason_;
    std::string status = board_.sideToMove() == chess::Color::WHITE ? "weiß ist am zug" : "schwarz ist am zug";
    if (board_.inCheck()) status += " und steht im schach";
    return status;
}

}
