#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lfo.h"

#define BUFSIZE 1024
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

char *program_name;

char* shape_names[] = {"sin", "tri", "saw", "sqr"};

void
usage ()
{
	fprintf(stderr, "Usage: %s nsamples del att spd shape mod\n",
	        program_name);
	exit(1);
}

float
parse_float (char *s, char *n)
{
	float f;
	if (sscanf(s, "%f", &f) == 1) {
		return f;
	} else {
		fprintf(stderr, "%s: Could not parse %s\n", program_name, n);
		usage();
		return 0;
	}
}

int
parse_int (char *s, char *n)
{
	int i;
	if (sscanf(s, "%d", &i) == 1) {
		return i;
	} else {
		fprintf(stderr, "%s: Could not parse %s\n", program_name, n);
		usage();
		return 0;
	}
}

float
parse_shape (char *s, char *n)
{
	int i;
	for (i=0; i<ARRAY_SIZE(shape_names); ++i) {
		if (!strcmp(s, shape_names[i])) {
			return i;
		}
	}

	fprintf(stderr, "%s: %s must be one of:", program_name, n);
	for (i=0; i<ARRAY_SIZE(shape_names); ++i) {
		fputc(' ', stderr);
		fputs(shape_names[i], stderr);
	}
	fputc('\n', stderr);
	usage();
	return 0;
}

int
main (int argc, char **argv)
{
	float outbuf[BUFSIZE];
	float del, att, spd, shape, mod, op;
	int nsamples, s, i;

	program_name = argv[0];

	if (argc != 7) {
		usage();
	}

	nsamples = parse_int(argv[1], "nsamples");
	del = parse_float(argv[2], "del");
	att = parse_float(argv[3], "att");
	spd = parse_float(argv[4], "spd");
	shape = parse_shape(argv[5], "shape");
	mod = parse_float(argv[6], "mod");
	op = 0.0f;

	if (!nsamples) {
		fprintf(stderr, "%s: nsamples must be nonzero\n", program_name);
		usage();
	}
	
	// Seconds * samples/second = samples
	LfoParams p = {20.0f * 48000.0f, &del, &att, &spd, &shape, &mod, &op};
	Lfo *lfo = lfo_create(&p);

	lfo_trigger(lfo);
	
	for (s = 0; s < nsamples;) {
		// Blank buffer
		for (i=0; i<BUFSIZE; ++i)
			outbuf[i]  = 0.0f;

		// Run
		lfo_run(lfo, outbuf, BUFSIZE);
		
		// Write
		for (i = 0; i < BUFSIZE && s < nsamples; ++i, ++s)
			printf("%d %f\n", s, outbuf[i]);
	}

	lfo_destroy(lfo);
	return EXIT_SUCCESS;
}
