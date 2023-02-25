/******************************************************************************
*                                                                             *
*       Copyright (C) 1992-1995 Tony Robinson                                 *
*                                                                             *
*       See the file LICENSE for conditions on distribution and usage         *
*                                                                             *
******************************************************************************/

#include <stdint.h>

#define MAGIC			"ajkg"
#define FORMAT_VERSION		2
#define MIN_SUPPORTED_VERSION	1
#define MAX_SUPPORTED_VERSION	2
#define MAX_VERSION		7
#define BUGFIX_RELEASE		"3a"

#define UNDEFINED_UINT		-1
#define DEFAULT_BLOCK_SIZE	256
#define DEFAULT_V0NMEAN	0
#define DEFAULT_V2NMEAN	4
#define DEFAULT_MAXNLPC	0
#define DEFAULT_NCHAN		1
#define DEFAULT_NSKIP		0
#define DEFAULT_NDISCARD	0
#define NBITS			32
#define DEFAULT_MINSNR          256
#define DEFAULT_MAXRESNSTR	"32.0"
#define DEFAULT_QUANTERROR	0
#define MINBITRATE		2.5

#define MAX_LPC_ORDER	64
#define CHANSIZE	0
#define ENERGYSIZE	3
#define BITSHIFTSIZE	2
#define NWRAP		3

#define FNSIZE		2
#define FN_DIFF0	0
#define FN_DIFF1	1
#define FN_DIFF2	2
#define FN_DIFF3	3
#define FN_QUIT		4
#define FN_BLOCKSIZE	5
#define FN_BITSHIFT	6
#define FN_QLPC		7
#define FN_ZERO		8
#define FN_VERBATIM     9

#define VERBATIM_CKSIZE_SIZE 5	/* a var_put code size */
#define VERBATIM_BYTE_SIZE 8	/* code size 8 on single bytes means
				 * no compression at all */
#define VERBATIM_CHUNK_MAX 256	/* max. size of a FN_VERBATIM chunk */

#define NSKIPSIZE	1
#define LPCQSIZE	2
#define LPCQUANT	5
#define XBYTESIZE	7

#define TYPESIZE	4
#define TYPE_AU1	0	/* original lossless ulaw                    */
#define TYPE_S8	        1	/* signed 8 bit characters                   */
#define TYPE_U8         2	/* unsigned 8 bit characters                 */
#define TYPE_S16HL	3	/* signed 16 bit shorts: high-low            */
#define TYPE_U16HL	4	/* unsigned 16 bit shorts: high-low          */
#define TYPE_S16LH	5	/* signed 16 bit shorts: low-high            */
#define TYPE_U16LH	6	/* unsigned 16 bit shorts: low-high          */
#define TYPE_ULAW	7	/* lossy ulaw: internal conversion to linear */
#define TYPE_AU2	8	/* new ulaw with zero mapping                */
#define TYPE_AU3	9	/* lossless alaw                             */
#define TYPE_ALAW 	10	/* lossy alaw: internal conversion to linear */
#define TYPE_RIFF_WAVE  11	/* Microsoft .WAV files                      */
#define TYPE_EOF	12
#define TYPE_GENERIC_ULAW 128
#define TYPE_GENERIC_ALAW 129

#define POSITIVE_ULAW_ZERO 0xff
#define NEGATIVE_ULAW_ZERO 0x7f

#ifndef	MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef	MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#if defined(unix) && !defined(linux)
#define labs abs
#endif

#define ROUNDEDSHIFTDOWN(x, n) (((n) == 0) ? (x) : ((x) >> ((n) - 1)) >> 1)

#ifndef M_LN2
#define	M_LN2	0.69314718055994530942
#endif

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

/* BUFSIZ must be a multiple of four to contain a whole number of words */
#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

#define putc_exit(val, stream)\
{ char rval;\
  if((rval = putc((val), (stream))) != (char) (val))\
    errx(1, "write failed: putc returns EOF\n");\
}

extern int getc_exit_val;
#define getc_exit(stream)\
(((getc_exit_val = getc(stream)) == EOF) ? \
  errx(1, "read failed: getc returns EOF\n"), 0: getc_exit_val)

#undef	uchar
#define uchar	unsigned char
#undef	ulong
#define ulong	unsigned long

/************************/
/* defined in shorten.c */
extern void	init_offset	(long**, int, int, int);
extern int	shorten		(FILE*, FILE*, int, char**);

/**************************/
/* defined in ualaw.c */
extern int Sulaw2lineartab[];
#define Sulaw2linear(i) (Sulaw2lineartab[i])
#ifndef Sulaw2linear
extern int	Sulaw2linear (uchar);
#endif
extern uchar	Slinear2ulaw (int);

extern int Salaw2lineartab[];
#define Salaw2linear(i) (Salaw2lineartab[i])
#ifndef Salaw2linear
extern int	Salaw2linear (uchar);
#endif
extern uchar	Slinear2alaw (int);

/**********************/
/* defined in fixio.c */
extern void	init_sizeof_sample (void);
extern void	fread_type_init (void);
extern int	fread_type (long**, int, int, int, FILE*, long*);
extern void	fread_type_quit (void);
extern void	fwrite_type_init (void);
extern void	fwrite_type (long**, int, int, int, FILE*);
extern void	fwrite_type_quit (void);
extern int	find_bitshift (long*, int, int);
extern void	fix_bitshift (long*, int, int, int);

/**********************/
/* defined in vario.c */
extern void	var_put_init (void);
extern void	uvar_put (ulong, int, FILE*);
extern void	var_put (long, int, FILE*);
extern void	ulong_put (uint32_t, FILE*);
extern void	var_put_quit (FILE*);

extern void	var_get_init (void);
extern uint32_t	uvar_get (int, FILE*);
extern int32_t	var_get (int, FILE*);
extern uint32_t	ulong_get (FILE*);
extern void	var_get_quit (void);

extern int	sizeof_uvar (ulong, int);
extern int	sizeof_var (long, int);

extern void	mkmasktab (void);
extern void	word_put (ulong, FILE*);
extern uint32_t	word_get (FILE*);

/********************/
/* defined in lpc.c */
extern int	wav2lpc (long*, int, long, int*,int,int,float*,float*);

/*********************/
/* defined in poly.c */
extern int	wav2poly (long*, int, long, int, float*, float*);

/*********************/
/* defined in exit.c */
extern void	basic_exit (int);
extern void	usage_exit (int, char*,...);

/**********************/
/* defined in array.c */
extern void* 	pmalloc (size_t);
extern long**	long2d (size_t, size_t);

/****************************/
/* defined in dupfileinfo.c */
extern int	dupfileinfo (char*, char*);

/*************************/
/* defined in riffwave.c */

typedef struct Riff_Wave_Header Riff_Wave_Header;
Riff_Wave_Header *riff_wave_prochdr (FILE *, int *, int *, long *, int *);
void              write_header (Riff_Wave_Header *, FILE *);
void              free_header (Riff_Wave_Header *);
void              verbatim_file (FILE *, FILE *);
