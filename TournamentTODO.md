Add the Tournament.h file into your classes directory as is. There is no need to change your makefile.

In Application.cpp add the following from below (the include and the TournamentClient pointer)

```cpp
#define TOURNAMENT_IMPLEMENTATION
#include "classes/Tournament.h"

namespace ClassGame {
        //
        // our global variables
        //
        Game *game = nullptr;
        TournamentClient *client = nullptr;
```

Then in your braced section that has all the game selections, add from the following:

```cpp
                    if (ImGui::Button("Start Chess")) {
                        game = new Chess();
                        game->setUpBoard();
                    }
                    if (ImGui::Button("AI vs AI"))
                    {
                        game = new Chess();
                        game->_gameOptions.AIvsAI = true;
                        game->setUpBoard();
                    }
                    if (ImGui::Button("Start Online Tournament")) {
                        game = new Chess();
                        game->setUpBoard();
                        client = new TournamentClient((Chess *)game, "Sherlock");       // THIS SHOULD BE YOUR BOT NAME
                        client->connect("13.223.80.180", 5000);
                    }
```
(Note, the AI vs AI is not necessay, but it's a good test for your code)

Finally change the GameWindow portion of Application.cpp to the following:

```cpp
                ImGui::Begin("GameWindow");
                if (client) {
                    client->update();
                } else if (game) {
                    if (game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
                    {
                        game->updateAI();
                    }
                    game->drawFrame();
                }
```

NOW, in your Chess.h add the following to the Chess class public section:
```cpp
    // Tournament support methods
    void setBoardFromFEN(const std::string& fen);
    BitMove getLastAIMove() const { return _lastAIMove; }
    std::string getFEN() const;

    // Get current player color (WHITE=1, BLACK=-1)
    int getCurrentPlayerColor() const { return _currentPlayer; }

    // you can make this variable private, it's just grouped with the public methods for convenience
    BitMove _lastAIMove;  // Stores the last move calculated by AI (for tournament)
```

And finally in Chess.cpp adjust UpdateAI to do the following (and also make sure you include the C++ library <sstream>)

```cpp
void Chess::updateAI() 
{
    _lastAIMove = BitMove();  // Reset last AI move (add this at the top of updateAI())
```

and in the part of the code that moves:
```cpp

    // Make the best move
    if(bestVal != negInfinite) {
        _lastAIMove = bestMove;
```

then FINALLY add these methods to the bottom of your Chess.cpp which are helpers.

```cpp
// Tournament support: Set board from FEN and reinitialize game state for AI
void Chess::setBoardFromFEN(const std::string& fen) {
    // Parse FEN string - can be full FEN or just piece placement
    std::string piecePlacement = fen;
    std::string activeColor = "w";
    std::string castling = "KQkq";
    std::string enPassant = "-";

    // Check if this is a full FEN string (has spaces)
    size_t spacePos = fen.find(' ');
    if (spacePos != std::string::npos) {
        // Parse full FEN
        std::istringstream fenStream(fen);
        fenStream >> piecePlacement >> activeColor >> castling >> enPassant;
    }

    // Set visual board from piece placement
    FENtoBoard(piecePlacement);

    // Determine current player from FEN
    _currentPlayer = (activeColor == "w" || activeColor == "W") ? WHITE : BLACK;

    // Reinitialize game state so AI sees correct board
    _gamestate.init(stateString().c_str(), _currentPlayer);

    // TODO: Parse castling rights and en passant from FEN for more accurate state
    // For now, the basic state is sufficient for AI to calculate moves

    // Generate legal moves for the new position
    _moves = _gamestate.generateAllMoves();

    std::cout << "[Tournament] Board set from FEN. Player: "
              << (_currentPlayer == WHITE ? "White" : "Black")
              << ", Legal moves: " << _moves.size() << std::endl;
}

// Tournament support: Generate FEN string from current board
std::string Chess::getFEN() const {
    std::string fen;
    fen.reserve(90);

    // Piece placement (from rank 8 to rank 1)
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            char piece = pieceNotation(file, rank);
            if (piece == '0') {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += piece;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    // Active color
    fen += ' ';
    fen += (_currentPlayer == WHITE) ? 'w' : 'b';

    // Castling availability (simplified - always report based on piece positions)
    fen += ' ';
    std::string castling;

    // Check if white can castle (king on e1, rooks on a1/h1)
    char e1 = pieceNotation(4, 0);
    char a1 = pieceNotation(0, 0);
    char h1 = pieceNotation(7, 0);
    if (e1 == 'K') {
        if (h1 == 'R') castling += 'K';
        if (a1 == 'R') castling += 'Q';
    }

    // Check if black can castle (king on e8, rooks on a8/h8)
    char e8 = pieceNotation(4, 7);
    char a8 = pieceNotation(0, 7);
    char h8 = pieceNotation(7, 7);
    if (e8 == 'k') {
        if (h8 == 'r') castling += 'k';
        if (a8 == 'r') castling += 'q';
    }

    fen += castling.empty() ? "-" : castling;

    // En passant target square (simplified - report as '-')
    fen += " -";

    // Halfmove clock (simplified)
    fen += " 0";

    // Fullmove number (simplified)
    fen += " 1";

    return fen;
}
```

If you don't have pieceNotation here it is:

```cpp
// definition for Chess.h
char pieceNotation(int x, int y) const;
// method
char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}
``` 
When you compile now it should be ready for the tournament! When you start the game up you should see a button for "Start Online Tournament" and if will send some commands to check you have done everything correctly (requires chess tournament server to be online, so ping Graeme first to make sure it's running)

The logging should look like this:

```
[Tournament] Sent to ADMIN: TEST:PONG
[Tournament] Responded to PING from ADMIN
[Tournament] Received from ADMIN: TEST:FEN:rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1
[Tournament] Comms test: Calculating move for test position...
[Tournament] Board set from FEN. Player: Black, Legal moves: 20
Moves checked: 35026911 (2235198.52 boards/s)
[Tournament] Sent to ADMIN: TEST:MOVE:57,42
[Tournament] Comms test: Sent test move TEST:MOVE:57,42
```
