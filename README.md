# Light Framework

Collection of C99 (single) header libraries, allowing for fast graphics apps developement, from easy 2d-renderers, through gui-only applications to even performant games.

The libraries are loosely connected, and coded with minimalism in mind, so there is no code bloat.

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

WRITE DOCS

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
