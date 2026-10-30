# Project Development Instructions

## Before you start

Read these first:

- [style.md](style.md) for style guidelines.
- [readme.html](readme.html) for project information.
- [utilities.hpp](main/utilities.hpp) for utility classes you will very likely
  need.

## Module structure

- A module consists of a header file and an optional source file of the same
  name.
- The header contains a namespace of the same name.

## Configuration and runtime

- Configuration and runtime data are modeled separately.
- Configuration is stored in a `config` struct.
- Runtime information is stored in a class, typically named the same as the
  namespace.
- If only one instance can exist (for example, the Wi-Fi module), there is no
  class.
- Don't mix configuration and runtime boundaries.

## Module scope

- Keep each module to one conceptual hardware device or one conceptual software
  responsibility. Don't tie it to the exact hardware representation.
    - Motor code controls one motor, even if two physical motors are driven from
      one pin.
    - Likewise, there are two conceptual motors in software even though there
      are four in hardware.
- Modules are partially generic. They shouldn't assume exact use cases, but may
  assume details that aren't useful to vary. This keeps scope tight and avoids
  building useless abstractions over everything.
- `main.cpp` and other files show the exact use of the modules.

## Resource allocation

Applies to allocatable resources such as LEDC timers and channels.

- Acquire resources through the config rather than allocating them internally.
  For example, motor code takes its timers from the config, so users must
  allocate them for the motor code to use.
- Hide which timer or channel number is used. The number is only an identity,
  like a memory allocation, and means little beyond that.
    - This adds slight runtime overhead (treating a fixed resource as
      dynamically allocated), so weigh it before deciding.
    - It's preferable that users don't have to request a specific resource, but
      that is only a preference.

## Exposing internals

- Expose `constexpr` constants when it is useful to know them.
- Prefer exposing a constant over documenting internals in a comment. For
  example, `ledc::gamma_duty_res` gives the timer bits needed for
  `ledc::channel::set_duty_gamma`, instead of a comment that merely links the
  two.
- Anonymous-namespace local functions are internal helpers and cannot be used externally.
