#include "microbook/book.hpp"

#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
template <typename Integer>
Integer integer(const std::string& token) {
    Integer value{};
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
        throw std::invalid_argument("invalid integer: " + token);
    }
    return value;
}

microbook::Side side(const std::string& token) {
    if (token == "B") return microbook::Side::Buy;
    if (token == "S") return microbook::Side::Sell;
    throw std::invalid_argument("side must be B or S");
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string token;
    while (std::getline(input, token, ',')) result.push_back(token);
    if (!line.empty() && line.back() == ',') result.emplace_back();
    return result;
}

void replay(std::istream& input) {
    microbook::Book book;
    std::string line;
    std::size_t number = 0;
    std::cout << "event,type,maker,taker,price_ticks,quantity,detail\n";
    while (std::getline(input, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        try {
            const auto row = fields(line);
            if (row.size() == 2 && row[0] == "CANCEL") {
                const auto id = integer<microbook::OrderId>(row[1]);
                const bool cancelled = book.cancel(id);
                std::cout << number << ",CANCEL,," << id << ",,,"
                          << (cancelled ? "cancelled" : "not_found") << '\n';
            } else {
                if (row.size() != 5 || (row[0] != "LIMIT" && row[0] != "MARKET")) {
                    throw std::invalid_argument("expected LIMIT,id,side,price,qty; MARKET,id,side,,qty; or CANCEL,id");
                }
                const auto id = integer<microbook::OrderId>(row[1]);
                const auto direction = side(row[2]);
                const auto quantity = integer<microbook::Quantity>(row[4]);
                microbook::Submission result;
                if (row[0] == "LIMIT") {
                    result = book.limit(id, direction, integer<microbook::Price>(row[3]), quantity);
                } else {
                    if (!row[3].empty()) throw std::invalid_argument("market price field must be empty");
                    result = book.market(id, direction, quantity);
                }
                for (const auto& trade : result.trades) {
                    std::cout << number << ",TRADE," << trade.maker << ',' << trade.taker
                              << ',' << trade.price << ',' << trade.quantity << ",\n";
                }
                std::cout << number << ",ACK,," << id << ",," << result.filled
                          << ",resting=" << result.resting << " unfilled=" << result.unfilled << '\n';
            }
            book.check_invariants();
        } catch (const std::exception& error) {
            throw std::runtime_error("line " + std::to_string(number) + ": " + error.what());
        }
    }
    if (input.bad()) throw std::runtime_error("input stream read failed");
    for (const auto direction : {microbook::Side::Buy, microbook::Side::Sell}) {
        for (const auto& level : book.depth(direction)) {
            std::cout << number << ",DEPTH,,," << level.price << ',' << level.quantity << ','
                      << (direction == microbook::Side::Buy ? "B" : "S")
                      << " orders=" << level.orders << '\n';
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: microbook-replay EVENTS.csv (use - for stdin)\n";
        return 2;
    }
    try {
        if (std::string(argv[1]) == "-") replay(std::cin);
        else {
            std::ifstream input(argv[1]);
            if (!input) throw std::runtime_error("cannot open input file");
            replay(input);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
