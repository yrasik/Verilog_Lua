/*
 * lua_device for QEMU
 *
 * Yuri Stepanenko stepanenkoyra@gmail.com   2022
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

/*
В данном контексте нельзя применять это. Важно подключать к каждому
с - файлу свою копию debug.h. Иначе правильного эффекта не будет
#ifndef _DEBUG_H_
#define _DEBUG_H_
*/


#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>

/********************************** debug & error *************************************/
//#define PFX  __FILE__": "
//#define  _FD_  fd /* FILE *fd   or  stderr */

#define MSG_ERROR       1
#define MSG_WARNING     2
#define MSG_INFO        3


#ifndef TENDSTR
  #define TENDSTR         "\n"
#endif

#ifndef MSG_LEVEL
  #define MSG_LEVEL       MSG_INFO
#endif


#define CONFIG_DBG_SHOW_FUNCTION
#define CONFIG_DBG_SHOW_LINE_NUM


void DebugMessage(FILE *fd, int level, const char *prefix, const char *suffix, const char *function, int line, const char *errFmt, ...);


#ifndef DEBUG
  #define REPORT(level, fmt, ...)
#else
  #define REPORT(level, fmt, ... ) \
    DebugMessage(_FD_, level, PFX, TENDSTR, __func__, __LINE__, fmt, ## __VA_ARGS__)
#endif


#ifdef __cplusplus
}
#endif


//#endif /*_DEBUG_H_*/
