#pragma once

#include <string>
#include <optional>
#include "Message.hpp"
#include "order/OrderTypes.hpp"

using obk::Side;
using obk::OrderType;
using obk::TimeInForce;


class Parser {
public:
    // Parses a single feed line → semantic message
    std::optional<FeedMessage> parseLine(const std::string& line);

private:
    static Side        parseSide(const std::string& s);
    static OrderType   parseOrderType(const std::string& s);
    static TimeInForce parseTIF(const std::string& s);
};
