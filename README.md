# Market Microstructure Lab

A small C++17 limit-order-book simulator for learning how market rules become
software. It matches orders, records trades and replays synthetic event streams
deterministically. The matching core uses only the C++ standard library.

**Status: v0.1 prototype.** Matching and replay work, with tests and a
synthetic example. Participant accounts, strategy agents and experiments on
market quality are planned extensions. This is a simplified model with explicit
rules, not a reconstruction of a particular exchange.

## What it does

- Matches the best price first, then arrival order within the same price.
- Supports limit orders, immediate-or-cancel market orders and cancellation.
- Preserves a resting order's queue position after a partial fill.
- Executes trades at the resting order's price.
- Reports fills, resting quantity, unfilled market quantity and final depth.
- Rejects invalid values and reused order IDs with actionable errors.
- Uses integer price ticks and quantities, avoiding floating-point price comparisons.

## Build and run

Requirements: a C++17 compiler, Make and Python 3.9+ for the CLI tests. No Python
packages, market-data subscriptions, API keys, CMake or network services are
needed for the build and tests. The initial local check used Apple Clang 21 on
macOS with Python 3.14.6. [Linux CI](https://github.com/yugeshsarikonda/market-microstructure-lab/actions)
also passed with GCC and Clang, including ordinary and sanitizer checks.

```sh
make
./build/microbook-replay examples/orders.csv
make test
make sanitize
```

The executable also accepts standard input:

```sh
./build/microbook-replay - < examples/orders.csv
```

Choose a compiler explicitly when needed:

```sh
make CXX=clang++
```

The C++ API is header-only. Include `microbook/book.hpp` and add `-Iinclude` to
your compiler flags. [API examples and model rules](docs/DESIGN.md) explain how
to embed it in a simulator.

## Example result

`examples/orders.csv` is a handwritten synthetic fixture. Its ten submitted
orders and one cancellation produce these trades:

| Resting order | Incoming order | Price in ticks | Quantity |
| --- | --- | --- | --- |
| 1 | 5 | 101 | 10 |
| 2 | 5 | 101 | 2 |
| 3 | 6 | 103 | 8 |
| 7 | 8 | 100 | 4 |
| 4 | 8 | 99 | 10 |
| 4 | 9 | 99 | 2 |

There are 36 traded units. Market orders 6 and 9 leave 2 and 6 units unfilled,
respectively. Order 10 remains as an ask for 6 units at 105 ticks. There are no
remaining bids. The actual replay output is saved in
[`examples/expected.csv`](examples/expected.csv), and the CLI test checks it.

## Event format

```text
LIMIT,1,S,101,10
MARKET,2,B,,4
CANCEL,1
```

Sides are `B` and `S`. Market events require an empty price field. New order IDs are
nonzero unsigned 64-bit integers. Limit prices and quantities are positive signed
64-bit integers. IDs remain reserved for the whole session after acceptance,
including after cancellation, full execution or an unfilled market order.

The input is a strict, unquoted comma-delimited format. Spaces, quoted fields,
decimal prices and headers are not accepted. Blank lines and lines beginning
with `#` are ignored. Both LF and CRLF are supported. Replay output is CSV with
`ACK`, `TRADE`, `CANCEL` and final `DEPTH` records. The `event` field is the input
line number, including ignored lines.

On invalid input the CLI returns a nonzero status and names the line. Earlier
valid events may already have been written to stdout. Treat output from a failed
run as an incomplete prefix, not a completed simulation. Invoking the program
without a filename returns usage status 2.

## Tests and reproducibility

`make test` runs 13 named C++ test cases and 7 Python black-box CLI tests.
One C++ case runs 1,000 generated events for each of ten fixed seeds. After every
event it compares trades and final book state with an independently implemented
flat-vector matching model and checks quantity conservation:

```text
submitted units = 2 * executed trade units
                + cancelled resting units
                + unfilled market units
                + currently resting units
```

The factor of two counts both sides of each trade. This is an order-quantity
identity. It is not a claim about conservation of participant cash or inventory,
which are not modeled yet. `make sanitize` repeats the suite with AddressSanitizer
and UndefinedBehaviorSanitizer. See [validation scope](docs/VALIDATION.md).

## Design and limitations

Ordered price maps hold FIFO lists. An order-ID index stores iterators for
cancellation. This makes queue behavior visible without a large framework.
The book has one instrument and one sequential event stream. There are no owner
IDs, balances, self-trade prevention, fees, latency, auctions, order amendments,
hidden orders or persistence across processes. Memory used by the accepted-ID
history grows with session length. Depth aggregation throws on signed-integer
overflow rather than wrapping.

There is no measured latency benchmark, profitability result or claim of
production readiness. [The roadmap](docs/ROADMAP.md) defines the next useful
deliverables and the evidence each would need.

## Development and license

The initial implementation, tests and documentation were prepared with Codex
assistance. Human review and further development should be recorded in focused
commits. [The walkthrough](docs/WALKTHROUGH.md) gives concrete exercises for
understanding and extending the core.

MIT licensed. All included events are synthetic. No code or dataset from the
reference student's repositories was incorporated.
