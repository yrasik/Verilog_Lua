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

#include <stdio.h>
#include <stdarg.h>
#include "debug.h"


/**
 * \brief The main debug message output function
 */
void DebugMessage(FILE *fd, int level, const char *prefix,
                    const char *suffix, const char *function, int line, const char *errFmt, ...)
{
  va_list arg;
  int print = 0;

  if (MSG_LEVEL < MSG_ERROR)
  {
    return;
  }

  if ( (level == MSG_INFO) && (MSG_LEVEL >= MSG_INFO) )
  {
    fprintf(fd, "INFO: ");
    print = 1;
  }

  if ( (level == MSG_WARNING) && (MSG_LEVEL >= MSG_WARNING) )
  {
    fprintf(fd, "WARNING: ");
    print = 1;
  }

  if ( (level == MSG_ERROR) && (MSG_LEVEL >= MSG_ERROR) )
  {
    fprintf(fd, "ERROR: ");
    print = 1;
  }

  if(print == 0)
  {
    return;
  }

  if (prefix)
    fprintf(fd, "%s: ", prefix);

#ifdef CONFIG_DBG_SHOW_FUNCTION
  if (line > 0)
    fprintf(fd, "%s: ", function);
#endif

#ifdef CONFIG_DBG_SHOW_LINE_NUM
  if (line > 0)
    fprintf(fd, "@%d - ", line);
#endif

  va_start(arg, errFmt);
  vfprintf(fd, errFmt, arg);
  va_end(arg);

  if (suffix)
    fprintf(fd, "%s", suffix);

  return;
}

