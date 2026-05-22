# Light Framework

Collection of C99 (single) header libraries, allowing for fast graphics apps developement, from easy 2d-renderers, through gui applications to performant games.

## Philosophy

Light Framework is built around a small set of principles intended to make low-level graphics and application development practical, predictable, and enjoyable in pure C99.

**Conciseness**

The API is designed to stay minimal and focused.
Libraries expose only the functionality required to solve common problems without unnecessary abstractions, boilerplate, or hidden behavior.

**Stability**

The framework prioritizes long-term API consistency.
Interfaces are designed to remain reliable across versions, allowing projects to evolve without constant rewrites or breaking changes.

**Ease of Use**

The framework emphasizes straightforward integration and fast iteration.
Most modules are distributed as single-header libraries, require minimal setup, and expose intuitive APIs suitable for both prototypes and larger applications.

**Performance-Oriented Design**

The libraries are implemented with real-time applications in mind.
Abstractions are lightweight, allocations are kept explicit whenever possible, and systems are designed to work efficiently with modern graphics APIs and game loops.

**Modularity**

Each library is separate, composable being.
You can use only the components you need — from low-level rendering and input handling to higher-level UI and rendering utilities — without pulling in the entire framework.

## Contents
Light framework consists of two types of libraries: 
* core ones, that depends on the platform (OS, supported graphic APIS)
* higher level ones, that depends on core libraries, and implements some commonly used behavior

### Core Libraries - Platform dependent
| Library File      | Function |
| -------------     | ------------- |
| graphics.h        | Cross platform, graphics api independent RHI |
| input.h           | Cross platform, input library allowing reading input from mouses, keyboards and gamepads |
| linear_algebra.h  | Linear algebra library |
| user_interface.h  | Node-based, data driven, user inteface system |

### Higher Libraries - Core libraries dependent

| Library File                  | Function |
| -------------                 | ------------- |
| synchronised_window.h         | Window wrapper, with builtin frames-in-flight synchronisation |
| font.h                        | Font object, which can be queried for glyphs metrics, creates gpu font atlas texutre |
| shapes_rendering.h            | Immediate-mode rendering pipeline for basic 2D shapes |
| user_interface_rendering.h    | Immediate-mode rendering pipeline for user_interface.h |

## Usage

Big core systems have a separate documentation inside ``documentation`` folder.
Smaller libraries have big header comment, describing what and how.

## Building

Most of libraries provided are single-header libraries.
To build such a library in a blank .c file you write:

```c
#define LIGHT_XXX_IMPL
// here also additional compile flags
#include "light/XXX.h"
#endif
```

Each file, have a header with a ``Code info`` section - see it for more details.  
Also core libraries may require some extra third-party depedencies - in case your build fails, see ``Code info``.

There are also header-only libraries that does not require such a building step.
