# wasm3 v0.5.0

Source: https://github.com/wasm3/wasm3/tree/v0.5.0
License: MIT, retained in LICENSE. Original authors' notices remain intact.
Only the twelve runtime C translation units listed in project.yml are built;
the upstream WASI, libc API adapters and tracer are not linked.

ESPatcher local changes:

- m3_env.h: per-runtime `gsFuel` counter.
- m3_exec_defs.h: decrement fuel on direct next/jump dispatch; trap at zero.
- m3_env.c: initialization expressions receive a temporary memory header and
  a 10,000-dispatch budget (upstream passed a null header).
- gs_sandbox.h/.c: small C functions to set memory/fuel limits without exposing
  configuration-sensitive wasm3 internal layouts to the Apple C++ translation unit.

The interpreter runs with optimization and DEBUG undefined in every configuration.
The host validates the file and compiles all functions before publishing a module.
This is an execution-operation bound, not a wall-clock deadline or a security audit.
