#pragma once

// Compilation config. Run with -DRPI to build a release build for the pi

#ifdef RPI
#define TARGET_RPI 1

#define UIFRAMEWORK_RPI 1

#else
#define TARGET_DEV 1

#define UIFRAMEWORK_GLFW 1

#endif

