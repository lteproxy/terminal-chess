#include "retro_chess/commands.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void play(retro_chess::GameSession& game, const std::string& move) {
    std::string message;
    require(game.play(move, message), move + " failed: " + message);
}

void test_opening_and_input_formats() {
    retro_chess::GameSession game;
    chess::Movelist legal;
    chess::movegen::legalmoves(legal, chess::Board{});
    require(legal.size() == 20, "the initial position should have twenty legal moves");

    play(game, "e3");
    require(!game.view().white_to_move, "san input should pass the turn to black");
    play(game, "e7e5");
    require(game.view().white_to_move, "uci input should pass the turn to white");
    require(game.view().history.size() == 2, "both move formats should be recorded");
    require(game.view().history[1].san == "e5", "uci moves should be stored in san");
}

void test_illegal_and_self_check_moves() {
    retro_chess::GameSession initial;
    std::string message;
    require(!initial.play("e2e5", message), "a pawn should not move three squares");

    retro_chess::GameSession pinned("4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");
    require(!pinned.play("e2f2", message), "a pinned rook must not expose its king");
    require(pinned.view().white_to_move, "an illegal move must not change the side to move");
}

void test_castling_and_undo() {
    retro_chess::GameSession game("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    play(game, "O-O");
    const auto after_castle = game.view();
    require(after_castle.pieces[6] == 'K' && after_castle.pieces[5] == 'R', "castling should move the king and rook");
    require(after_castle.history.back().san == "O-O", "castling should be recorded in san");

    std::string message;
    require(game.undo(message), "undo should reverse castling");
    const auto restored = game.view();
    require(restored.pieces[4] == 'K' && restored.pieces[7] == 'R', "undo should restore both castling pieces");
    require(restored.history.empty(), "undo should remove the move from history");
}

void test_en_passant_and_capture_undo() {
    retro_chess::GameSession game("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    play(game, "exd6");
    auto view = game.view();
    require(view.pieces[43] == 'P' && view.pieces[35] == '.', "en passant should move the pawn and remove the captured pawn");
    require(view.captured_by_white.size() == 1 && view.captured_by_white[0] == 'p', "en passant should record the captured pawn");
    std::string message;
    require(game.undo(message), "undo should reverse en passant");
    view = game.view();
    require(view.pieces[36] == 'P' && view.pieces[35] == 'p', "undo should restore both pawns");
    require(view.captured_by_white.empty(), "undo should remove the captured piece from the panel data");
}

void test_promotion() {
    retro_chess::GameSession game("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    play(game, "a7a8q");
    const auto view = game.view();
    require(view.pieces[56] == 'Q', "uci promotion should create a queen");
    require(view.history.back().uci == "a7a8q", "promotion should remain in the move record");
}

void test_checkmate_and_stalemate() {
    retro_chess::GameSession mate;
    play(mate, "f3");
    play(mate, "e5");
    play(mate, "g4");
    std::string claim_message;
    require(!mate.claim_draw("Qh4#", claim_message), "a mating announced move must not be accepted as a draw claim");
    play(mate, "Qh4#");
    require(mate.view().outcome == retro_chess::Outcome::black_wins, "fool's mate should end in a black win");
    require(mate.status_text().find("schachmatt") != std::string::npos, "mate status should be visible");

    retro_chess::GameSession stalemate("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    require(stalemate.view().outcome == retro_chess::Outcome::draw, "a stalemate position should be a draw");
    require(stalemate.status_text() == "patt", "stalemate should have a clear status");
}

void repeat_knights(retro_chess::GameSession& game, int times) {
    for (int i = 0; i < times; ++i) {
        play(game, "Nf3");
        play(game, "Nf6");
        play(game, "Ng1");
        play(game, "Ng8");
    }
}

void test_claimable_and_automatic_draws() {
    retro_chess::GameSession threefold;
    repeat_knights(threefold, 2);
    require(threefold.view().can_claim_draw, "the third occurrence should be claimable");
    std::string message;
    require(threefold.claim_draw("", message), "a threefold repetition should be claimable");
    require(threefold.view().outcome == retro_chess::Outcome::draw, "claiming repetition should end the game");

    retro_chess::GameSession fivefold;
    repeat_knights(fivefold, 4);
    require(fivefold.view().outcome == retro_chess::Outcome::draw, "the fifth occurrence should end the game automatically");
    require(fivefold.status_text().find("fünffache") != std::string::npos, "automatic repetition draw should be named");

    retro_chess::GameSession fifty("7k/8/8/8/8/8/8/KR6 w - - 99 1");
    require(!fifty.view().can_claim_draw, "a position at 99 halfmoves should need an announced move before it can be claimed");
    const bool fifty_claimed = fifty.claim_draw("Rb2", message);
    require(fifty_claimed, "a legal announced move should allow a fifty move claim: " + message + " | " + fifty.view().fen);

    retro_chess::GameSession seventy_five("7k/8/8/8/8/8/8/KR6 w - - 149 75");
    play(seventy_five, "Rb2");
    require(seventy_five.view().outcome == retro_chess::Outcome::draw, "the seventy five move rule should end the game automatically");
}

void test_insufficient_material() {
    retro_chess::GameSession game("8/8/8/8/8/8/4k3/6K1 w - - 0 1");
    require(game.view().outcome == retro_chess::Outcome::draw, "king versus king should be an automatic draw");
}

void test_commands_restart_undo_and_view_sync() {
    retro_chess::GameSession game;
    retro_chess::CommandProcessor commands(game);
    auto result = commands.execute("e4");
    require(result.changed && result.output.find("gespielt") != std::string::npos, "terminal moves should use the shared game session");
    require(game.view().pieces[28] == 'P' && !game.view().white_to_move, "the gui view should immediately reflect terminal input");
    require(commands.execute("moves").output.find("e4") != std::string::npos, "moves should show the move history");
    require(commands.execute("undo").changed, "the undo command should work");
    require(game.view().history.empty() && game.view().white_to_move, "undo should update the shared view");
    require(commands.execute("e2e4").changed, "uci input should work through terminal commands");
    require(commands.execute("restart").changed, "restart should work through terminal commands");
    require(game.view().history.empty() && game.view().pieces[12] == 'P', "restart should restore the opening position");
    require(commands.execute("quit").action == retro_chess::CommandAction::quit, "quit should signal the application");
}

}

int main() {
    const std::vector<std::pair<std::string, void (*)()>> tests{
        {"opening and input formats", test_opening_and_input_formats},
        {"illegal moves and self check", test_illegal_and_self_check_moves},
        {"castling and undo", test_castling_and_undo},
        {"en passant", test_en_passant_and_capture_undo},
        {"promotion", test_promotion},
        {"checkmate and stalemate", test_checkmate_and_stalemate},
        {"draw rules", test_claimable_and_automatic_draws},
        {"insufficient material", test_insufficient_material},
        {"commands and view sync", test_commands_restart_undo_and_view_sync}
    };

    int failed = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "ok  " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "fail " << name << ": " << error.what() << '\n';
        }
    }
    return failed == 0 ? 0 : 1;
}
