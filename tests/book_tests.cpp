#include "microbook/book.hpp"

#include <iostream>
#include <random>
#include <string>
#include <type_traits>

using namespace microbook;

namespace {
void check(bool value, const char* expression, int line) {
    if (!value) throw std::runtime_error(std::string(expression) + " at line " + std::to_string(line));
}
#define CHECK(expression) check((expression), #expression, __LINE__)

template <typename Error, typename Function>
void throws(Function function) {
    try { function(); }
    catch (const Error&) { return; }
    throw std::runtime_error("expected exception was not thrown");
}

// Independent slow model: scan a flat arrival-ordered vector for each fill.
// It shares value types with the engine, but no queues, indexes or matching code.
class Reference {
public:
    std::vector<Order> orders;

    Submission submit(OrderId id, Side side, std::optional<Price> price, Quantity quantity) {
        Submission result{id, 0, 0, 0, {}};
        Quantity remaining = quantity;
        while (remaining) {
            std::size_t best = orders.size();
            for (std::size_t i = 0; i < orders.size(); ++i) {
                const auto& order = orders[i];
                if (order.side == side) continue;
                if (price && ((side == Side::Buy && order.price > *price) ||
                              (side == Side::Sell && order.price < *price))) continue;
                if (best == orders.size() ||
                    (side == Side::Buy && order.price < orders[best].price) ||
                    (side == Side::Sell && order.price > orders[best].price)) best = i;
            }
            if (best == orders.size()) break;
            auto& maker = orders[best];
            const auto filled = std::min(remaining, maker.remaining);
            result.trades.push_back({maker.id, id, maker.price, filled});
            remaining -= filled;
            maker.remaining -= filled;
            if (!maker.remaining) orders.erase(orders.begin() + static_cast<std::ptrdiff_t>(best));
        }
        result.filled = quantity - remaining;
        if (price && remaining) {
            orders.push_back({id, side, *price, remaining});
            result.resting = remaining;
        } else result.unfilled = remaining;
        return result;
    }

    bool cancel(OrderId id) {
        for (auto it = orders.begin(); it != orders.end(); ++it) {
            if (it->id == id) { orders.erase(it); return true; }
        }
        return false;
    }

