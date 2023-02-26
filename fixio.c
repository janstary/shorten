/*
 * Copyright (C) 1992-1995 Tony Robinson
 * Copyright (C) 2023      Jan Starý
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

# include "shorten.h"
# include "bitshift.h"

#define CAPS8(x)  ((x > 127) ? 127 : x)
#define CAPU8(x)  ((x > 255) ? 255 : x)
#define CAPS16(x) ((x > 32767) ? 32767 : x)
#define CAPU16(x) ((x > 65535) ? 65535 : x)

static int sizeof_sample[TYPE_EOF];

void init_sizeof_sample() {
  sizeof_sample[TYPE_AU1]   = sizeof(uchar);
  sizeof_sample[TYPE_S8]    = sizeof(char);
  sizeof_sample[TYPE_U8]    = sizeof(uchar);
  sizeof_sample[TYPE_S16HL] = sizeof(int16_t);
  sizeof_sample[TYPE_U16HL] = sizeof(int16_t);
  sizeof_sample[TYPE_S16LH] = sizeof(int16_t);
  sizeof_sample[TYPE_U16LH] = sizeof(int16_t);
  sizeof_sample[TYPE_ULAW]  = sizeof(uchar);
  sizeof_sample[TYPE_AU2]   = sizeof(uchar);
  sizeof_sample[TYPE_AU3]   = sizeof(uchar);
  sizeof_sample[TYPE_ALAW]  = sizeof(uchar);
}

/**************/
/* fixed read */
/**************/

static char *readbuf, *readfub;
static int  nreadbuf;

void fread_type_init() {
  init_sizeof_sample();
  readbuf  = (char*) NULL;
  readfub  = (char*) NULL;
  nreadbuf = 0;
}

void fread_type_quit() {
  if(readbuf != NULL) free(readbuf);
  if(readfub != NULL) free(readfub);
}

