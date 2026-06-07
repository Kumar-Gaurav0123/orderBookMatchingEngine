#pragma once

#include <string>
#include "Parser.hpp"
#include "../book/MatchingEngine.hpp"

class Replay {
public:
    explicit Replay(MatchingEngine& engine);

    // Runs through a file of feed messages
    void run(const std::string& filepath);

private:
    MatchingEngine& engine_;
    Parser parser_;
};
