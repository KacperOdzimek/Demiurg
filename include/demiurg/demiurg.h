/*
----------------------------------------------------------------
Contents:
This file builds all demiurg libraries, to avoid building one after another.
Additional flags like DEMIURG_GRAPHICS_VALIDATE may be declared before this file.

----------------------------------------------------------------
Code info:
- DEMIURG_IMPL macro to build
*/

#ifdef DEMIURG_IMPL

#define DEMIURG_GRAPHICS_IMPL
#include "demiurg/platform/graphics.h"

#define DEMIURG_THREADS_IMPL
#include "demiurg/platform/threads.h"

#define DEMIURG_FILESYSTEM_IMPL
#include "demiurg/platform/filesystem.h"

#define DEMIURG_INPUTS_IMPL
#include "demiurg/platform/inputs.h"

#define DEMIURG_PARTITIONER_IMPL
#include "demiurg/algorithm/partitioner.h"

#define DEMIURG_SEGMENTER_IMPL
#include "demiurg/algorithm/segmenter.h"

#endif
