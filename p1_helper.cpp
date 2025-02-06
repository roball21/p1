/*
 * P1 HELPER
 * ---------
 * Author: Thoshitha Gamage
 * Date: 01/29/2025
 * License: MIT License
 * Description: This helper function loads the video game data from a .db file into a vector of Game structures.
 *              This code is intended to be used as a helper function in CS447 Spring 2025 P1 server code.
 */

#include "p1_helper.h"
#include <fstream>
#include <sstream>

std::vector<Game> loadGamesFromFile(const std::string& filename) {
    std::vector<Game> games;
    std::ifstream file(filename);
    std::string line;

    // Skip header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string id, title, platform, genre, year_str, esrb, availability_str, copies_str;

        std::getline(ss, id, ';');
        std::getline(ss, title, ';');
        std::getline(ss, platform, ';');
        std::getline(ss, genre, ';');
        std::getline(ss, year_str, ';');
        std::getline(ss, esrb, ';');
        std::getline(ss, availability_str, ';');
        std::getline(ss, copies_str, ';');

        Game game;
        game.title = title;
        game.platform = platform;
        game.genre = genre;
        game.year = std::stoi(year_str);
        game.esrb = esrb;
        game.available = (availability_str == "True");
        game.copies = std::stoi(copies_str);

        games.push_back(game);
    }

    return games;
}
