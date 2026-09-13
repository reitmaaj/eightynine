# 0001-stakeholders

- **WID toolchain author** — I want a strict-C89 JSON parser that is
  `green`-clean under the sibling `green` toolchain, so that the `wid`
  project can parse WID documents without failing its own quality gates.
- **WID maintainer** — I want a small, auditable JSON subset rather than a
  vendored jq-derived library, so that every line is inspectable and the
  whole dependency graph is gate-clean.
- **libj89 developer** — I want a self-contained project with its own test
  suite and gates, so that correctness is verified independently before
  `wid` links against it.
