# 10. The store is its own layer, above the simulation closure

Status: accepted

## Context

The monetisation spec states the non-P2W invariant in two halves that
contradict each other literally. The structural half says:

> `grep -r "store_catalog\|entitlement" src/domain src/world src/content
> src/core` → zero hits

The other half — the project structure section — says `data/cosmetics.json` and
`data/store_catalog.json` are "new content, loaded by `ContentLoader`", and
`ContentLoader` lives in `src/content`. If the content loader parses the store
catalog, the grep over `src/content` cannot return zero hits. Both passages
cannot be literally true.

## Decision

The grep is literal and binding: it runs over all four directories in CI, and
the store data is therefore **not** loaded by `ContentLoader`. The store gets
its own layer, `src/store` (`StoreCatalog.hpp/.cpp`), which parses both files
and is linked only by tools and tests. The dependency graph stays
`core <- content <- world <- domain <- app`; `deepforge_store` hangs off `core`
alone and nothing below the closure links it. The spec's sample accessor
(`db.store_catalog()` on `ContentDatabase`) is the loose part of the two
passages and was not implemented.

The behavioural half is unchanged: the enumerating test asserts every shipped
entitlement is `kind == cosmetic` with zero sim-visible effects, and the kind
enum already expresses expansion/convenience so a future pack needs no schema
migration.

## Consequences

- The literal CI grep passes as written, with no exemptions to remember.
- `StoreCatalog` is a second ingest path next to `ContentDatabase`. That is the
  cost of taking the closure literally, and it is a small one: two files, one
  parse each, no shared state.
- The strings test counts store keys as referenced so the single string table
  still covers them (ADR 7 stands: a missing key is a build failure).
- If a future expansion pack needs store data inside the simulation, this
  record is the thing that gets revisited — which is the point of writing it
  down.
