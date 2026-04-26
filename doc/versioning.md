# Versioning Policy

The Vigine engine follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
starting from the `v1.0.0` release. This document explains how version numbers
are assigned before and after that milestone, what compatibility guarantees
each release tier offers, and how downstream consumers can request fixes for
older lines.

## Pre-1.0 releases

While the engine is in its `0.x` series, the SemVer compatibility contract
does **not** apply. In particular:

- A **minor** bump (for example, `0.2.0` to `0.3.0`) is allowed to carry
  breaking changes to any public header, type, or build option.
- A **patch** bump on a `0.x` line (for example, `0.2.0` to `0.2.1`) is
  reserved for bug fixes that preserve source and ABI compatibility with
  the matching minor tag.
- Release notes for every `0.X.0` tag must call out breaking changes
  explicitly, including migration notes where the change is not trivial.

The first release under the full SemVer contract is `v1.0.0`. From that
point onward the rules in the rest of this document are binding.

## Release branches (post-1.0)

After a MAJOR or MINOR tag lands on `main` (`vX.Y.0`), a long-lived
`release/X.Y.x` branch is created from that tag. This branch exists to
collect patch-level fixes for the `X.Y` line without forcing consumers
onto a newer minor.

| Branch          | Purpose                                          | Tags on it        |
|-----------------|--------------------------------------------------|-------------------|
| `main`          | Active development of the next MAJOR/MINOR       | `vX.(Y+1).0`, ... |
| `release/X.Y.x` | Backports and fixes for the `X.Y` line           | `vX.Y.1`, `vX.Y.2`, ... |

Patch releases are cut on the release branch and tagged as `vX.Y.Z`, with
`Z` increasing monotonically. The release branch never receives new
features or breaking changes; only bug fixes, security patches, and
buildability fixes are eligible.

## Experimental tier

Any subsystem living under the `vigine::experimental::*` namespace,
or gated behind the CMake option `VIGINE_ENABLE_EXPERIMENTAL=ON`, sits
**outside** the SemVer contract. Such APIs may change shape, be renamed,
or be removed entirely in any `0.x` **or** `1.x` release — including
patch releases — without triggering a MAJOR bump.

Consumers who opt into the experimental tier accept that:

- Header paths and symbols may be renamed between any two releases.
- ABI is not preserved across any boundary.
- Removal of an experimental API does not require a deprecation window.

Release notes should still mention experimental-tier changes so that
early adopters can track them, but the change does not itself constitute
a breaking change under SemVer for the engine as a whole.

## Deprecation policy (post-1.0)

Once the engine is on a `1.x` or newer line, removal of a public API
follows a structured lifecycle:

1. The API is annotated with `[[deprecated("use X")]]`, where `X` names
   the replacement API or migration path. The annotation lands in a
   MINOR release.
2. The deprecated API remains functional for **at least one full MINOR
   release cycle** after the annotation is added, and in no case less
   than **six months** of wall-clock time, whichever is longer.
3. Removal happens in a subsequent MAJOR or MINOR release and is called
   out in the release notes for that version.

This rule gives downstream consumers a predictable window to migrate and
ensures that no deprecation is silently removed in a patch release.
Deprecations on experimental-tier APIs are not bound by this window.

## Requesting a backport

If a bug fix has already landed on `main` and is needed on an older
release line, open a new issue in the engine repository and:

- Add the label `backport/vX.Y`, where `X.Y` is the release line you
  need the fix on (for example, `backport/v1.2`).
- Link to the merged fix PR on `main` in the issue body.
- State the user-visible impact of the bug on the target release line,
  which helps prioritise the backport against other work on that
  branch.

Maintainers will evaluate whether the fix is a clean cherry-pick or
requires a dedicated port. Once merged onto `release/X.Y.x`, the fix
will ship in the next patch release on that branch.