/* read a file for a given data type and convert to signed long ints */
int fread_type(data, ftype, nchan, nitem, stream, datalen) long **data;
		int ftype, nchan, nitem; FILE* stream; long *datalen;
{
  int hiloint = 1, hilo = !(*((char*) &hiloint));
  int i, nbyte = 0, nread, datasize = sizeof_sample[ftype], chan;
  long *data0 = data[0];

  if (*datalen >= 0 && nchan * nitem * datasize > *datalen)
    nitem = *datalen / (nchan * datasize);

  if(nreadbuf < nchan * nitem * datasize) {
    nreadbuf = nchan * nitem * datasize;
    if(readbuf != NULL) free(readbuf);
    if(readfub != NULL) free(readfub);
    readbuf = (char*) pmalloc(nreadbuf);
    readfub = (char*) pmalloc(nreadbuf);
  }

  switch(ftype) {
  case TYPE_AU1:
  case TYPE_S8:
  case TYPE_U8:
  case TYPE_ULAW:
  case TYPE_AU2:
  case TYPE_AU3:
  case TYPE_ALAW:
    nbyte = fread((char*) readbuf, 1 , datasize * nchan * nitem, stream);
    break;
  case TYPE_S16HL:
  case TYPE_U16HL:
    if(hilo)
      nbyte = fread((char*) readbuf, 1 , datasize * nchan * nitem, stream);
    else {
      nbyte = fread((char*) readfub, 1 , datasize * nchan * nitem, stream);
      swab(readfub, readbuf, nbyte);
    }
    break;
  case TYPE_S16LH:
  case TYPE_U16LH:
    if(hilo) {
      nbyte = fread((char*) readfub, 1 , datasize * nchan * nitem, stream);
      swab(readfub, readbuf, nbyte);
    }
    else
      nbyte = fread((char*) readbuf, 1 , datasize * nchan * nitem, stream);
    break;
  default:
    errx(1, "can't read file type: %d\n", ftype);
  }

  if (*datalen > 0)
    *datalen -= nbyte;

  { int nextra = nbyte % (datasize * nchan);
    if(nextra != 0)
      errx(1, "alignment problem: %d extra bytes\n", nextra);
  }
  nread = nbyte / (datasize * nchan);

  switch(ftype) {
  case TYPE_AU1:
  case TYPE_AU2:
  case TYPE_U8: {
    uchar *readbufp = (uchar*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = *readbufp++;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = *readbufp++;
    break;
  }
  case TYPE_S8: {
    char *readbufp = (char*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = *readbufp++;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = *readbufp++;
    break;
  }
  case TYPE_S16HL:
  case TYPE_S16LH: {
    int16_t *readbufp = (int16_t*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = *readbufp++;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = *readbufp++;
    break;
  }
  case TYPE_U16HL:
  case TYPE_U16LH: {
    uint16_t *readbufp = (uint16_t*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = *readbufp++;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = *readbufp++;
    break;
  }
  case TYPE_ULAW: {
    uchar *readbufp = (uchar*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = Sulaw2linear(*readbufp++) >> 3;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = Sulaw2linear(*readbufp++) >> 3;
    break;
  }
  case TYPE_AU3: {
    uchar *readbufp = (uchar*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++) {
	int alaw = *readbufp++;

	if(alaw < 128)
	  data0[i] = 127 - (alaw ^ 0xd5);
	else
	  data0[i] = (alaw ^ 0x55) - 128;
      }
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++) {
	  int alaw = *readbufp++;

	  if(alaw < 128)
	    data[chan][i] = 127 - (alaw ^ 0xd5);
	  else
	    data[chan][i] = (alaw ^ 0x55) - 128;
	}
    break;
  }
  case TYPE_ALAW: {
    uchar *readbufp = (uchar*) readbuf;
    if(nchan == 1)
      for(i = 0; i < nread; i++)
	data0[i] = Salaw2linear(*readbufp++) >> 3;
    else
      for(i = 0; i < nread; i++)
	for(chan = 0; chan < nchan; chan++)
	  data[chan][i] = Salaw2linear(*readbufp++) >> 3;
    break;
  }
  }
  return(nread);
}


/***************/
/* fixed write */
/***************/

static char *writebuf, *writefub;
static int  nwritebuf;

void fwrite_type_init() {
  init_sizeof_sample();
  writebuf  = (char*) NULL;
  writefub  = (char*) NULL;
  nwritebuf = 0;
}

void fwrite_type_quit() {
  if(writebuf != NULL) free(writebuf);
  if(writefub != NULL) free(writefub);
}

/* convert from signed ints to a given type and write */
void fwrite_type(data, ftype, nchan, nitem, stream) long **data; int ftype,
       nchan, nitem; FILE* stream; {
  int hiloint = 1, hilo = !(*((char*) &hiloint));
  int i, nwrite = 0, datasize = sizeof_sample[ftype], chan;
  long *data0 = data[0];

  if(nwritebuf < nchan * nitem * datasize) {
    nwritebuf = nchan * nitem * datasize;
    if(writebuf != NULL) free(writebuf);
    if(writefub != NULL) free(writefub);
    writebuf = (char*) pmalloc(nwritebuf);
    writefub = (char*) pmalloc(nwritebuf);
  }

  switch(ftype) {
  case TYPE_AU1: /* leave the conversion to fix_bitshift() */
  case TYPE_AU2: {
    uchar *writebufp = (uchar*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = data0[i];
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = data[chan][i];
    break;
  }
  case TYPE_U8: {
    uchar *writebufp = (uchar*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = CAPU8(data0[i]);
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ =  CAPU8(data[chan][i]);
    break;
  }
  case TYPE_S8: {
    char *writebufp = (char*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = CAPS8(data0[i]);
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = CAPS8(data[chan][i]);
    break;
  }
  case TYPE_S16HL:
  case TYPE_S16LH: {
    int16_t *writebufp = (int16_t*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = CAPS16(data0[i]);
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = CAPS16(data[chan][i]);
    break;
  }
  case TYPE_U16HL:
  case TYPE_U16LH: {
    uint16_t *writebufp = (uint16_t*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = CAPU16(data0[i]);
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = CAPU16(data[chan][i]);
    break;
  }
  case TYPE_ULAW: {
    uchar *writebufp = (uchar*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = Slinear2ulaw(CAPS16(data0[i] << 3));
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = Slinear2ulaw(CAPS16(data[chan][i] << 3));
    break;
  }
  case TYPE_AU3: {
    uchar *writebufp = (uchar*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	if(data0[i] < 0)
	  *writebufp++ = (127 - data0[i]) ^ 0xd5;
	else
	  *writebufp++ = (data0[i] + 128) ^ 0x55;
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  if(data[chan][i] < 0)
	    *writebufp++ = (127 - data[chan][i]) ^ 0xd5;
	  else
	    *writebufp++ = (data[chan][i] + 128) ^ 0x55;
    break;
  }
  case TYPE_ALAW: {
    uchar *writebufp = (uchar*) writebuf;
    if(nchan == 1)
      for(i = 0; i < nitem; i++)
	*writebufp++ = Slinear2alaw(CAPS16(data0[i] << 3));
    else
      for(i = 0; i < nitem; i++)
	for(chan = 0; chan < nchan; chan++)
	  *writebufp++ = Slinear2alaw(CAPS16(data[chan][i] << 3));
    break;
  }
  }
  
  switch(ftype) {
  case TYPE_AU1:
  case TYPE_S8:
  case TYPE_U8:
  case TYPE_ULAW:
  case TYPE_AU2:
  case TYPE_AU3:
  case TYPE_ALAW:
    nwrite = fwrite((char*) writebuf, datasize * nchan, nitem, stream);
    break;
  case TYPE_S16HL:
  case TYPE_U16HL:
    if(hilo)
      nwrite = fwrite((char*) writebuf, datasize * nchan, nitem, stream);
    else {
      swab(writebuf, writefub, datasize * nchan * nitem);
      nwrite = fwrite((char*) writefub, datasize * nchan, nitem, stream);
    }
    break;
  case TYPE_S16LH:
  case TYPE_U16LH:
    if(hilo) {
      swab(writebuf, writefub, datasize * nchan * nitem);
      nwrite = fwrite((char*) writefub, datasize * nchan, nitem, stream);
    }
    else
      nwrite = fwrite((char*) writebuf, datasize * nchan, nitem, stream);
    break;
  }

  if(nwrite != nitem)
    errx(1, "failed to write decompressed stream\n");
}

/*************/
/* bitshifts */
/*************/

int find_bitshift(data, nitem, ftype) long *data; int nitem, ftype; {
  int i, bitshift;
  
  if(ftype == TYPE_AU1 || ftype == TYPE_AU2) {
    bitshift = NBITS;
    for(i = 0; i < nitem && 
	(bitshift = MIN(bitshift, ulaw_maxshift[data[i]])) != 0; i++);
    if(ftype == TYPE_AU1)
      for(i = 0; i < nitem; i++)
	data[i] = ulaw_inward[bitshift][data[i]];
    else
      for(i = 0; i < nitem; i++) {
	if(data[i] > NEGATIVE_ULAW_ZERO)
	  data[i] = ulaw_inward[bitshift][data[i]];
	else if(data[i] == NEGATIVE_ULAW_ZERO)
	  data[i] = -1;
	else
	  data[i] = ulaw_inward[bitshift][data[i]] - 1;
      }
  }
  else {
    int hash = 0;
	
    for(i = 0; i < nitem && ((hash |= data[i]) & 1) == 0; i++);
    if(hash != 0) {
      for(bitshift = 0; (hash & 1) == 0; bitshift++) hash >>= 1;
      if(bitshift != 0)
	for(i = 0; i < nitem; i++) data[i] >>= bitshift;
    }
    else
      bitshift = NBITS;
  }

  return(bitshift);
}

void fix_bitshift(buffer, nitem, bitshift, ftype) long *buffer; int nitem,
       bitshift, ftype; {
  int i;

  if(ftype == TYPE_AU1)
    for(i = 0; i < nitem; i++)
      buffer[i] = ulaw_outward[bitshift][buffer[i] + 128];
  else if(ftype == TYPE_AU2)
    for(i = 0; i < nitem; i++) {
      if(buffer[i] >= 0)
	buffer[i] = ulaw_outward[bitshift][buffer[i] + 128];
      else if(buffer[i] == -1)
	buffer[i] =  NEGATIVE_ULAW_ZERO;
      else
	buffer[i] = ulaw_outward[bitshift][buffer[i] + 129];
    }
  else
    if(bitshift != 0)
      for(i = 0; i < nitem; i++)
	buffer[i] <<= bitshift;
}
