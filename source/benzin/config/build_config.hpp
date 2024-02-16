#pragma once

#if defined(BENZIN_DEBUG_BUILD)
  #define BENZIN_IS_DEBUG_BUILD 1
  #define BENZIN_IS_RELEASE_BUILD 0
#elif defined(BENZIN_RELEASE_BUILD)
  #define BENZIN_IS_DEBUG_BUILD 0
  #define BENZIN_IS_RELEASE_BUILD 1
#else
  #error Unknown build type
#endif

#if defined(BENZIN_PLATFORM_WIN64)
  #define BENZIN_IS_PLATFORM_WIN64 1
#else
  #error Unknown platform
#endif
