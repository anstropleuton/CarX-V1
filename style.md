# CarX-V1 Coding Style

C++26 on ESP-IDF / FreeRTOS. Python 3 for build-time generators.

## Formatting

- 4 spaces. Attached braces. Namespace bodies and `case` labels are indented.
- About 120 columns. When wrapping a signature, put one parameter per line, with
  `) -> T` on its own line.
- Omit braces on single-statement bodies. Early returns stay on one line:
  `if (!x) return std::nullopt;`
- Trailing commas in multi-line initializer lists.
- Column-align consecutive declarations.

## Naming

- `snake_case` for everything: namespaces, types, functions, variables,
  constants, enumerators, template parameters.
- Private members and private methods end in `_` (`dir_`, `release_()`). Public
  data members have no suffix.
- One namespace per module, named after the file (`ledc.hpp` →
  `namespace ledc`).
- Put units in names: `_mv`, `_cm`, `_us`, `_ms`, `_hz`.
- Pin constants are short lowercase (`p26`, `svn_39`).
- Abbreviate only when abbreviation is understood and an alternative term isn't
  usable: `us` for microseconds is acceptable. `fd` for file descriptor can be
  renamed to `file` since conceptually it represents one.

## Declarations

- Trailing return types on every function: `auto f(int x) -> void`.
- East const: `float const x`, `config const &cfg`.
- Locals are `const` unless modified. By-value parameters are `const` in
  definitions, not in header declarations.
- Brace-initialize everything: `auto const index{...}`, `int raw{}`,
  `.unit_id{adc::unit}`.
- `F` suffix on float literals (`0.1F`). Digit separators on big numbers
  (`1'000'000`).
- Fixed-width `std::` integer types.
- `[[nodiscard]]` on factories, getters and measurements.
- Mark classes and structs `final`. `static_cast` only, no C-style casts.
- Use `constexpr` for constants and configs.

## Resource wrappers

- Private default constructor, plus a static `create(...)` returning
  `std::optional<T>`.
- Destructor calls an idempotent private `release_()`.
- Copy deleted. Move is `noexcept`, built on `std::exchange`, with a
  `this != &other` guard in move assignment.
- Take a `config` struct by `const &` per call instead of storing it.

## Errors

- Setup that must succeed: `ESP_ERROR_CHECK`. Runtime:
  `ESP_ERROR_CHECK_WITHOUT_ABORT`.
- Expected failure (no free slot, timeout): return `std::nullopt`, never a
  sentinel.
- Early-return guard clauses.

## Code

- Prefer `<ranges>` (`views::enumerate`, `views::zip`) with structured bindings
  over index loops.
- `std::scoped_lock lock{mutex};` for locking.
- Use `.at()` for bounds-checked access in application code.
- Prefer `inplace_vector` / `inplace_string` to heap containers.
- File-local functions and variables are in anonymous namespace.
- A `switch` over an `enum class` lists every case.

## Structure

- One `.hpp` / `.cpp` pair per module, `#pragma once`. Register each `.cpp` in
  `main/CMakeLists.txt`.
- Pins live in `pins.hpp`. `main.cpp` only wires hardware and runs tasks.
- Tasks are `[[noreturn]] auto name(void *) -> void` with a
  `while (true)` loop declared in anonymous namespace.
  Delay with `vTaskDelay(pdMS_TO_TICKS(n))`.
- ISR callbacks are `IRAM_ATTR` and talk to tasks through `std::atomic` and
  `xTaskNotifyFromISR`.
- Definitions are written outside the namespace with qualified names:
  `auto ledc::channel::set_duty(...) const -> void`.

## Includes

Own header first, then standard library, then project headers, then
ESP-IDF/FreeRTOS headers. Blank line between groups.

## Comments

Sparse. Explain why, units, or constraints, not what. Use `//`. Put caveats on
the header declaration (`// Callback runs in ISR context`). No commented-out
code.

## CMake / Python

- One `SRCS` and one `REQUIRES` entry per line.
- Generated headers go in `${CMAKE_BINARY_DIR}/generated/`, produced by
  `add_custom_command` with parameters passed on the command line.
- Python: type hints, `argparse`, small pure functions, `main()` behind
  `__main__`. Generated headers start with
  `// AUTO-GENERATED FILE. DO NOT EDIT.`
