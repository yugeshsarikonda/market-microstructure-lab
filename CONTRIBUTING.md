# Contributing

Start with a reproducible bug or a scoped feature from `docs/ROADMAP.md`. Check
existing work before opening a duplicate report or pull request.

Keep matching-rule changes separate from presentation changes. State the intended
rule, include a small example, and add a regression test. Update the model
documentation when behavior changes.

Run:

```sh
make test
make sanitize
git diff --check
```

Use descriptive, coherent commits. Describe AI assistance and what validation
was actually performed. Report untested platforms and incomplete experiments
explicitly. Do not commit private records, API keys or market datasets without
the rights to redistribute them. The provided fixture is entirely synthetic.
