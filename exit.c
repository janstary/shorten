/******************************************************************************
*                                                                             *
*       Copyright (C) 1992-1996 Tony Robinson and SoftSound Limited           *
*                                                                             *
*       See the file LICENSE for conditions on distribution and usage         *
*                                                                             *
******************************************************************************/
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <stdarg.h>
# include "shorten.h"

extern char *argv0;
extern char *filenameo;
extern FILE *fileo;

void basic_exit(exitcode) int exitcode; {
    exit(exitcode < 0 ? 0 : exitcode);
}

void usage_exit(int exitcode, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  {
    if(fmt != NULL) {
      fprintf(stderr, "%s: ", argv0);
      (void) vfprintf(stderr, fmt, args);
    }
  }
  va_end(args);
  basic_exit(exitcode);
}

void update_exit(int exitcode, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  {
    if(fmt != NULL) {
      fprintf(stderr, "%s: ", argv0);
      (void) vfprintf(stderr, fmt, args);
    }
  }
  va_end(args);
  basic_exit(exitcode);
}
