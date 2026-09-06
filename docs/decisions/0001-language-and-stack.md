# 1. C++23 with CMake and SDL3

Status: accepted

## Context

The brief mandates modern C++23 with CMake 3.28+ and SDL3, and allows a Rust
fallback only if a viable C++23 toolchain cannot be obtained after reasonable
attempts, or if an existing healthy repository is already substantially written
in Rust.

## Decision

C++23 with CMake and SDL3, as specified. The repository was empty, so there was
no existing code to preserve either way.

Toolchain verified before any code was written: GCC 13.3 with `-std=c++23`,
`<format>` and `<expected>` both available (`<print>` is not, and is not used);
CMake 3.28.3; Ninja 1.11.1. SDL3 is not packaged for Ubuntu 24.04, so it is
built from the pinned `release-3.4.16` tag, which succeeded once the X11
development headers were installed.

The Rust fallback is not triggered: nothing about the C++ path was blocked.

## Consequences

- `-std=c++23` with no compiler extensions; `std::expected` is the error type
  throughout and `std::format` is the logging and UI formatting mechanism.
- SDL3 is built from source on first configure, which costs a few minutes.
  `-DDEEPFORGE_USE_SYSTEM_SDL=ON` skips that for anyone who already has it.
- GCC 13 lacks `<print>`, so `std::format` plus `fputs` is used in the logger.
