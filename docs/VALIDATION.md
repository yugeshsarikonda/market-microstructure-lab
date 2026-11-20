# Observed validation

Checked November 20, 2026. This records local results for the v0.1 prototype.

| Check | Observed result |
| --- | --- |
| `make test` | 13 C++ cases and 7 Python CLI tests passed |
| Compiler warnings | Built with C++17, `-Wall -Wextra -Wpedantic -Werror` |
| Differential test | 10 seeds, 1,000 events each, compared with a flat-vector reference |
| Per-event diagnostics | Queue/index consistency, uncrossed resting book, final depth and quantity conservation passed |
| `make sanitize` | All tests passed with AddressSanitizer and UndefinedBehaviorSanitizer, with no diagnostic reports |
| Example replay | Six trades totaling 36 units, final ask at 105 for 6 units |
| Golden output | Actual executable output saved in `examples/expected.csv` and checked by a CLI test |
| Repeatability | Replaying the same fixture produced byte-identical CSV output |

Environment: Apple Clang 21.0.0, arm64 macOS, Make 3.81 and Python 3.14.6.
No third-party library or Python package was installed for this project.

The reference model shares the small value structs with the engine but scans a
flat arrival-ordered vector for each fill. It does not reuse the engine's price
queues, index or matching method. Hand-checked tests separately cover priority,
execution price, partial fills, cancellation, remainders, reused IDs, invalid
requests and signed-integer depth overflow. Differential agreement is evidence
within these workloads, not a proof that every possible input is correct.

The CLI tests cover expected trades and depth, exact fixture output, repeated
replay, malformed inputs, overflow during parsing, duplicate IDs, CRLF, unknown
cancellations, file/stdin equivalence and missing-file/usage errors.

The CI draft builds with GCC and Clang on Linux and runs ordinary and sanitizer
checks. It has not executed on GitHub, since no remote repository was created or
pushed for this prototype. No throughput, latency, live-data, trading-performance
or user-impact result is claimed.
