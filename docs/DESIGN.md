# Model rules and implementation

## An order's lifecycle

`Book::limit(id, side, price, quantity)` validates the request, matches against
compatible opposite-side orders, and queues the remaining quantity at its limit
price. `Book::market(id, side, quantity)` follows the same matching path but
cancels any remainder. `Book::cancel(id)` removes a live order and returns true.
Cancelling an unknown or inactive ID returns false.

```cpp
#include "microbook/book.hpp"

microbook::Book book;
book.limit(1, microbook::Side::Sell, 101, 10);
const auto result = book.limit(2, microbook::Side::Buy, 102, 4);
// result.filled == 4, trade price == 101, order 1 has 6 units remaining.
```

For every accepted submission, `filled + resting + unfilled` equals the incoming
quantity. Limit remainders rest until filled or cancelled. Market remainders
never rest. A trade reports resting maker ID, incoming taker ID, execution price
and quantity. Equal-price crossing is allowed. Buying stops when the lowest ask
exceeds the limit. Selling stops when the highest bid is below the limit.

IDs identify orders, not participants. A maker and a taker could conceptually
belong to the same participant, since ownership is outside this version's model.

## Priority and price

Price priority comes before time priority. At an identical price, the first
accepted resting order fills first. Its ID has no role in deciding priority.
Partial execution leaves it in its original queue position. Trades execute at
the maker's price, even if the taker supplied a more aggressive limit.

Input line order is the only time ordering. There is no physical clock or
timestamp-based reordering. Replaying the same accepted input stream starts
from an empty book and produces the same trades and final depth.

## Data structures

| Structure | Purpose |
| --- | --- |
| Descending `std::map` of bids | Makes the highest buy price the first level |
| Ascending `std::map` of asks | Makes the lowest sell price the first level |
| `std::list<Order>` at each level | Maintains FIFO order and stable node iterators |
| `std::unordered_map` from ID to location | Locates a live order for queries or cancellation |
| `std::unordered_set` of accepted IDs | Prevents ID reuse throughout a session |

For L price levels, adding a resting order or locating its price level takes
O(log L). ID lookup is average O(1), and removing a known list node is O(1).
Cancellation still needs O(log L) to locate the associated map level. A market
order's work grows with the number of orders it touches. These are structural
complexities, not measured throughput results. Hash tables have worst-case
behavior different from their average behavior.

`depth` scans all live orders and aggregates each level. It rejects totals that
cannot fit in signed 64-bit quantity. `check_invariants` is an intentionally
expensive diagnostic. Replay calls it after every input event, so CLI timings
would include diagnostic overhead.

## Index and memory safety

The index points into the resting lists. Filling or cancelling an order removes
its index entry as well as its list node. Empty levels are removed. Copying and
moving a Book are disabled, avoiding accidental copies of iterators into
another instance. Construct a fresh Book to replay a session.

Normal request validation occurs before ID acceptance or matching, and invalid
requests do not consume an ID. The implementation does not promise transactional
recovery from allocation failure. It has no crash-recovery log or concurrency
control. These would require explicit designs before a production use case.

## Output fields

| Field | Meaning |
| --- | --- |
| `event` | Input line number, including comments and blanks |
| `type` | `ACK`, `TRADE`, `CANCEL` or final `DEPTH` |
| `maker` | Resting order ID for trades |
| `taker` | Incoming order ID, or the cancelled ID for cancellation records |
| `price_ticks` | Execution or depth price when applicable |
| `quantity` | Trade quantity, filled quantity for ACK, or total depth quantity |
| `detail` | ACK remainders, cancellation status, or depth side and order count |

Empty fields have no applicable value. `CANCEL` records do not report the removed
quantity. Cash, wealth and inventory metrics cannot be reconstructed from this
format without participant ownership and additional accounting rules.
