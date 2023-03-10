/*
 * Copyright (C) 1992-1995 Tony Robinson
 * Copyright (C) 2023      Jan Starý
 */

# include <stdio.h>
# include <err.h>

# include "shorten.h"

# define USIZE 256
# define HUSIZE 128
# define SHIFTSIZE 13

char	*filenameo = NULL;
FILE	*fileo = NULL;

int main() {
  FILE *fout;
  char *filename = "bitshift.h", *writemode = "w";
  int shift, i;
  int tab[USIZE];
  long sample;
  long **forwardmap = long2d(SHIFTSIZE, USIZE);
  long **reversemap = long2d(SHIFTSIZE, USIZE);

  if ((fout = fopen(filename, writemode)) == NULL)
    err(1, "%s", filename);

  for(i = 0; i < USIZE; i++) tab[i] = 0;

  /* brute force search of the largest number of zero bits in a linear value */
  for(shift = 0; shift < SHIFTSIZE; shift++)
    for(sample = -(1L << 15); sample < (1L << 15); sample += 1 << (shift + 3))
      tab[Slinear2ulaw(sample)] = shift;

  /* print this out as a lookup table */
  fprintf(fout, "char ulaw_maxshift[%d] = {", USIZE);
  for(i = 0; i < USIZE - 1; i++)
    fprintf(fout, "%d,", tab[i]);
  fprintf(fout, "%d};\n\n", tab[USIZE - 1]);

  /* compute the greatest inward shift compatable with ??? */
  for(shift = 0; shift < SHIFTSIZE; shift++) {
    int nused;
  
    nused = 0;
    for(i = 255; i >= 128; i--)
      if(tab[i] >= shift) forwardmap[shift][i] = nused++;
    for(i = 255; i >= 128; i--)
      if(tab[i] < shift) forwardmap[shift][i] = nused++;
  
    nused = -1;
    for(i = 126; i >= 0; i--)
      if(tab[i] >= shift) forwardmap[shift][i] = nused--;
    forwardmap[shift][127] = nused--;
    for(i = 126; i >= 0; i--)
      if(tab[i] < shift) forwardmap[shift][i] = nused--;

    for(i = 0; i < USIZE; i++)
      reversemap[shift][forwardmap[shift][i] + HUSIZE] = i;
  }

  /* simple check */
  for(shift = 0; shift < SHIFTSIZE; shift++)
    for(i = 0; i < USIZE; i++)
      if(forwardmap[shift][reversemap[shift][i]] != i - HUSIZE)
       errx(1, "identity maping failed for shift: %d\tindex: %d\n",shift,i);

  /* print out the ulaw_inward lookup table */
  fprintf(fout, "char ulaw_inward[%d][%d] = {\n", SHIFTSIZE, USIZE);
  for(shift = 0; shift < SHIFTSIZE; shift++) {
    fprintf(fout, "{");
    for(i = 0; i < USIZE - 1; i++)
      fprintf(fout, "%ld,", forwardmap[shift][i]);
    if(shift != SHIFTSIZE - 1)
      fprintf(fout, "%ld},\n", forwardmap[shift][USIZE - 1]);
    else
      fprintf(fout, "%ld}\n};\n", forwardmap[shift][USIZE - 1]);
  }
  fprintf(fout, "\n");

  /* print out the ulaw_outward lookup table */
  fprintf(fout, "uchar ulaw_outward[%d][%d] = {\n", SHIFTSIZE, USIZE);
  for(shift = 0; shift < SHIFTSIZE; shift++) {
    fprintf(fout, "{");
    for(i = 0; i < USIZE - 1; i++)
      fprintf(fout, "%ld,", reversemap[shift][i]);
    if(shift != SHIFTSIZE - 1)
      fprintf(fout, "%ld},\n", reversemap[shift][USIZE - 1]);
    else
      fprintf(fout, "%ld}\n};\n", reversemap[shift][USIZE - 1]);
  }

  return(0);
}
