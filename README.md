# Fundatio

**Fundatio** is a minimalistic set of C99 libraries for application development, aiming to provide stable, cross-platform foundation in following areas:
- Graphics — Fundatio provides an RHI that enables performant cross-platform rendering.
- Threading - Cross platform threading library included
- Input - Allows reading input from mouse, keyboard and multiple gamepads
- And much more in less-emphasized areas.

# Design Points

- Single-header libraries
- Standardized, minimalistic API
- Libraries are modular — users can build only the libraries they need along with their dependencies.

# Contents

| Library | Domain | Usage |
| ------- | ------ | ----- |
| Linear Algebra.h | Mathematics | Linear algebra library |
| Camera.h | Mathematics | Camera projection and movement |
| Partitioner.h | Algorithm | Efficient TLSF memory suballocator |
| Segmenter.h | Algorithm |  Library for segmenting uploads over bandwidth-constrained connections |
| Graphics.h | Platform | A graphics RHI |
| Input.h | Platform | HID device interface |
| Threads.h | Platform | Cross platform multithreading api |
| Filesystem.h | Platform | Cross platform virtual filesystem, with directory watching |

# Repository Structure

- *include* - This is where the libraries live.
- *depdency* - Libraries implementations depedencies

# Platform Support

Fundatio libraries in include/platform have platform-specific backends. Their current support status is listed below.

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

| Platform | Support |
| -------- | :----:  |
| Windows  | Yes     |
| Linux    | Yes     |

### Filesystem.h

| Platform | Support |
| -------- | :-----: |
| Windows  | No      |
| Linux    | Yes     |
