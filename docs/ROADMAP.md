# Deliverables after v0.1

Keep one maintained project. Finish and explain each layer before expanding it.

| Milestone | Deliverable | Evidence needed |
| --- | --- | --- |
| v0.1, implemented locally | Matching core, replay CLI and synthetic fixture | Priority, fill, cancellation and invalid-input tests. Differential checks and sanitizer runs. |
| v0.2 | Per-event book snapshots and a replay viewer | A viewer driven by actual matcher output. A user can step through the example and see each queue change. Schema tests preserve compatibility. |
| v0.3 | Participant accounts and configurable self-trade prevention | Owner IDs, balance rules and explicit short-selling policy. Cash and inventory conservation tests, including rejected orders and fees. |
| v0.4 | A small controlled agent experiment | A documented question, fixed configurations, synthetic event provenance, multiple seeds and a simple baseline. Report inventory, fills and costs with uncertainty, not only a favorable return. |
| Later | Performance work and Python integration | Reproducible benchmarks with compiler, hardware, workload and diagnostic settings. Optimize only after profiling. Add bindings if a real experiment needs them. |

The best next feature is **per-event snapshots and a replay viewer**. It makes the
working engine inspectable while keeping the matching rules central. Recording
snapshots also creates a testable interface for later experiments.

Before the first public release, review the API and license, explain the queue
and cancellation code in your own words, and run the CI draft on a clean Linux
runner. Publish one coherent initial implementation. Follow it with real fixes
and features when they are ready, rather than splitting the initial files into
many activity commits.

Suggested initial commit:

```text
feat: add deterministic matching engine and tested replay CLI

Implement price-time priority, partial fills, cancellation and market IOC.
Compare the engine against a flat-vector reference on 10000 generated events.
Add reproducible synthetic examples, build checks and model limitations.

Co-authored-by: Codex <noreply@openai.com>
```
