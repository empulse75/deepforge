# 11. What is actually pinned, and where it is named

Status: accepted (supersedes record 2's version list; the dependency set decided there stands)

## Context

Record 2 fixed *which* dependencies the project uses. What it could not do is
keep naming them: the pins in effect have since moved, and the record above
still says nlohmann/json 3.11.3, SDL3 `release-3.4.16` and Catch2 v3.7.1 while
`CMakeLists.txt` fetched json 3.12.0, SDL3 `release-3.4.18` and Catch2
`v3.16.0`. Records are never edited, so this one says what is in effect.

The audit that prompted it (SEC-08, corroborated by OPS-DEPS-02 and the
code-quality audit's DEPS-01/DEPS-02) measured the trust problem: a git tag is a
pointer someone else can move, so only the json dependency — pinned by SHA-256
of the downloaded archive, hash verified against the upstream download — was
actually pinned. The other two were fetched as mutable tags, shallow-cloned. CI
had the same shape: every action floated on a mutable major tag, and no
dependency-audit job existed anywhere in the pipeline.

## Decision

- Both git dependencies are named by commit SHA, with the tag they correspond to
  written next to them: Catch2 `317ac1ed4c0bb6e6b91eafc817e05c488feffcb3`
  (= `v3.16.0`), SDL3 `829a65d769d935c4852f8159e964312c0957260a`
  (= `release-3.4.18`). json keeps its `URL` + `URL_HASH`.
- Third-party CI actions are named by commit SHA: checkout
  `11d5960a326750d5838078e36cf38b85af677262` (= v4.4.0), cache
  `0057852bfaa89a56745cba8c7296529d2fc39830` (= v4.3.0), upload-artifact
  `ea165f8d65b6e75b540449e92b4886f43607fa02` (= v4.6.2),
  ilammy/msvc-dev-cmd `0b201ec74fa43914dc39ae48a89fd1d8cb592756` (= v1.13.0).
- The format gate pins the formatter itself (clang-format 21.1.8 in CI, the same
  version in `.pre-commit-config.yaml`), because a gate whose tool version floats
  checks someone else's style.

Measured, with numbers: each SHA was resolved from the tag with `git ls-remote`
and then fetched into a clean directory — both check out at exactly the named
commit, `GIT_SHALLOW` included. The formatter was run over the whole tree at
three versions (18.1.8, 19.1.7, 21.1.8) and produced the identical result, so the
pin buys determinism for the future rather than settling a disagreement today.

## Consequences

- A fresh clone resolves to the same three upstream trees whatever the servers do
  to those tags; `git ls-remote` against the old tag names is the standing check.
- A version bump is now an explicit edit to a SHA, and a bump that changes the
  comment but not the SHA is a bug.
- Every pin edit pays one cold dependency fetch in CI, because the cache key is
  `hashFiles('CMakeLists.txt')`.
- Cost named honestly: the audit job is *not* here. The project has no lockfile,
  so an OSV scan over `CMakeLists.txt` has nothing to lock on; a job that prints
  "no packages" for every dependency is coverage evidence, not proof of absence.
  The security audit queried OSV for all three dependencies by hand — 0 hits —
  and the job belongs in its own backlog item once there is something to scan.
