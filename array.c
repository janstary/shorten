/*
 * Copyright (C) 1992-1995 Tony Robinson
 * Copyright (C) 2023      Jan Starý
 */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include "shorten.h"

void *
pmalloc(size_t size)
{
	void *ptr;
	if ((ptr = calloc(1, size)) == NULL)
		err(1, NULL);
	return ptr;
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
