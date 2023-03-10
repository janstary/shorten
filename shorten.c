/*
 * Copyright (C) 1992-1997 Tony Robinson
 * Copyright (C) 2023      Jan Starý
 */

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <utime.h>
#include <math.h>
#include <err.h>

#include "shorten.h"

char *readmode = "r";
char *writemode = "w";

#define SUFFIX ".shn"
#define SUFLEN (strlen(SUFFIX))

int getc_exit_val;
char *oname = NULL;
FILE *ofile = NULL;

#define V2LPCQOFFSET (1 << LPCQUANT);

#define UINT_PUT(val, nbit, file) \
  if(version == 0) uvar_put((unsigned long) val, nbit, file); \
  else ulong_put((unsigned long) val, file)

#define UINT_GET(nbit, file) \
  ((version == 0) ? uvar_get(nbit, file) : ulong_get(file))

#define VAR_PUT(val, nbit, file) \
  if(version == 0) var_put((unsigned long) val, nbit - 1, file); \
  else var_put((unsigned long) val, nbit, file)

void
init_offset(offset, nchan, nblock, ftype)
	long **offset;
	int nchan, nblock, ftype;
{
	long mean = 0;
	int chan, i;

	/* initialise offset */
	switch (ftype) {
	case TYPE_AU1:
	case TYPE_S8:
	case TYPE_S16HL:
	case TYPE_S16LH:
	case TYPE_ULAW:
	case TYPE_AU2:
	case TYPE_AU3:
	case TYPE_ALAW:
		mean = 0;
		break;
	case TYPE_U8:
		mean = 0x80;
		break;
	case TYPE_U16HL:
	case TYPE_U16LH:
		mean = 0x8000;
		break;
	default:
		errx(1, "unknown file type: %d\n", ftype);
	}

	for (chan = 0; chan < nchan; chan++)
		for (i = 0; i < nblock; i++)
			offset[chan][i] = mean;
}

float
Satof(char *string)
{
	float val = 0;
	char *e = NULL;

	val = strtod(string, &e);
	if (e && *e)
		errx(1, "error in float expression at %s", e);
	return val;
}

float *
parseList(char *maxresnstr, int nchan)
{
	int nval;
	char *str, *floatstr;
	float *floatval;

	str = strdup(maxresnstr);
	floatval = pmalloc(nchan * sizeof(*floatval));
	floatstr = strtok(str, ",");
	floatval[0] = Satof(floatstr);

	for (nval = 1; (floatstr = strtok(NULL, ",")) && nval < nchan; nval++)
		floatval[nval] = Satof(floatstr);

	for (; nval < nchan; nval++)
		floatval[nval] = floatval[nval - 1];

	free(str);
	return (floatval);
}

int
dupfileinfo(path0, path1)
	char *path0, *path1;
{
	int errcode;
	struct stat buf;

	errcode = stat(path0, &buf);
	if (!errcode) {
		struct utimbuf ftime;

		/* do what can be done, and igore errors */
		(void)chmod(path1, buf.st_mode);
		ftime.actime = buf.st_atime;
		ftime.modtime = buf.st_mtime;
		(void)utime(path1, &ftime);
		(void)chown(path1, buf.st_uid, -1);
		(void)chown(path1, -1, buf.st_gid);
	}
	return (errcode);
}

