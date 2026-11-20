# Understand the engine before presenting it

This prototype was prepared with AI assistance. The useful next step is to
understand its choices, review it and make extensions you can defend.

## Trace the example

1. Build the executable and replay `examples/orders.csv`.
2. Explain why order 1 fills before order 2 at the same price.
3. Explain why order 5 trades at 101 even though its limit is 102.
4. After cancelling order 2, identify how much ask liquidity remains.
5. Explain why market order 6 leaves 2 units unfilled instead of becoming a bid.
6. Follow sell order 8 through the bid queues at 100 and 99.
7. Verify the six trades, 36 executed units and final ask at 105.

## Review the implementation

Find where the map orders price levels and where the list preserves arrival
order. Explain what an index iterator points to after a partial fill, full fill
or cancellation. Explain why copying Book is disabled. Contrast integer price
ticks with rounding decimal prices in floating point.

Read the independent Reference in the tests. It scans an arrival-ordered vector
for the next eligible maker. Explain why retaining the first equal-price match
implements FIFO. Then explain what errors both implementations could still share,
which is why hand-checked cases are also included.

## Make a bounded improvement

Start with one of these tasks and keep the change focused:

- Add per-event snapshots without changing matching outcomes. Verify their
  order, quantities and schema against the synthetic example.
- Specify and implement cancel-and-replace. Decide when replacement loses queue
  priority, then test both sides and partial fills.
- Add one documented rejection policy. Define its effect on IDs and state before
  changing the code, then write a regression test that checks that behavior.

Keep a short development note with the question, a failing case, the change and
the command that verifies it. Record what assistance was used. This creates
evidence of decisions and understanding that a repository count cannot show.
