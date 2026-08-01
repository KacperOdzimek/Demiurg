/*
----------------------------------------------------------------
Contents:
This file builds all fundatio libraries, to avoid building one after another.
Additional flags like FUNDATIO_GRAPHICS_VALIDATE may be declared before this file.

----------------------------------------------------------------
Code info:
- FUNDATIO_IMPL macro to build
*/

#ifdef FUNDATIO_IMPL

#define FUNDATIO_GRAPHICS_IMPL
#include "fundatio/platform/graphics.h"

#define FUNDATIO_THREADS_IMPL
#include "fundatio/platform/threads.h"

#define FUNDATIO_FILESYSTEM_IMPL
#include "fundatio/platform/filesystem.h"

#define FUNDATIO_INPUTS_IMPL
#include "fundatio/platform/inputs.h"

#define FUNDATIO_PARTITIONER_IMPL
#include "fundatio/algorithm/partitioner.h"

#define FUNDATIO_SEGMENTER_IMPL
#include "fundatio/algorithm/segmenter.h"

#endif
