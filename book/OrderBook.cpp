#include "OrderBook.hpp"
#include <iostream>
#include <algorithm>

using obk::Order;
using obk::PriceLevel;
using obk::Side;
using obk::OrderType;
using obk::TimeInForce;

// ============================================================
//  PUBLIC API
// ============================================================

bool OrderBook::addOrder(Order* order) {
    if (!order || order->remaining == 0) return false;

    // FOK pre-check: reject immediately if the full quantity cannot be filled
    // without resting. We never let a FOK order touch the book.
    if (order->tif == TimeInForce::FOK && !canFillFOK(order)) {
        return false;   // caller (MatchingEngine) fires onOrderRejected
    }

    orderIndex_[order->id] = order;

    if (order->side == Side::Buy)
        matchBuy(order);
    else
        matchSell(order);

    if (order->remaining > 0) {
        if (order->type == OrderType::LIMIT && order->tif == TimeInForce::GTC) {
            // Resting limit order — add to book.
            insertOrderToBook(order);
        } else {
            // IOC / FOK leftovers / market orders that couldn't fully fill
            // are discarded (never rest in the book).
            orderIndex_.erase(order->id);
        }
    }
    return true;
}

bool OrderBook::cancelOrder(OrderId id) {
    auto it = orderIndex_.find(id);
    if (it == orderIndex_.end()) return false;

    Order* order = it->second;
    removeOrderFromBook(order);
    orderIndex_.erase(it);
    return true;
}

bool OrderBook::modifyOrder(OrderId id, Quantity newQty) {
    auto it = orderIndex_.find(id);
    if (it == orderIndex_.end()) return false;

    Order* order = it->second;

    // Remove from book first (preserves intrusive-list consistency).
    removeOrderFromBook(order);

    // Update remaining and original qty. Lose queue position per exchange convention.
    order->qty       = newQty;
    order->remaining = newQty;

    // Re-insert at back of price level (lost time-priority).
    insertOrderToBook(order);
    return true;
}

// ============================================================
//  FOK PRE-CHECK
// ============================================================

bool OrderBook::canFillFOK(const Order* order) const noexcept {
    Quantity needed = order->remaining;
    const bool isMarket = (order->type == OrderType::MARKET);

    if (order->side == Side::Buy) {
        for (auto& [price, level] : asks_) {
            // MARKET FOK: accept any ask price.
            // LIMIT  FOK: stop as soon as ask exceeds our limit.
            if (!isMarket && price > order->price) break;
            if (needed <= level.totalQuantity()) return true;
            needed -= level.totalQuantity();
        }
    } else {
        for (auto& [price, level] : bids_) {
            if (!isMarket && price < order->price) break;
            if (needed <= level.totalQuantity()) return true;
            needed -= level.totalQuantity();
        }
    }
    return false;
}

// ============================================================
//  MATCHING LOGIC
// ============================================================

void OrderBook::matchBuy(Order* order) {
    while (order->remaining > 0 && !asks_.empty()) {
        Price bestAsk = asks_.begin()->first;

        if (order->type == OrderType::MARKET || order->price >= bestAsk) {
            PriceLevel& level = asks_.begin()->second;
            matchAgainstPriceLevel(order, level);
            cleanupPriceLevel(/*isBuy=*/false, bestAsk);
        } else {
            break;
        }
    }
}

void OrderBook::matchSell(Order* order) {
    while (order->remaining > 0 && !bids_.empty()) {
        Price bestBid = bids_.begin()->first;

        if (order->type == OrderType::MARKET || order->price <= bestBid) {
            PriceLevel& level = bids_.begin()->second;
            matchAgainstPriceLevel(order, level);
            cleanupPriceLevel(/*isBuy=*/true, bestBid);
        } else {
            break;
        }
    }
}

