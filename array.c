/******************************************************************************
*                                                                             *
*       Copyright (C) 1992-1995 Tony Robinson                                 *
*                                                                             *
*       See the file LICENSE for conditions on distribution and usage         *
*                                                                             *
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "shorten.h"

void *
pmalloc(size_t size)
{
	void *ptr;
	ptr = malloc(size);
	if (ptr == NULL)
		perror_exit("malloc(%ld)", size);
	return (ptr);
}

long **
long2d(size_t num, size_t each)
{
	long **a;

	if ((a = (long **)
	pmalloc((num * sizeof(long*) + num * each * sizeof(long)))) != NULL) {
		long *b = (long *)(a + num);
		int i;
		for (i = 0; i < num; i++)
			a[i] = b + i * each;
	}
	return a;
}
