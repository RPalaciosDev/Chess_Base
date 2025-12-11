#define TOURNAMENT_IMPLEMENTATION
#include "Application.h"
#include "imgui/imgui.h"
#include "classes/TicTacToe.h"
#include "classes/Checkers.h"
#include "classes/Othello.h"
#include "classes/Connect4.h"
#include "classes/Chess.h"
#include "classes/Tournament.h"

namespace ClassGame
{
    //
    // our global variables
    //
    Game *game = nullptr;
    TournamentClient *client = nullptr;
    bool gameOver = false;
    int gameWinner = -1;

    //
    // game starting point
    // this is called by the main render loop in main.cpp
    //
    void GameStartUp()
    {
        game = nullptr;
    }

    //
    // game render loop
    // this is called by the main render loop in main.cpp
    //
    void RenderGame()
    {
        ImGui::DockSpaceOverViewport();

        // ImGui::ShowDemoWindow();

        ImGui::Begin("Settings");

        if (gameOver)
        {
            ImGui::Text("Game Over!");
            ImGui::Text("Winner: %d", gameWinner);
            if (ImGui::Button("Reset Game"))
            {
                game->stopGame();
                game->setUpBoard();
                gameOver = false;
                gameWinner = -1;
            }
        }
        if (!game)
        {
            if (ImGui::Button("Start Tic-Tac-Toe"))
            {
                game = new TicTacToe();
                game->setUpBoard();
            }
            if (ImGui::Button("Start Checkers"))
            {
                game = new Checkers();
                game->setUpBoard();
            }
            if (ImGui::Button("Start Othello"))
            {
                game = new Othello();
                game->setUpBoard();
            }
            if (ImGui::Button("Start Connect 4"))
            {
                game = new Connect4();
                game->setUpBoard();
            }
            if (ImGui::Button("Start Chess"))
            {
                game = new Chess();
                game->setUpBoard();
            }
            if (ImGui::Button("AI vs AI"))
            {
                game = new Chess();
                game->_gameOptions.AIvsAI = true;
                game->setUpBoard();
            }
            if (ImGui::Button("Start Online Tournament"))
            {
                game = new Chess();
                game->setUpBoard();
                client = new TournamentClient((Chess *)game, "Decoy"); // THIS SHOULD BE YOUR BOT NAME
                client->connect("13.223.80.180", 5000);
            }
        }
        else
        {
            ImGui::Text("Current Player Number: %d", game->getCurrentPlayer()->playerNumber());
            std::string stateString = game->stateString();
            int stride = game->_gameOptions.rowX;
            int height = game->_gameOptions.rowY;

            for (int y = 0; y < height; y++)
            {
                ImGui::Text("%s", stateString.substr(y * stride, stride).c_str());
            }
            ImGui::Text("Current Board State: %s", game->stateString().c_str());
        }
        ImGui::End();

        ImGui::Begin("GameWindow");
        if (client)
        {
            client->update();
        }
        else if (game)
        {
            if (game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
            {
                game->updateAI();
            }
            game->drawFrame();
        }
        ImGui::End();
    }

    //
    // end turn is called by the game code at the end of each turn
    // this is where we check for a winner
    //
    void EndOfTurn()
    {
        Player *winner = game->checkForWinner();
        if (winner)
        {
            gameOver = true;
            gameWinner = winner->playerNumber();
        }
        if (game->checkForDraw())
        {
            gameOver = true;
            gameWinner = -1;
        }
    }
}
