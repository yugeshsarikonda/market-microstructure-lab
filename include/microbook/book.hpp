#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace microbook {

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
enum class Side { Buy, Sell };

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity remaining;
};

struct Trade {
    OrderId maker;
    OrderId taker;
    Price price;
    Quantity quantity;
    bool operator==(const Trade& other) const {
        return maker == other.maker && taker == other.taker &&
               price == other.price && quantity == other.quantity;
    }
};

struct Submission {
    OrderId id;
    Quantity filled = 0;
    Quantity resting = 0;
    Quantity unfilled = 0;
    std::vector<Trade> trades;
};

struct Level {
    Price price;
    Quantity quantity;
    std::size_t orders;
    bool operator==(const Level& other) const {
        return price == other.price && quantity == other.quantity && orders == other.orders;
    }
};

// Single-threaded price-time-priority model. Prices are integer ticks, not money.
class Book {
public:
    Book() = default;
    Book(const Book&) = delete;
    Book& operator=(const Book&) = delete;
    Book(Book&&) = delete;
    Book& operator=(Book&&) = delete;

    Submission limit(OrderId id, Side side, Price price, Quantity quantity) {
        if (price <= 0) throw std::invalid_argument("limit price must be positive");
        return submit(id, side, price, quantity);
    }

    // Market orders are immediate-or-cancel. Unfilled quantity never rests.
    Submission market(OrderId id, Side side, Quantity quantity) {
        return submit(id, side, std::nullopt, quantity);
    }

    bool cancel(OrderId id) {
        const auto found = active_.find(id);
        if (found == active_.end()) return false;
        const auto location = found->second;
        if (location.side == Side::Buy) erase(bids_, location);
        else erase(asks_, location);
        active_.erase(found);
        return true;
    }

    std::optional<Order> find(OrderId id) const {
        const auto found = active_.find(id);
        if (found == active_.end()) return std::nullopt;
        return *found->second.position;
    }

    std::optional<Price> best_bid() const {
        return bids_.empty() ? std::nullopt : std::optional<Price>(bids_.begin()->first);
    }

    std::optional<Price> best_ask() const {
        return asks_.empty() ? std::nullopt : std::optional<Price>(asks_.begin()->first);
    }

    std::vector<Level> depth(Side side) const {
        validate_side(side);
        return side == Side::Buy ? aggregate(bids_) : aggregate(asks_);
    }

    std::size_t size() const { return active_.size(); }

    // Expensive diagnostic intended for tests, rather than a fast trading path.
    void check_invariants() const {
        std::unordered_set<OrderId> found;
        inspect(bids_, Side::Buy, found);
        inspect(asks_, Side::Sell, found);
        if (found.size() != active_.size()) throw std::logic_error("active index differs from queues");
        if (best_bid() && best_ask() && *best_bid() >= *best_ask()) {
            throw std::logic_error("resting book is crossed");
        }
    }

private:
    using Queue = std::list<Order>;
    using Bids = std::map<Price, Queue, std::greater<Price>>;
    using Asks = std::map<Price, Queue>;
    struct Location {
        Side side;
        Price price;
        Queue::iterator position;
    };
    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Location> active_;
    std::unordered_set<OrderId> seen_;

    static void validate_side(Side side) {
        if (side != Side::Buy && side != Side::Sell) throw std::invalid_argument("invalid side");
    }

    Submission submit(OrderId id, Side side, std::optional<Price> price, Quantity quantity) {
        validate_side(side);
        if (id == 0) throw std::invalid_argument("order ID must be nonzero");
        if (quantity <= 0) throw std::invalid_argument("quantity must be positive");
        if (seen_.count(id)) throw std::invalid_argument("order ID already used in this session");
        seen_.insert(id);
        Submission result{id, 0, 0, 0, {}};
        Quantity remaining = quantity;
        if (side == Side::Buy) match(asks_, id, side, price, remaining, result);
        else match(bids_, id, side, price, remaining, result);
        result.filled = quantity - remaining;
        if (price && remaining) {
            Order order{id, side, *price, remaining};
            if (side == Side::Buy) rest(bids_, order);
            else rest(asks_, order);
            result.resting = remaining;
        } else {
            result.unfilled = remaining;
        }
        return result;
    }

    template <typename Levels>
    void match(Levels& levels, OrderId taker, Side side, std::optional<Price> limit,
               Quantity& remaining, Submission& result) {
        while (remaining && !levels.empty()) {
            auto best = levels.begin();
            const Price price = best->first;
            if (limit && ((side == Side::Buy && price > *limit) ||
                          (side == Side::Sell && price < *limit))) break;
            auto& queue = best->second;
            auto& maker = queue.front();
            const Quantity quantity = std::min(remaining, maker.remaining);
            result.trades.push_back({maker.id, taker, price, quantity});
            remaining -= quantity;
            maker.remaining -= quantity;
            if (!maker.remaining) {
                active_.erase(maker.id);
                queue.pop_front();
                if (queue.empty()) levels.erase(best);
            }
        }
    }

    template <typename Levels>
    void rest(Levels& levels, const Order& order) {
        auto& queue = levels[order.price];
        queue.push_back(order);
        active_.emplace(order.id, Location{order.side, order.price, std::prev(queue.end())});
    }

    template <typename Levels>
    static void erase(Levels& levels, const Location& location) {
        auto level = levels.find(location.price);
        level->second.erase(location.position);
        if (level->second.empty()) levels.erase(level);
    }

    template <typename Levels>
    static std::vector<Level> aggregate(const Levels& levels) {
        std::vector<Level> result;
        for (const auto& [price, queue] : levels) {
            Quantity total = 0;
            for (const auto& order : queue) {
                if (order.remaining > std::numeric_limits<Quantity>::max() - total) {
                    throw std::overflow_error("depth quantity exceeds int64 range");
                }
                total += order.remaining;
            }
            result.push_back({price, total, queue.size()});
        }
        return result;
    }

    template <typename Levels>
    void inspect(const Levels& levels, Side side, std::unordered_set<OrderId>& found) const {
        for (const auto& [price, queue] : levels) {
            if (queue.empty()) throw std::logic_error("empty price level");
            for (auto position = queue.begin(); position != queue.end(); ++position) {
                const auto& order = *position;
                const auto indexed = active_.find(order.id);
                if (order.price != price || order.side != side || order.remaining <= 0 ||
                    !found.insert(order.id).second || !seen_.count(order.id) ||
                    indexed == active_.end() || indexed->second.price != price ||
                    indexed->second.side != side || indexed->second.position != position) {
                    throw std::logic_error("order queue/index invariant failed");
                }
            }
        }
    }
};

} // namespace microbook
