#include "Parser.hpp"
#include <sstream>
#include <vector>
#include <stdexcept>

static std::vector<std::string> split(const std::string& line) {
    std::stringstream ss(line);
    std::vector<std::string> result;
    std::string token;
    while (std::getline(ss, token, ',')) result.push_back(token);
    return result;
}

Side Parser::parseSide(const std::string& s) {
    return (s == "B" || s == "BUY") ? Side::Buy : Side::Sell;
}

OrderType Parser::parseOrderType(const std::string& s) {
    if (s == "LIMIT") return OrderType::LIMIT;
    return OrderType::MARKET;
    // IOC/FOK are TimeInForce policies parsed separately via parseTIF().
}

TimeInForce Parser::parseTIF(const std::string& s) {
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    return TimeInForce::GTC;
}

// CSV format:
//   NEW,  <SIDE>, <PRICE_TICKS>, <QTY>, <ORDER_TYPE>[,<TIF>]
//   CANCEL, <ORDER_ID>
//   MODIFY, <ORDER_ID>, <NEW_QTY>
std::optional<FeedMessage> Parser::parseLine(const std::string& line) {
    if (line.empty() || line[0] == '#') return std::nullopt;

    auto tokens = split(line);
    if (tokens.empty()) return std::nullopt;

    FeedMessage msg;

    if (tokens[0] == "NEW") {
        if (tokens.size() < 5) return std::nullopt;

        msg.type      = FeedMessageType::NEW_ORDER;
        msg.side      = parseSide(tokens[1]);
        msg.price     = std::stoull(tokens[2]);   // integer ticks
        msg.quantity  = std::stoull(tokens[3]);
        msg.orderType = parseOrderType(tokens[4]);
        msg.tif       = (tokens.size() >= 6) ? parseTIF(tokens[5])
                                             : TimeInForce::GTC;
        return msg;
    }

    if (tokens[0] == "CANCEL") {
        if (tokens.size() < 2) return std::nullopt;
        msg.type    = FeedMessageType::CANCEL_ORDER;
        msg.orderId = std::stoull(tokens[1]);
        return msg;
    }

    if (tokens[0] == "MODIFY") {
        if (tokens.size() < 3) return std::nullopt;
        msg.type     = FeedMessageType::MODIFY_ORDER;
        msg.orderId  = std::stoull(tokens[1]);
        msg.quantity = std::stoull(tokens[2]);
        return msg;
    }

    return std::nullopt;
}
