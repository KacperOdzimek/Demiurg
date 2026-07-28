# Demiurg

**Demiurg** is a minimalistic set of C99 libraries for application development.

Framework areas of **interest** are:
- **Graphics** — Demiurg provides an RHI that enables performant cross-platform rendering.
- **User Interface** — Demiurg provides a flexible, data-driven UI system suitable for both simple GUI applications and visually rich game UIs.
- **Fast prototyping** — Demiurg requires little boilerplate (especially for a C application) and includes useful prefabs such as an intermediate shape rendering system.
- And much more in less-emphasized areas.

# Design Points

- Single-header libraries
- Standardized, minimalistic API
- Libraries are modular — users can build only the libraries they need along with their dependencies.

# Contents

| Library | Domain | Usage |
| ------- | ------ | ----- |
| Linear Algebra.h | Mathematics | Linear algebra library |
| Graphics.h | Platform | A graphics RHI |
| Input.h | Platform | HID device interface |
| Threads.h | Platform | Cross platform multithreading api |
| Partitioner.h | Algorithm | Efficient TLSF memory suballocator |
| Segmenter.h | Algorithm |  Library for segmenting uploads over bandwidth-constrained connections |
| Serialization.h | Algorithm | Safe cross-platform serialization |
| Font.h | Resource | Font loader and system |
| User Interface.h | User Interface | A flexible user interface |

# Repository Structure

- *include* - This is where the libraries live.
- *shaders* - GPU shaders required by the libraries.
- *utility* - Utility scripts, file format converters, and other development tools. This directory contains development-only code and is not intended for shipping.
- *examples* - Code examples

# Platform Support

Most Demiurg libraries are platform-independent. They either rely on Demiurg's platform abstraction interfaces or do not require any platform-specific code.

Only a few libraries in include/platform have platform-specific backends. Their current support status is listed below.

> **Note:** Platform support is actively expanding. Additional operating systems and graphics backends will be implemented over time.

### Graphics.h

| Platform | Vulkan |
| -------- | :----: |
| Windows  | Yes    |
| Linux    | Yes    |

### Input.h

| Platform | Support |
| -------- | :-----: |
| Windows  | No      |
| Linux    | Yes     |

### Threads.h

| Platform | Vulkan |
| -------- | :----: |
| Windows  | Yes    |
| Linux    | Yes    |