int
shorten(stdi, stdo, argc, argv)
	FILE *stdi, *stdo;
	int argc;
	char **argv;
{
	long **buffer, **offset;
	long lpcqoffset = 0;
	int version = FORMAT_VERSION, extract = 0, lastbitshift = 0, bitshift = 0;
	int hiloint = 1, hilo = !(*((char *)&hiloint));
	int ftype = TYPE_EOF;
	char *magic = MAGIC, *iname = NULL;
	char *maxresnstr = DEFAULT_MAXRESNSTR;
	FILE *ifile;
	int blocksize = DEFAULT_BLOCK_SIZE, nchan = DEFAULT_NCHAN;
	int i, chan, nwrap, nskip = DEFAULT_NSKIP, ndiscard = DEFAULT_NDISCARD;
	int *qlpc = NULL, maxnlpc = DEFAULT_MAXNLPC, nmean = UNDEFINED_UINT;
	int quanterror = DEFAULT_QUANTERROR, minsnr = DEFAULT_MINSNR, nfilename;
	int ulawZeroMerge = 0;
	long datalen = -1;
	Riff_Wave_Header *wavhdr = NULL;

	/* this block just processes the command line arguments */
	{
		int c;

		while ((c = getopt(argc, argv, "a:b:c:d:m:n:p:q:r:t:uv:x")) != -1)
			switch (c) {
			case 'a':
				if ((nskip = atoi(optarg)) < 0)
					errx(1, "number of bytes to copy must be positive\n");
				break;
			case 'b':
				if ((blocksize = atoi(optarg)) <= 0)
					errx(1, "block size must be greater than zero\n");
				break;
			case 'c':
				if ((nchan = atoi(optarg)) <= 0)
					errx(1, "number of channels must be greater than zero\n");
				break;
			case 'd':
				if ((ndiscard = atoi(optarg)) < 0)
					errx(1, "number of bytes to discard must be positive\n");
				break;
			case 'm':
				if ((nmean = atoi(optarg)) < 0)
					errx(1, "number of blocks for mean estimation must be positive\n");
				break;
			case 'n':
				if ((minsnr = atoi(optarg)) < 0)
					errx(1, "Useful signal to noise ratios are positive\n");
				break;
			case 'p':
				maxnlpc = atoi(optarg);
				if (maxnlpc < 0 || maxnlpc > MAX_LPC_ORDER)
					errx(1, "linear prediction order must be in the range 0 ... %d\n", MAX_LPC_ORDER);
				break;
case 'q':
				if ((quanterror = atoi(optarg)) < 0)
					errx(1, "quantisation level must be positive\n");
				break;
			case 'r':
				maxresnstr = optarg;
				break;
			case 't':
				if (!strcmp(optarg, "au"))
					ftype = TYPE_GENERIC_ULAW;
				else if (!strcmp(optarg, "ulaw"))
					ftype = TYPE_GENERIC_ULAW;
				else if (!strcmp(optarg, "alaw"))
					ftype = TYPE_GENERIC_ALAW;
				else if (!strcmp(optarg, "s8"))
					ftype = TYPE_S8;
				else if (!strcmp(optarg, "u8"))
					ftype = TYPE_U8;
				else if (!strcmp(optarg, "s16"))
					ftype = hilo ? TYPE_S16HL : TYPE_S16LH;
				else if (!strcmp(optarg, "u16"))
					ftype = hilo ? TYPE_U16HL : TYPE_U16LH;
				else if (!strcmp(optarg, "s16x"))
					ftype = hilo ? TYPE_S16LH : TYPE_S16HL;
				else if (!strcmp(optarg, "u16x"))
					ftype = hilo ? TYPE_U16LH : TYPE_U16HL;
				else if (!strcmp(optarg, "s16hl"))
					ftype = TYPE_S16HL;
				else if (!strcmp(optarg, "u16hl"))
					ftype = TYPE_U16HL;
				else if (!strcmp(optarg, "s16lh"))
					ftype = TYPE_S16LH;
				else if (!strcmp(optarg, "u16lh"))
					ftype = TYPE_U16LH;
				else if (!strcmp(optarg, "wav"))
					ftype = TYPE_RIFF_WAVE;
				else
					errx(1, "unknown file type: %s\n", optarg);
				break;
			case 'u':
				ulawZeroMerge = 1;
				break;
			case 'v':
				version = atoi(optarg);
				if (version < 0 || version > MAX_SUPPORTED_VERSION)
					errx(1, "currently supported versions are in the range %d ... %d\n",
						   MIN_SUPPORTED_VERSION, MAX_SUPPORTED_VERSION);
				break;
			case 'x':
				extract = 1;
				break;
default:
				errx(1, NULL);
				break;
			}
	}

	if (nmean == UNDEFINED_UINT)
		nmean = (version < 2) ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

	/* a few sanity checks */
	if (blocksize <= NWRAP)
		errx(1, "blocksize must be greater than %d\n", NWRAP);

	if (maxnlpc >= blocksize)
		errx(1, "the predictor order must be less than the block size\n");

	if (ulawZeroMerge == 1 && ftype != TYPE_GENERIC_ULAW)
		errx(1, "the -u flag is only applicable to ulaw coding\n");

	/* this chunk just sets up the input and output files */
	nfilename = argc - optind;
	switch (nfilename) {
	case 0:
		iname = strdup("-");
		oname = strdup("-");
		break;
	case 1:
		iname = argv[argc - 1];
		if (extract) {
			char *dot;
			oname = strdup(iname);
			if (((dot = strrchr(oname, '.')) == NULL)
			|| strncmp(dot, SUFFIX, SUFLEN))
				errx(1,"no %s in %s", SUFFIX, iname);
			*dot = '\0';
		} else {
			oname = calloc(1, FILENAME_MAX);
			strlcpy(oname, iname, FILENAME_MAX);
			strlcat(oname, SUFFIX, FILENAME_MAX);
		}
		break;
	case 2:
		iname = argv[argc - 2];
		oname = argv[argc - 1];
		break;
	default:
		errx(1, NULL);
	}

	if (strncmp(iname, "-", 1)) {
		if ((ifile = fopen(iname, readmode)) == NULL)
			err(1, "%s", iname);
	} else {
		ifile = stdi;
	}

	if (strncmp(oname, "-", 1)) {
		if ((ofile = fopen(oname, writemode)) == NULL)
			err(1, "%s", oname);
	} else {
		ofile = stdo;
	}

	/* discard header on input file - can't rely on fseek() here */
	if (ndiscard != 0) {
		char discardbuf[BUFSIZ];

		for (i = 0; i < ndiscard / BUFSIZ; i++)
			if (fread(discardbuf, BUFSIZ, 1, ifile) != 1)
				errx(1, "EOF on input when discarding header\n");

		if (ndiscard % BUFSIZ != 0)
			if (fread(discardbuf, ndiscard % BUFSIZ, 1, ifile) != 1)
				errx(1, "EOF on input when discarding header\n");
	}
	if (!extract) {
		float *maxresn;
		int nread, nscan = 0, vbyte = MAX_VERSION + 1;

		/* define type to be RIFF WAVE if not already committed */
		if (ftype == TYPE_EOF)
			ftype = TYPE_RIFF_WAVE;

		/* If we're dealing with RIFF WAVE, process the header now,
		 * before calling fread_type_init() which will want to know
		 * the sample type. This also resets ftype to the _real_
		 * sample type, and nchan to the correct number of channels,
		 * and prepares a verbatim section containing the header,
		 * which we will write out after the filetype and channel
		 * count. */
		if (ftype == TYPE_RIFF_WAVE) {
			int wtype;
			wavhdr = riff_wave_prochdr(ifile, &ftype, &nchan, &datalen, &wtype);
			if (wavhdr == NULL) {
				if (wtype == 0)
					/* the header must have been invalid */
					errx(1, "input file is not a valid RIFF WAVE file\n");
				else
					/* the wave type is wrong */
					errx(1, "RIFF WAVE file has unhandled format tag %d\n", wtype);
			} else {
				/* we have a valid RIFF WAVE so override
				 * anything the user may have said to do with
				 * the alignment */
				nskip = 0;
			}
		}
		/* sort out specific types for ULAW and ALAW */
		if (ftype == TYPE_GENERIC_ULAW || ftype == TYPE_GENERIC_ALAW) {
			int linearLossy = (Satof(maxresnstr) != Satof(DEFAULT_MAXRESNSTR)
				       || quanterror != DEFAULT_QUANTERROR);

			/* sort out which ulaw type we are going to use */
			if (ftype == TYPE_GENERIC_ULAW) {
				if (linearLossy)
					ftype = TYPE_ULAW;
				else if (version < 2 || ulawZeroMerge == 1)
					ftype = TYPE_AU1;
				else
					ftype = TYPE_AU2;
			}
			/* sort out which alaw type we are going to use */
			if (ftype == TYPE_GENERIC_ALAW) {
				if (linearLossy)
					ftype = TYPE_ALAW;
				else
					ftype = TYPE_AU3;
			}
		}
		/* mean compensation is not supported for TYPE_AU1 or
		 * TYPE_AU2 */
		/* (the bit shift compensation can't cope with the lag
		 * vector) */
		if (ftype == TYPE_AU1 || ftype == TYPE_AU2)
			nmean = 0;

		nwrap = MAX(NWRAP, maxnlpc);

		if (maxnlpc > 0)
			qlpc = (int *) pmalloc((maxnlpc * sizeof(*qlpc)));

		/* verbatim copy of skip bytes from input to output checking
		 * for the existence of magic number in header, and
		 * defaulting to internal storage if that happens */
		if (version >= 2) {
			while (nskip - nscan > 0 && vbyte > MAX_VERSION) {
				int byte = getc_exit(ifile);
				if (magic[nscan] != '\0' && byte == magic[nscan])
					nscan++;
				else if (magic[nscan] == '\0' && byte <= MAX_VERSION)
					vbyte = byte;
				else {
					for (i = 0; i < nscan; i++)
						putc_exit(magic[i], ofile);
					if (byte == magic[0]) {
						nskip -= nscan;
						nscan = 1;
					} else {
						putc_exit(byte, ofile);
						nskip -= nscan + 1;
						nscan = 0;
					}
				}
			}
			if (vbyte > MAX_VERSION) {
				for (i = 0; i < nscan; i++)
					putc_exit(magic[i], ofile);
				nskip -= nscan;
				nscan = 0;
			}
		}
		/* write magic number */
		if (fwrite(magic, strlen(magic), 1, ofile) != 1)
			errx(1, "could not write the magic number\n");

		/* write version number */
		putc_exit(version, ofile);

		/* grab some space for the input buffers */
		buffer = long2d(nchan, blocksize + nwrap);
		offset = long2d(nchan, MAX(1, nmean));

		maxresn = parseList(maxresnstr, nchan);
		for (chan = 0; chan < nchan; chan++)
			if (maxresn[chan] < MINBITRATE)
				errx(1, "channel %d: expected bit rate must be >= %3.1f: %3.1f\n",
					   chan, MINBITRATE, maxresn[chan]);
			else
				maxresn[chan] -= 3.0;

		for (chan = 0; chan < nchan; chan++) {
			for (i = 0; i < nwrap; i++)
				buffer[chan][i] = 0;
			buffer[chan] += nwrap;
		}

		/* initialise the fixed length file read for the uncompressed
		 * stream */
		fread_type_init();

		/* initialise the variable length file write for the
		 * compressed stream */
		var_put_init();

		/* put file type and number of channels */
		UINT_PUT(ftype, TYPESIZE, ofile);
		UINT_PUT(nchan, CHANSIZE, ofile);

		/* put blocksize if version > 0 */
		if (version == 0) {
			if (blocksize != DEFAULT_BLOCK_SIZE) {
				uvar_put((ulong) FN_BLOCKSIZE, FNSIZE, ofile);
				UINT_PUT(blocksize, (int)(log((double)DEFAULT_BLOCK_SIZE) / M_LN2),
					 ofile);
			}
		} else {
			UINT_PUT(blocksize, (int)(log((double)DEFAULT_BLOCK_SIZE) / M_LN2),
				 ofile);
			UINT_PUT(maxnlpc, LPCQSIZE, ofile);
			UINT_PUT(nmean, 0, ofile);
			UINT_PUT(nskip, NSKIPSIZE, ofile);
			if (version == 1) {
				for (i = 0; i < nskip; i++) {
					int byte = getc_exit(ifile);
					uvar_put((ulong) byte, XBYTESIZE, ofile);
				}
			} else {
				if (vbyte <= MAX_VERSION) {
					for (i = 0; i < nscan; i++)
						uvar_put((ulong) magic[i], XBYTESIZE, ofile);
					uvar_put((ulong) vbyte, XBYTESIZE, ofile);
				}
				for (i = 0; i < nskip - nscan - 1; i++) {
					int byte = getc_exit(ifile);
					uvar_put((ulong) byte, XBYTESIZE, ofile);
				}
				lpcqoffset = V2LPCQOFFSET;
			}
		}

		/* if we had a RIFF WAVE header, write it out in the form of
		 * verbatim chunks at this point */
		if (wavhdr)
			write_header(wavhdr, ofile);

		init_offset(offset, nchan, MAX(1, nmean), ftype);

		/* this is the main read/code/write loop for the whole file */
		while ((nread = fread_type(buffer, ftype, nchan,
					blocksize, ifile, &datalen)) != 0) {

			/* put blocksize if changed */
			if (nread != blocksize) {
				uvar_put((ulong) FN_BLOCKSIZE, FNSIZE, ofile);
				UINT_PUT(nread, (int)(log((double)blocksize) / M_LN2), ofile);
				blocksize = nread;
			}
			/* loop over all channels, processing each channel in
			 * turn */
			for (chan = 0; chan < nchan; chan++) {
				float sigbit;	/* PT expected root mean
						 * squared value of the
						 * signal */
				float resbit;	/* PT expected root mean
						 * squared value of the
						 * residual */
				long coffset, *cbuffer = buffer[chan],
				 fulloffset = 0L;
				int fnd, resn = 0, nlpc = 0;

				/* force the lower quanterror bits to be zero */
				if (quanterror != 0) {
					long offset = (1L << (quanterror - 1));
					for (i = 0; i < blocksize; i++)
						cbuffer[i] = (cbuffer[i] + offset) >> quanterror;
				}
				/* merge both ulaw zeros if required */
				if (ulawZeroMerge == 1)
					for (i = 0; i < blocksize; i++)
						if (cbuffer[i] == NEGATIVE_ULAW_ZERO)
							cbuffer[i] = POSITIVE_ULAW_ZERO;

				/* test for excessive and exploitable
				 * quantisation, and exploit!! */
				bitshift = find_bitshift(cbuffer, blocksize, ftype) + quanterror;
				if (bitshift > NBITS)
					bitshift = NBITS;

				/* find mean offset : N.B. this code
				 * duplicated */
				if (nmean == 0)
					fulloffset = coffset = offset[chan][0];
				else {
					long sum = (version < 2) ? 0 : nmean / 2;
					for (i = 0; i < nmean; i++)
						sum += offset[chan][i];
					if (version < 2)
						coffset = sum / nmean;
					else {
						fulloffset = sum / nmean;
						if (bitshift == NBITS && version >= 2)
							coffset = ROUNDEDSHIFTDOWN(fulloffset, lastbitshift);
						else
							coffset = ROUNDEDSHIFTDOWN(fulloffset, bitshift);
					}
				}

				/* find the best model */
				if (bitshift == NBITS && version >= 2) {
					bitshift = lastbitshift;
					fnd = FN_ZERO;
				} else {
					int maxresnbitshift, snrbitshift,
					 extrabitshift;
					float sigpow, nn;

					if (maxnlpc == 0)
						fnd = wav2poly(cbuffer, blocksize, coffset, version, &sigbit, &resbit);
					else {
						nlpc = wav2lpc(cbuffer, blocksize, coffset, qlpc, maxnlpc, version,
							  &sigbit, &resbit);
						fnd = FN_QLPC;
					}

					if (resbit > 0.0)
						resn = floor(resbit + 0.5);
					else
						resn = 0;

					maxresnbitshift = floor(resbit - maxresn[chan] + 0.5);
					sigpow = exp(2.0 * M_LN2 * sigbit) / (0.5 * M_LN2 * M_LN2);
					nn = 12.0 * sigpow / pow(10.0, minsnr / 10.0);
					snrbitshift = (nn > 25.0 / 12.0) ? floor(0.5 * log(nn - 25.0 / 12.0) / M_LN2) : 0;
					extrabitshift = MAX(maxresnbitshift, snrbitshift);

					if (extrabitshift > resn)
						extrabitshift = resn;

					if (extrabitshift > 0) {
						long offset = (1L << (extrabitshift - 1));
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = (cbuffer[i] + offset) >> extrabitshift;
						bitshift += extrabitshift;
						if (version >= 2)
							coffset = ROUNDEDSHIFTDOWN(fulloffset, bitshift);
						resn -= extrabitshift;
					}
				}

				/* store mean value if appropriate : N.B.
				 * Duplicated code */
				if (nmean > 0) {
					long sum = (version < 2) ? 0 : blocksize / 2;

					for (i = 0; i < blocksize; i++)
						sum += cbuffer[i];

					for (i = 1; i < nmean; i++)
						offset[chan][i - 1] = offset[chan][i];
					if (version < 2)
						offset[chan][nmean - 1] = sum / blocksize;
					else
						offset[chan][nmean - 1] = (sum / blocksize) << bitshift;
				}
				if (bitshift != lastbitshift) {
					uvar_put((ulong) FN_BITSHIFT, FNSIZE, ofile);
					uvar_put((ulong) bitshift, BITSHIFTSIZE, ofile);
					lastbitshift = bitshift;
				}
				if (fnd == FN_ZERO) {
					uvar_put((ulong) fnd, FNSIZE, ofile);
				} else if (maxnlpc == 0) {
					uvar_put((ulong) fnd, FNSIZE, ofile);
					uvar_put((ulong) resn, ENERGYSIZE, ofile);

					switch (fnd) {
					case FN_DIFF0:
						for (i = 0; i < blocksize; i++)
							VAR_PUT(cbuffer[i] - coffset, resn, ofile);
						break;
					case FN_DIFF1:
						for (i = 0; i < blocksize; i++)
							VAR_PUT(cbuffer[i] - cbuffer[i - 1], resn, ofile);
						break;
					case FN_DIFF2:
						for (i = 0; i < blocksize; i++)
							VAR_PUT(cbuffer[i] - 2 * cbuffer[i - 1] + cbuffer[i - 2],
								resn, ofile);
						break;
					case FN_DIFF3:
						for (i = 0; i < blocksize; i++)
							VAR_PUT(cbuffer[i] - 3 * (cbuffer[i - 1] - cbuffer[i - 2]) -
								cbuffer[i - 3], resn, ofile);
						break;
					}
				} else {
					uvar_put((ulong) FN_QLPC, FNSIZE, ofile);
					uvar_put((ulong) resn, ENERGYSIZE, ofile);
					uvar_put((ulong) nlpc, LPCQSIZE, ofile);
					for (i = 0; i < nlpc; i++)
						var_put((long)qlpc[i], LPCQUANT, ofile);

					/* deduct mean from everything */
					for (i = -nlpc; i < blocksize; i++)
						cbuffer[i] -= coffset;

					/* use the quantised LPC coefficients
					 * to generate the residual */
					for (i = 0; i < blocksize; i++) {
						int j;
						long sum = lpcqoffset;
						long *obuffer = &(cbuffer[i - 1]);

						for (j = 0; j < nlpc; j++)
							sum += qlpc[j] * obuffer[-j];
						var_put(cbuffer[i] - (sum >> LPCQUANT), resn, ofile);
					}

					/* add mean back to those samples
					 * that will be wrapped */
					for (i = blocksize - nwrap; i < blocksize; i++)
						cbuffer[i] += coffset;
				}

				/* do the wrap */
				for (i = -nwrap; i < 0; i++)
					cbuffer[i] = cbuffer[i + blocksize];
			}
		}

		/* if we had a RIFF WAVE header, we had better be prepared to
		 * deal with a RIFF WAVE footer too... */
		if (wavhdr)
			verbatim_file(ifile, ofile);

		/* wind up */
		fread_type_quit();
		uvar_put((ulong) FN_QUIT, FNSIZE, ofile);
		var_put_quit(ofile);

		/* and free the space used */
		if (wavhdr)
			free_header(wavhdr);
		free((char *)buffer);
		free((char *)offset);
		if (maxnlpc > 0)
			free((char *)qlpc);
	} else {
		/***********************/
		/* EXTRACT starts here */
		/***********************/

		int i, cmd;
		int internal_ftype;

		/* Firstly skip the number of bytes requested in the command
		 * line */
		for (i = 0; i < nskip; i++) {
			int byte = getc(ifile);
			if (byte == EOF)
				errx(1, "File too short for requested alignment\n");
			putc_exit(byte, ofile);
		}

		/* read magic number */
		{
			int nscan = 0;

			version = MAX_VERSION + 1;
			while (version > MAX_VERSION) {
				int byte = getc(ifile);
				if (byte == EOF)
					errx(1, "No magic number\n");
				if (magic[nscan] != '\0' && byte == magic[nscan])
					nscan++;
				else if (magic[nscan] == '\0' && byte <= MAX_VERSION)
					version = byte;
				else {
					for (i = 0; i < nscan; i++)
						putc_exit(magic[i], ofile);
					if (byte == magic[0])
						nscan = 1;
					else {
						putc_exit(byte, ofile);
						nscan = 0;
					}
					version = MAX_VERSION + 1;
				}
			}
		}

		/* check version number */
		if (version > MAX_SUPPORTED_VERSION)
			errx(1, "can't decode version %d\n", version);

		/* set up the default nmean, ignoring the command line state */
		nmean = (version < 2) ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

		/* initialise the variable length file read for the
		 * compressed stream */
		var_get_init();

		/* initialise the fixed length file write for the
		 * uncompressed stream */
		fwrite_type_init();

		/* get the internal file type */
		internal_ftype = UINT_GET(TYPESIZE, ifile);

		/* has the user requested a change in file type? */
		if (internal_ftype != ftype) {
			if (ftype == TYPE_EOF) {
				ftype = internal_ftype;	/* no problems here */
			} else {/* check that the requested conversion is
				 * valid */
				if (internal_ftype == TYPE_AU1 || internal_ftype == TYPE_AU2 ||
				    internal_ftype == TYPE_AU3 || ftype == TYPE_AU1 ||
				    ftype == TYPE_AU2 || ftype == TYPE_AU3)
					errx(1, "Not able to perform requested output format conversion\n");
			}
		}
		nchan = UINT_GET(CHANSIZE, ifile);

		/* get blocksize if version > 0 */
		if (version > 0) {
			blocksize = UINT_GET((int)(log((double)DEFAULT_BLOCK_SIZE) / M_LN2),
					     ifile);
			maxnlpc = UINT_GET(LPCQSIZE, ifile);
			nmean = UINT_GET(0, ifile);
			nskip = UINT_GET(NSKIPSIZE, ifile);
			for (i = 0; i < nskip; i++) {
				int byte = uvar_get(XBYTESIZE, ifile);
				putc_exit(byte, ofile);
			}
		} else
			blocksize = DEFAULT_BLOCK_SIZE;

		nwrap = MAX(NWRAP, maxnlpc);

		/* grab some space for the input buffer */
		buffer = long2d(nchan, blocksize + nwrap);
		offset = long2d(nchan, MAX(1, nmean));

		for (chan = 0; chan < nchan; chan++) {
			for (i = 0; i < nwrap; i++)
				buffer[chan][i] = 0;
			buffer[chan] += nwrap;
		}

		if (maxnlpc > 0)
			qlpc = (int *) pmalloc((maxnlpc * sizeof(*qlpc)));

		if (version > 1)
			lpcqoffset = V2LPCQOFFSET;

		init_offset(offset, nchan, MAX(1, nmean), internal_ftype);

		/* get commands from file and execute them */
		chan = 0;
		while ((cmd = uvar_get(FNSIZE, ifile)) != FN_QUIT) {
			switch (cmd) {
			case FN_ZERO:
			case FN_DIFF0:
			case FN_DIFF1:
			case FN_DIFF2:
			case FN_DIFF3:
			case FN_QLPC:{
					long coffset, *cbuffer = buffer[chan];
					int resn = 0, nlpc, j;

					if (cmd != FN_ZERO) {
						resn = uvar_get(ENERGYSIZE, ifile);
						/* this is a hack as version
						 * 0 differed in definition
						 * of var_get */
						if (version == 0)
							resn--;
					}
					/* find mean offset : N.B. this code
					 * duplicated */
					if (nmean == 0)
						coffset = offset[chan][0];
					else {
						long sum = (version < 2) ? 0 : nmean / 2;
						for (i = 0; i < nmean; i++)
							sum += offset[chan][i];
						if (version < 2)
							coffset = sum / nmean;
						else
							coffset = ROUNDEDSHIFTDOWN(sum / nmean, bitshift);
					}

					switch (cmd) {
					case FN_ZERO:
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = 0;
						break;
					case FN_DIFF0:
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = var_get(resn, ifile) + coffset;
						break;
					case FN_DIFF1:
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = var_get(resn, ifile) + cbuffer[i - 1];
						break;
					case FN_DIFF2:
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = var_get(resn, ifile) + (2 * cbuffer[i - 1] -
							    cbuffer[i - 2]);
						break;
					case FN_DIFF3:
						for (i = 0; i < blocksize; i++)
							cbuffer[i] = var_get(resn, ifile) + 3 * (cbuffer[i - 1] -
												 cbuffer[i - 2]) + cbuffer[i - 3];
						break;
					case FN_QLPC:
						nlpc = uvar_get(LPCQSIZE, ifile);

						for (i = 0; i < nlpc; i++)
							qlpc[i] = var_get(LPCQUANT, ifile);
						for (i = 0; i < nlpc; i++)
							cbuffer[i - nlpc] -= coffset;
						for (i = 0; i < blocksize; i++) {
							long sum = lpcqoffset;

							for (j = 0; j < nlpc; j++)
								sum += qlpc[j] * cbuffer[i - j - 1];
							cbuffer[i] = var_get(resn, ifile) + (sum >> LPCQUANT);
						}
						if (coffset != 0)
							for (i = 0; i < blocksize; i++)
								cbuffer[i] += coffset;
						break;
					}

					/* store mean value if appropriate :
					 * N.B. Duplicated code */
					if (nmean > 0) {
						long sum = (version < 2) ? 0 : blocksize / 2;

						for (i = 0; i < blocksize; i++)
							sum += cbuffer[i];

						for (i = 1; i < nmean; i++)
							offset[chan][i - 1] = offset[chan][i];
						if (version < 2)
							offset[chan][nmean - 1] = sum / blocksize;
						else
							offset[chan][nmean - 1] = (sum / blocksize) << bitshift;
					}
					/* do the wrap */
					for (i = -nwrap; i < 0; i++)
						cbuffer[i] = cbuffer[i + blocksize];

					fix_bitshift(cbuffer, blocksize, bitshift, internal_ftype);

					if (chan == nchan - 1)
						fwrite_type(buffer, ftype, nchan, blocksize, ofile);
					chan = (chan + 1) % nchan;
				}
				break;
			case FN_BLOCKSIZE:
				blocksize = UINT_GET((int)(log((double)blocksize) / M_LN2), ifile);
				break;
			case FN_BITSHIFT:
				bitshift = uvar_get(BITSHIFTSIZE, ifile);
				break;
			case FN_VERBATIM:{
					int cklen = uvar_get(VERBATIM_CKSIZE_SIZE, ifile);
					while (cklen--)
						fputc(uvar_get(VERBATIM_BYTE_SIZE, ifile), ofile);
				}
				break;
			default:
				errx(1, "sanity check fails trying to decode function: %d\n", cmd);
			}
		}
		/* wind up */
		var_get_quit();
		fwrite_type_quit();

		free((char *)buffer);
		free((char *)offset);
		if (maxnlpc > 0)
			free((char *)qlpc);
	}

	/* close the files if this function opened them */
	if (ifile != stdi)
		fclose(ifile);
	if (ofile != stdo)
		fclose(ofile);

	/* make the compressed file look like the original if possible */
	if ((ifile != stdi) && (ofile != stdo))
		(void)dupfileinfo(iname, oname);

	if (nfilename == 1)
		if (unlink(iname))
			err(1, "%s", iname);

	return (0);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	return (shorten(stdin, stdout, argc, argv));
}
