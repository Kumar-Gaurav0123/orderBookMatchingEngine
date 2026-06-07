#pragma once

#include <unordered_map>
#include <map>
#include <functional>
#include <vector>
#include <utility>

#include "../order/Order.hpp"
#include "../order/OrderTypes.hpp"
#include "PriceLevel.hpp"

// A two-sided limit order book with price-time priority matching.
//
// Price representation: integer ticks (uint64_t) throughout — no floating-point
// arithmetic in the hot path, which avoids rounding errors and is consistent
// with Order::price.
//
// Complexity:
//   addOrder     O(log P)   — std::map lookup/insert, P = # distinct price levels
//   cancelOrder  O(log P + 1) — map lookup + O(1) intrusive-list removal
//   modifyOrder  O(log P)
//   bestBid/Ask  O(1)       — map::begin() is a cached pointer

class OrderBook {
public:
    using OrderId  = uint64_t;
    using Price    = uint64_t;   // integer ticks; no floating-point in the hot path
    using Quantity = uint64_t;

    // Fired for every fill: (makerOrderId, takerOrderId, price, quantity).
    std::function<void(OrderId, OrderId, Price, Quantity)> onTrade;

    // Fired when a resting order is fully consumed by an incoming order.
    // The engine uses this to release the order from liveOrders_ and the
    // memory pool — without it, fully-filled resting orders leak forever.
    std::function<void(OrderId)> onRestingOrderFilled;

private:
    // Ask side: ascending (lowest ask at begin())
    std::map<Price, obk::PriceLevel> asks_;

    // Bid side: descending (highest bid at begin())
    std::map<Price, obk::PriceLevel, std::greater<Price>> bids_;

    // O(1) lookup: orderId → Order*
    std::unordered_map<OrderId, obk::Order*> orderIndex_;

public:
    OrderBook() = default;
    ~OrderBook() = default;

    // ------------------- Core API -------------------

    // Submit a new order. Handles LIMIT / MARKET / IOC / FOK.
    // Returns true if the order was accepted (rests in book or matched),
    // false if rejected (e.g. FOK not fully fillable, bad params).
    bool addOrder(obk::Order* order);

    bool cancelOrder(OrderId id);
    bool modifyOrder(OrderId id, Quantity newQty);

    // ------------------- Market Queries -------------------

    Price bestBid() const noexcept;
    Price bestAsk() const noexcept;

    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    // Returns up to `depth` levels as (price, total_quantity) pairs.
    // Bids are returned highest-first; asks lowest-first.
    std::vector<std::pair<Price, Quantity>> bidDepth(int depth) const;
    std::vector<std::pair<Price, Quantity>> askDepth(int depth) const;

    // ------------------- Debug -------------------
    void dumpBook() const;

private:
    // Returns false if a FOK order cannot be fully filled by the current book.
    bool canFillFOK(const obk::Order* order) const noexcept;

    void matchBuy(obk::Order* order);
    void matchSell(obk::Order* order);

    // Matches incoming against one price level. Fires onTrade per fill.
    // Returns the last resting order popped (fully filled), or nullptr.
    obk::Order* matchAgainstPriceLevel(obk::Order* incoming, obk::PriceLevel& level);

    void insertOrderToBook(obk::Order* order);
    obk::PriceLevel* getOrCreateLevel(bool isBuy, Price price);
    void removeOrderFromBook(obk::Order* order);
    void cleanupPriceLevel(bool isBuy, Price price);
};
