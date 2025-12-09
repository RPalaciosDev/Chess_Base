#include "Chess.h"
#include <limits>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <algorithm>
#include "ChessSquare.h"

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

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

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    syncEngineFromGrid();
    regenerateLegalMoves();

    startGame();
}

bool Chess::placePieceFromFEN(char fenChar, int x, int y)
{
    // Map FEN character to piece type, player, and gameTag
    static const std::unordered_map<char, std::tuple<int, int, int>> fenPieceMap = {
        // White pieces
        {'P', {Pawn, 0, 1}},
        {'N', {Knight, 0, 2}},
        {'B', {Bishop, 0, 3}},
        {'R', {Rook, 0, 4}},
        {'Q', {Queen, 0, 5}},
        {'K', {King, 0, 6}},

        // Black pieces
        {'p', {Pawn, 1, 129}},
        {'n', {Knight, 1, 130}},
        {'b', {Bishop, 1, 131}},
        {'r', {Rook, 1, 132}},
        {'q', {Queen, 1, 133}},
        {'k', {King, 1, 134}}};

    // Replace nasty type with auto
    auto it = fenPieceMap.find(fenChar);

    // if key is found
    if (it != fenPieceMap.end())
    {
        // destructure the data the iterator is pointing at into its respective parts
        auto [pieceId, playerNumber, gameTag] = it->second;
        ChessPiece piece = static_cast<ChessPiece>(pieceId);

        // Create and place the piece
        Bit *bit = PieceForPlayer(playerNumber, piece);
        bit->setGameTag(gameTag);
        ChessSquare *square = _grid->getSquare(x, y);
        if (square)
        {
            bit->setPosition(square->getPosition());
            square->setBit(bit);
        }
        return true;
    }
    return false;
}

void Chess::FENtoBoard(const std::string &fen)
{
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)

    // Clear the board first
    _grid->forEachSquare([](ChessSquare *square, int x, int y)
                         { square->destroyBit(); });

    int y = 7; // Start at rank 8
    int x = 0; // Start at file a

    for (size_t i = 0; i < fen.length() && y >= 0; i++)
    {
        char c = fen[i];

        // Shift to next rank on slashes and reset the file
        if (c == '/')
        {
            y--;
            x = 0;
        }
        // Skip consecutive empty squares
        else if (c >= '1' && c <= '8')
        {
            x += (c - '0');
        }
        else
        {
            // Place piece if valid FEN character
            if (placePieceFromFEN(c, x, y) && x < 8)
            {
                x++;
            }
        }
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    auto srcSquare = dynamic_cast<ChessSquare *>(&src);
    if (!srcSquare)
    {
        return false;
    }

    regenerateLegalMoves();

    Player *current = getCurrentPlayer();
    int currentPlayer = current ? current->playerNumber() : 0;
    int pieceColor = (bit.gameTag() & 128) ? 1 : 0;
    if (pieceColor != currentPlayer)
    {
        return false;
    }

    int fromIndex = srcSquare->getSquareIndex();
    return std::any_of(_legalMoves.begin(), _legalMoves.end(), [fromIndex](const BitMove &move)
                       { return move.from == fromIndex; });
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    auto srcSquare = dynamic_cast<ChessSquare *>(&src);
    auto dstSquare = dynamic_cast<ChessSquare *>(&dst);
    if (!srcSquare || !dstSquare)
    {
        return false;
    }

    if (_legalMoves.empty())
    {
        regenerateLegalMoves();
    }

    int fromIndex = srcSquare->getSquareIndex();
    int toIndex = dstSquare->getSquareIndex();

    auto it = std::find_if(_legalMoves.begin(), _legalMoves.end(), [&](const BitMove &move)
                           { return move.from == fromIndex && move.to == toIndex; });
    return it != _legalMoves.end();
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) { 
        square->destroyBit(); 
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
         s += pieceNotation(x, y ); 
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, static_cast<ChessPiece>(Pawn)));
        } else {
            square->setBit(nullptr);
        } });
    syncEngineFromGrid();
    regenerateLegalMoves();
}

void Chess::syncEngineFromGrid()
{
    std::string state = stateString();
    Player *current = getCurrentPlayer();
    char playerColor = (current && current->playerNumber() == 0) ? WHITE : BLACK;
    _engineState.init(state.c_str(), playerColor);
}

void Chess::regenerateLegalMoves()
{
    syncEngineFromGrid();
    _legalMoves = _engineState.generateAllMoves();
}
