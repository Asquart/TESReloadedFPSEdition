#pragma once

#if defined(_WIN32)
#  ifdef TESR_BRIDGE_BUILD
#    define TESR_BRIDGE_API __declspec(dllexport)
#  else
#    define TESR_BRIDGE_API __declspec(dllimport)
#  endif
#else
#  ifdef TESR_BRIDGE_BUILD
#    define TESR_BRIDGE_API __attribute__((visibility("default")))
#  else
#    define TESR_BRIDGE_API
#  endif
#endif
