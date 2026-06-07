#include "Replay.hpp"
#include "EngineCommand.hpp"
#include "../utils/Logging.hpp"
#include <fstream>

Replay::Replay(MatchingEngine& engine)
    : engine_(engine) {}

void Replay::run(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) {
        Logger::error("Replay: Could not open file: " + filepath);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto parsed = parser_.parseLine(line);
        if (!parsed) continue;

        switch (parsed->type) {
            case FeedMessageType::NEW_ORDER:
                engine_.postCommand(EngineCommand::New(
                    parsed->side, parsed->orderType, parsed->tif,
                    parsed->price, parsed->quantity));
                break;

            case FeedMessageType::CANCEL_ORDER:
                engine_.postCommand(EngineCommand::Cancel(parsed->orderId));
                break;

            case FeedMessageType::MODIFY_ORDER:
                engine_.postCommand(EngineCommand::Modify(
                    parsed->orderId, parsed->quantity));
                break;
        }
    }

    Logger::info("Replay completed.");
}