    std::vector<Level> depth(Side side) const {
        std::map<Price, Level> levels;
        for (const auto& order : orders) {
            if (order.side != side) continue;
            auto& level = levels[order.price];
            level.price = order.price;
            level.quantity += order.remaining;
            ++level.orders;
        }
        std::vector<Level> result;
        for (const auto& [price, level] : levels) {
            (void)price;
            result.push_back(level);
        }
        if (side == Side::Buy) std::reverse(result.begin(), result.end());
        return result;
    }
};

void compare(const Submission& a, const Submission& b) {
    CHECK(a.id == b.id);
    CHECK(a.filled == b.filled);
    CHECK(a.resting == b.resting);
    CHECK(a.unfilled == b.unfilled);
    CHECK(a.trades == b.trades);
}

void price_priority_buy() {
    Book book;
    book.limit(1, Side::Sell, 105, 5);
    book.limit(2, Side::Sell, 101, 4);
    const auto result = book.limit(3, Side::Buy, 106, 6);
    CHECK((result.trades == std::vector<Trade>{{2, 3, 101, 4}, {1, 3, 105, 2}}));
    CHECK(book.find(1)->remaining == 3);
    CHECK(!book.find(2));
    book.check_invariants();
}

void price_priority_sell() {
    Book book;
    book.limit(1, Side::Buy, 95, 5);
    book.limit(2, Side::Buy, 99, 4);
    const auto result = book.limit(3, Side::Sell, 94, 6);
    CHECK((result.trades == std::vector<Trade>{{2, 3, 99, 4}, {1, 3, 95, 2}}));
    book.check_invariants();
}

void fifo_is_arrival_order_not_id_order() {
    Book book;
    book.limit(90, Side::Sell, 100, 3);
    book.limit(2, Side::Sell, 100, 5);
    const auto result = book.market(7, Side::Buy, 4);
    CHECK((result.trades == std::vector<Trade>{{90, 7, 100, 3}, {2, 7, 100, 1}}));
}

void partial_fill_keeps_priority() {
    Book book;
    book.limit(1, Side::Buy, 100, 10);
    book.market(2, Side::Sell, 2);
    book.limit(3, Side::Buy, 100, 5);
    const auto result = book.market(4, Side::Sell, 9);
    CHECK((result.trades == std::vector<Trade>{{1, 4, 100, 8}, {3, 4, 100, 1}}));
}

void limit_residual_rests() {
    Book book;
    book.limit(1, Side::Sell, 101, 3);
    const auto result = book.limit(2, Side::Buy, 102, 8);
    CHECK(result.filled == 3 && result.resting == 5 && result.unfilled == 0);
    CHECK(book.best_bid() == 102 && !book.best_ask());
    CHECK(book.find(2)->remaining == 5);
}

void respects_limit_price() {
    Book book;
    book.limit(1, Side::Sell, 102, 3);
    const auto result = book.limit(2, Side::Buy, 101, 8);
    CHECK(result.trades.empty() && result.resting == 8);
    CHECK(book.best_bid() == 101 && book.best_ask() == 102);
    book.check_invariants();
}

void market_orders_cancel_unfilled_quantity() {
    Book book;
    const auto empty = book.market(1, Side::Buy, 9);
    CHECK(empty.unfilled == 9 && empty.resting == 0 && book.size() == 0);
    book.limit(2, Side::Sell, 102, 3);
    const auto partial = book.market(3, Side::Buy, 8);
    CHECK(partial.filled == 3 && partial.unfilled == 5 && partial.resting == 0);
    CHECK(book.size() == 0);
}

void cancel_middle_and_best_level() {
    Book book;
    for (OrderId id : {1, 2, 3}) book.limit(id, Side::Sell, 100, 2);
    book.limit(4, Side::Sell, 101, 3);
    CHECK(book.cancel(2));
    const auto result = book.market(5, Side::Buy, 4);
    CHECK((result.trades == std::vector<Trade>{{1, 5, 100, 2}, {3, 5, 100, 2}}));
    CHECK(book.best_ask() == 101);
    CHECK(book.cancel(4));
    CHECK(!book.best_ask() && !book.cancel(4) && !book.cancel(999));
    book.limit(6, Side::Buy, 90, 3);
    CHECK(book.cancel(6) && !book.best_bid());
    book.check_invariants();
}

void ids_are_unique_for_the_whole_session() {
    Book book;
    book.limit(1, Side::Sell, 100, 1);
    book.market(2, Side::Buy, 1);
    book.limit(3, Side::Buy, 90, 1);
    book.cancel(3);
    book.market(4, Side::Buy, 1);
    for (OrderId id : {1, 2, 3, 4}) {
        throws<std::invalid_argument>([&] { book.limit(id, Side::Buy, 1, 1); });
    }
    CHECK(book.size() == 0);
}

void invalid_orders_do_not_consume_ids_or_change_book() {
    Book book;
    book.limit(1, Side::Sell, 100, 4);
    throws<std::invalid_argument>([&] { book.limit(2, Side::Buy, 100, 0); });
    throws<std::invalid_argument>([&] { book.limit(2, Side::Buy, 0, 3); });
    throws<std::invalid_argument>([&] { book.limit(2, Side::Buy, -1, 3); });
    throws<std::invalid_argument>([&] { book.market(2, Side::Buy, -1); });
    throws<std::invalid_argument>([&] { book.market(0, Side::Buy, 1); });
    throws<std::invalid_argument>([&] { book.market(2, static_cast<Side>(99), 1); });
    throws<std::invalid_argument>([&] { book.depth(static_cast<Side>(99)); });
    CHECK(book.find(1)->remaining == 4);
    CHECK(book.market(2, Side::Buy, 2).filled == 2);
    CHECK(book.find(1)->remaining == 2);
    throws<std::invalid_argument>([&] { book.limit(1, Side::Buy, 200, 1); });
    CHECK(book.find(1)->remaining == 2);
    book.check_invariants();
}

void integer_boundaries_and_depth_overflow() {
    Book book;
    const auto maximum = std::numeric_limits<Quantity>::max();
    const auto price = std::numeric_limits<Price>::max();
    book.limit(1, Side::Sell, price, maximum);
    CHECK(book.market(2, Side::Buy, maximum).filled == maximum);
    book.limit(3, Side::Buy, 1, maximum);
    book.limit(4, Side::Buy, 1, 1);
    throws<std::overflow_error>([&] { book.depth(Side::Buy); });
    CHECK(book.cancel(4));
    CHECK(book.depth(Side::Buy).front().quantity == maximum);
    book.check_invariants();
}

void deterministic_replay() {
    auto run = [] {
        Book book;
        std::vector<Trade> trades;
        for (OrderId id = 1; id <= 50; ++id) {
            const auto result = book.limit(id, id % 2 ? Side::Buy : Side::Sell,
                                           static_cast<Price>(98 + id % 5), 7);
            trades.insert(trades.end(), result.trades.begin(), result.trades.end());
        }
        book.check_invariants();
        return trades;
    };
    CHECK(!run().empty());
    CHECK(run() == run());
}

void differential_randomized_and_conservation() {
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        Book book;
        Reference reference;
        std::mt19937 random(seed);
        OrderId next_id = 1;
        Quantity submitted = 0, executed = 0, cancelled = 0, unfilled = 0;
        for (int step = 0; step < 1000; ++step) {
            const auto action = random() % 5;
            if (action == 4 && next_id > 1) {
                const auto id = 1 + random() % (next_id - 1);
                const auto before = book.find(id);
                CHECK(book.cancel(id) == reference.cancel(id));
                if (before) cancelled += before->remaining;
                CHECK(!book.find(id));
            } else {
                const auto id = next_id++;
                const auto direction = action % 2 ? Side::Sell : Side::Buy;
                const auto quantity = static_cast<Quantity>(1 + random() % 40);
                const auto price = static_cast<Price>(95 + random() % 11);
                const bool market = action >= 2;
                const auto actual = market ? book.market(id, direction, quantity)
                                           : book.limit(id, direction, price, quantity);
                compare(actual, reference.submit(id, direction, market ? std::nullopt :
                                                  std::optional<Price>(price), quantity));
                CHECK(actual.filled + actual.resting + actual.unfilled == quantity);
                submitted += quantity;
                unfilled += actual.unfilled;
                for (const auto& trade : actual.trades) executed += trade.quantity;
            }
            book.check_invariants();
            CHECK(book.size() == reference.orders.size());
            CHECK(book.depth(Side::Buy) == reference.depth(Side::Buy));
            CHECK(book.depth(Side::Sell) == reference.depth(Side::Sell));
            Quantity resting = 0;
            for (const auto& order : reference.orders) {
                const auto indexed = book.find(order.id);
                CHECK(indexed && indexed->remaining == order.remaining);
                resting += order.remaining;
            }
            CHECK(submitted == 2 * executed + cancelled + unfilled + resting);
        }
    }
}
} // namespace

static_assert(!std::is_copy_constructible_v<Book>, "indexed iterators must not be copied");
static_assert(!std::is_move_constructible_v<Book>, "movement is not part of the v0.1 API");

int main() {
    const std::pair<const char*, void (*)()> tests[] = {
        {"price priority buy", price_priority_buy},
        {"price priority sell", price_priority_sell},
        {"FIFO by arrival", fifo_is_arrival_order_not_id_order},
        {"partial fill priority", partial_fill_keeps_priority},
        {"limit residual", limit_residual_rests},
        {"limit boundary", respects_limit_price},
        {"market IOC", market_orders_cancel_unfilled_quantity},
        {"cancellation", cancel_middle_and_best_level},
        {"session IDs", ids_are_unique_for_the_whole_session},
        {"invalid input atomicity", invalid_orders_do_not_consume_ids_or_change_book},
        {"integer boundaries", integer_boundaries_and_depth_overflow},
        {"deterministic replay", deterministic_replay},
        {"reference model and 10000-event conservation", differential_randomized_and_conservation},
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    return failures ? 1 : 0;
}
