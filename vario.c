
/*
 * Copyright (C) 1992-1995 Tony Robinson
 * Copyright (C) 2023      Jan Starý
 */

# include <stdlib.h>
# include <stdio.h>
# include <err.h>

# include "shorten.h"

# define MASKTABSIZE 33
ulong masktab[MASKTABSIZE];

void mkmasktab() {
  int i;
  ulong val = 0;

  masktab[0] = val;
  for(i = 1; i < MASKTABSIZE; i++) {
    val <<= 1;
    val |= 1;
    masktab[i] = val;
  }
}

static uchar *putbuf;
static uchar *putbufp;
static ulong  pbuffer;
static int    nbitput;

void var_put_init() {
  mkmasktab();

  putbuf   = (uchar*) pmalloc(BUFSIZ);
  putbufp  = putbuf;
  pbuffer  = 0;
  nbitput  = 0;
}

static uchar *getbuf;
static uchar *getbufp;
static int    nbyteget;
static ulong  gbuffer;
static int    nbitget;

void var_get_init() {
  mkmasktab();

  getbuf   = (uchar*) pmalloc(BUFSIZ);
  getbufp  = getbuf;
  nbyteget = 0;
  gbuffer  = 0;
  nbitget  = 0;
}

void word_put(ulong buffer, FILE *stream)
{
  *putbufp++ = buffer >> 24;
  *putbufp++ = buffer >> 16;
  *putbufp++ = buffer >>  8;
  *putbufp++ = buffer;

  if(putbufp - putbuf == BUFSIZ) {
    if(fwrite((char*) putbuf, 1, BUFSIZ, stream) != BUFSIZ)
      errx(1, "failed to write compressed stream\n");
    putbufp = putbuf;
  }
}

void uvar_put(val, nbin, stream) ulong val; int nbin; FILE *stream; {
  ulong lobin, nsd;
  int  i, nlobin;

  if(nbin >= MASKTABSIZE)
    errx(1, "overflow of masktab[%d]\n", MASKTABSIZE);

  lobin = (1L << nbin) | (val & masktab[nbin]);
  nsd = val >> nbin;
  nlobin = nbin + 1;

  if(nbitput + nsd >= 32) {
    for(i = 0; i < ((nbitput + nsd) >> 5); i++) {
      word_put(pbuffer, stream);
      pbuffer = 0;
    }
    nbitput = (nbitput + nsd) % 32;
  }
  else
    nbitput += nsd;

  while(nlobin != 0) {
    if(nbitput + nlobin >= 32) {
      pbuffer |= (lobin >> (nbitput + nlobin - 32));
      word_put(pbuffer, stream);
      pbuffer = 0;
      nlobin -= 32 - nbitput;
      nbitput = 0;
    }
    else {
      nbitput += nlobin;
      pbuffer |= (lobin << (32 - nbitput));
      nlobin = 0;
    }
  }
}

void
ulong_put(uint32_t val, FILE *stream)
{
  int i;
  uint32_t nbit;

  for(i = 31; i >= 0 && (val & (1L << i)) == 0; i--);
  nbit = i + 1;

  uvar_put(nbit, 2, stream);
  uvar_put(val & masktab[nbit], nbit, stream);
}

uint32_t
word_get(FILE *stream)
{
  uint32_t buffer;

  if(nbyteget < 4) {
    nbyteget += fread((char*) getbuf, 1, BUFSIZ, stream);
    if(nbyteget < 4)
      errx(1, "premature EOF on compressed stream\n");
    getbufp = getbuf;
  }
  buffer =
	  (((uint32_t) getbufp[0]) << 24) |
	  (((uint32_t) getbufp[1]) << 16) |
	  (((uint32_t) getbufp[2]) <<  8) |
	  ((uint32_t)  getbufp[3]);
  getbufp += 4;
  nbyteget -= 4;
  return(buffer);
}

uint32_t
uvar_get(int nbin, FILE *stream)
{
  uint32_t result;

  if(nbitget == 0) {
    gbuffer = word_get(stream);
    nbitget = 32;
  }

  for(result = 0; !(gbuffer & (1L << --nbitget)); result++) {
    if(nbitget == 0) {
      gbuffer = word_get(stream);
      nbitget = 32;
    }
  }

  while(nbin != 0) {
    if(nbitget >= nbin) {
      result = (result << nbin) | ((gbuffer >> (nbitget-nbin)) &masktab[nbin]);
      nbitget -= nbin;
      nbin = 0;
    } 
    else {
      result = (result << nbitget) | (gbuffer & masktab[nbitget]);
      gbuffer = word_get(stream);
      nbin -= nbitget;
      nbitget = 32;
    }
  }

  return(result);
}

uint32_t
ulong_get(FILE *stream)
{
  uint16_t nbit = uvar_get(2, stream);
  return uvar_get(nbit, stream);
}

void var_put(val, nbin, stream) long val; int nbin; FILE *stream; {
  if(val < 0) uvar_put((ulong) ((~val) << 1) | 1L, nbin + 1, stream);
  else uvar_put((ulong) ((val) << 1), nbin + 1, stream);
}

void var_put_quit(stream) FILE *stream; {
  /* flush to a word boundary */
  uvar_put((ulong) 0, 31, stream);

  /* and write out the remaining chunk in the buffer */
  if(fwrite((char*) putbuf, 1, putbufp - putbuf, stream) != 
     putbufp - putbuf)
    errx(1, "failed to write compressed stream\n");
  free((char*) putbuf);
}

int32_t
var_get(nbin, stream) int nbin; FILE *stream; {
  uint32_t uvar = uvar_get(nbin + 1, stream);

  if(uvar & 1) return((int32_t) ~(uvar >> 1));
  else return((int32_t) (uvar >> 1));
}

void var_get_quit() {
  free((char*) getbuf);
}

int sizeof_uvar(val, nbin) ulong val; int nbin; {
  return((val >> nbin) + nbin);
}

int sizeof_var(val, nbin) long val; int nbin; {
  return((labs(val) >> nbin) + nbin + 1);
}
