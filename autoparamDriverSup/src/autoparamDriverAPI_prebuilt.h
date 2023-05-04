/* This is a generated file, do not edit! */
/* This file was pre-built for compatibility with EPICS older than 7.0.4 */

#ifndef INC_autoparamDriverAPI_H
#define INC_autoparamDriverAPI_H

#if defined(_WIN32) || defined(__CYGWIN__)

#  if !defined(epicsStdCall)
#    define epicsStdCall __stdcall
#  endif

#  if defined(BUILDING_autoparamDriver_API) && defined(EPICS_BUILD_DLL)
/* Building library as dll */
#    define AUTOPARAMDRIVER_API __declspec(dllexport)
#  elif !defined(BUILDING_autoparamDriver_API) && defined(EPICS_CALL_DLL)
/* Calling library in dll form */
#    define AUTOPARAMDRIVER_API __declspec(dllimport)
#  endif

#elif __GNUC__ >= 4
#  define AUTOPARAMDRIVER_API __attribute__ ((visibility("default")))
#endif

#if !defined(AUTOPARAMDRIVER_API)
#  define AUTOPARAMDRIVER_API
#endif

#if !defined(epicsStdCall)
#  define epicsStdCall
#endif

#endif /* INC_autoparamDriverAPI_H */