Order* OrderBook::matchAgainstPriceLevel(Order* incoming, PriceLevel& level) {
    Order* lastPopped = nullptr;

    while (incoming->remaining > 0 && !level.empty()) {
        Order* top = level.front();

        Quantity traded = std::min(incoming->remaining, top->remaining);

        // Update remaining quantities BEFORE firing callbacks so that
        // listeners see the post-fill state.
        incoming->remaining -= traded;
        top->remaining      -= traded;

        // Keep level's total in sync with the actual fill (O(1)).
        level.onFill(traded);

        // Fire trade event for every fill. The engine wires this to produce
        // TradeEvent records and update risk/P&L systems.
        if (onTrade) {
            // Resting order is always maker; incoming is always taker.
            onTrade(top->id, incoming->id, level.price(), traded);
        }

        if (top->remaining == 0) {
            lastPopped = level.popFront();
            if (lastPopped) {
                orderIndex_.erase(lastPopped->id);
                // Notify the engine so it can release the order from
                // liveOrders_ and return its memory to the pool.
                // Without this callback, fully-filled resting orders
                // leak: they're gone from orderIndex_ but the engine
                // still holds the pointer in liveOrders_.
                if (onRestingOrderFilled)
                    onRestingOrderFilled(lastPopped->id);
            }
        }
    }

    return lastPopped;
}

// ============================================================
//  BOOK INTERNALS
// ============================================================

void OrderBook::insertOrderToBook(Order* order) {
    PriceLevel* level = getOrCreateLevel(order->side == Side::Buy, order->price);
    level->addOrder(order);
}

OrderBook::PriceLevel* OrderBook::getOrCreateLevel(bool isBuy, Price price) {
    if (isBuy) {
        auto [it, inserted] = bids_.emplace(price, obk::PriceLevel(price));
        return &it->second;
    } else {
        auto [it, inserted] = asks_.emplace(price, obk::PriceLevel(price));
        return &it->second;
    }
}

void OrderBook::removeOrderFromBook(Order* order) {
    bool isBuy = (order->side == Side::Buy);

    if (isBuy) {
        auto it = bids_.find(order->price);
        if (it == bids_.end()) return;
        it->second.removeOrder(order);
        if (it->second.empty()) bids_.erase(it);
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) return;
        it->second.removeOrder(order);
        if (it->second.empty()) asks_.erase(it);
    }
}

void OrderBook::cleanupPriceLevel(bool isBuy, Price price) {
    if (isBuy) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second.empty()) bids_.erase(it);
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end() && it->second.empty()) asks_.erase(it);
    }
}

// ============================================================
//  QUERIES
// ============================================================

OrderBook::Price OrderBook::bestBid() const noexcept {
    return bids_.empty() ? 0 : bids_.begin()->first;
}

OrderBook::Price OrderBook::bestAsk() const noexcept {
    return asks_.empty() ? 0 : asks_.begin()->first;
}

std::vector<std::pair<OrderBook::Price, OrderBook::Quantity>>
OrderBook::bidDepth(int depth) const {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(depth);
    for (auto& [price, level] : bids_) {
        if (static_cast<int>(result.size()) >= depth) break;
        result.emplace_back(price, level.totalQuantity());
    }
    return result;
}

std::vector<std::pair<OrderBook::Price, OrderBook::Quantity>>
OrderBook::askDepth(int depth) const {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(depth);
    for (auto& [price, level] : asks_) {
        if (static_cast<int>(result.size()) >= depth) break;
        result.emplace_back(price, level.totalQuantity());
    }
    return result;
}

// ============================================================
//  DEBUG
// ============================================================

void OrderBook::dumpBook() const {
    std::cout << "=== ORDER BOOK ===\n";
    std::cout << "--- ASKS (lowest first) ---\n";
    for (auto it = asks_.rbegin(); it != asks_.rend(); ++it)
        std::cout << "  " << it->first << " x " << it->second.totalQuantity()
                  << " (" << it->second.size() << " orders)\n";
    std::cout << "--- BIDS (highest first) ---\n";
    for (auto& [price, lvl] : bids_)
        std::cout << "  " << price << " x " << lvl.totalQuantity()
                  << " (" << lvl.size() << " orders)\n";
}
