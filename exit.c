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
# include <setjmp.h>
# include <stdarg.h>
# include "shorten.h"

extern char *argv0;
extern char *filenameo;
extern FILE *fileo;

jmp_buf exitenv;
char    *exitmessage;

/***************************************************************************/

void basic_exit(exitcode) int exitcode; {

  /* try to delete the output file on all abnormal exit conditions */
  if(exitcode != 0 && fileo != NULL && fileo != stdout) {
    fclose(fileo);
    unlink(filenameo);
  }

  if(exitmessage == NULL)
    exit(exitcode < 0 ? 0 : exitcode);
  else
    longjmp(exitenv, exitcode);
}

/****************************************************************************
** error_exit() - standard error handler with printf() syntax
*/
void error_exit(char* fmt, ...) {
  va_list args;

  va_start(args, fmt);

  if(exitmessage == NULL)
  {
    fprintf(stderr, "%s: ", argv0);
    (void) vfprintf(stderr, fmt, args);
  }
  else
  {
    (void) vsprintf(exitmessage, fmt, args);
    strcat(exitmessage, "\n");
  }

  va_end(args);

  basic_exit(errno);
}

/****************************************************************************
** perror_exit() - system error handler with printf() syntax
**
** Appends system error message based on errno
*/
void perror_exit(char* fmt, ...) {
  va_list args;

  va_start(args, fmt);

  if(exitmessage == NULL) {
    fprintf(stderr, "%s: ", argv0);
    (void) vfprintf(stderr, fmt, args);
    (void) fprintf(stderr, ": ");
    perror("\0");
  }
  else {
    (void) vsprintf(exitmessage, fmt, args);
    strcat(exitmessage, ": ");
    /* if running SunOS4 use: strcat(exitmessage, sys_errlist[errno]); */
    strcat(exitmessage, strerror(errno));
    strcat(exitmessage, "\n");
  }

  va_end(args);

  basic_exit(errno);
}

void usage_exit(int exitcode, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if(exitmessage == NULL) {
    if(fmt != NULL) {
      fprintf(stderr, "%s: ", argv0);
      (void) vfprintf(stderr, fmt, args);
    }
  }
  else
  {
    (void) vsprintf(exitmessage, fmt, args);
    strcat(exitmessage, "\n");
  }
  va_end(args);
  basic_exit(exitcode);
}


void update_exit(int exitcode, char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if(exitmessage == NULL) {
    if(fmt != NULL) {
      fprintf(stderr, "%s: ", argv0);
      (void) vfprintf(stderr, fmt, args);
    }
    fprintf(stderr, "%s: version %d.%s\n",argv0,FORMAT_VERSION,BUGFIX_RELEASE);
    fprintf(stderr, "%s: please report this problem to ajr@softsound.com\n", argv0);
  }
  va_end(args);
  basic_exit(exitcode);
}
