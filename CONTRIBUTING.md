# Contributing to LightFramework

Thank you for your interest in contributing to LightFramework.

## Philosophy

LightFramework is designed to stay lightweight and modular, providing a small foundation that developers can easily expand by composing existing elements.

The goal is to keep the core library small, maintainable, and focused. Because of this, only features considered essential to the framework's core vision will be merged.

## What Will Be Merged

- Bug fixes
- Performance improvements
- Stability improvements
- Small and reusable core features
- Documentation improvements

## What May Be Rejected

- Large feature additions
- Highly specialized functionality
- Features that significantly increase complexity
- Features that conflict with the lightweight philosophy

## Pull Requests

Before opening a pull request:
- Keep changes focused and minimal
- Follow the existing code style
- Document important behavior changes
- Test your changes when possible

## Issues

When reporting issues, please include:
- Steps to reproduce
- Expected behavior
- Actual behavior
- Relevant logs or screenshots

# Future Goals

## Existing content

The existing content shall be maintained and keep clean.
Therefore state of code table:

| Library File  | State |
| ------------- | ------------- |
| graphics.h                    | Stable, extra validations may be added, can be refactored in some areas |
| input.h                       | Stable. Internal refactoring would still be nice. |
| linear_algebra.h              | Claude-d, needs to be tested and checked |
| user_interface.h              | Correct |
| synchronised_window.h         | Correct |
| font.h                        | Correct |
| shapes_rendering.h            | Correct |
| user_interface_rendering.h    | Functional, but open for refactoring and performance optimizations. |

## Upcoming content

The following features may be implemented in the future:

- UI prefabs                            (user_inteface_prefabs.h)
- Async networking                      (networking.h)
- Filesystem utilities                  (filesystem.h)
- File compression and decompression    (compression.h)
- File encryption on compression        (compression.h)
- Serialization and deserialization     (serialization.h)
