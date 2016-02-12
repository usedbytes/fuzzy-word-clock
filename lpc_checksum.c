/* LPC binary image checksum generator
 * Writes a checksum to the interrupt vector for validation by the LPC ISP
 * bootloader.
 *
 * Written by Brian Starkey <stark3y@gmail.com>
 * Public domain. Provided as-is with no warranty whatsoever.
 */
#include <stdio.h>
#include <stdint.h>

uint32_t calculate_checksum(uint32_t *vector, size_t n_elems)
{
	int i;
	uint32_t checksum = 0;

	for (i = 0; i < n_elems; i++) {
		checksum += vector[i];
	}

	return ~checksum + 1;
}

int main(int argc, char *argv[])
{
	int res, i;
	FILE *in, *out = NULL;
	uint32_t checksum, vector[7];

	if ((argc < 2) || (argc > 3)) {
		printf("Usage: %s input_file [output_file]\n"
			   "\n"
			   "Adds a checksum to a binary image for NXP Cortex-M0 microcontrollers\n"
			   "The checksum makes the sum of all the entries in the vector\n"
			   "table equal 0\n"
			   "Arguments:\n"
			   " input_file: The binary file to calculate the checksum for\n"
			   " output_file: If provided, write the checksum to a copy of the\n"
			   "              input file, using this filename\n"
			   "Note: If output file is not provided, input_file will be modified\n"
			   "      in place.\n", argv[0]);
			   return 1;
	}

	if (argc == 3) {
		in = fopen(argv[1], "r");
		if (!in) {
			fprintf(stderr, "Couldn't open input file for reading\n");
			return 1;
		}
		out = fopen(argv[2], "w");
		if (!out) {
			fprintf(stderr, "Couldn't open output file\n");
			fclose(in);
			return 1;
		}
	} else {
		in = fopen(argv[1], "r+");
		if (!in) {
			fprintf(stderr, "Couldn't open input file\n");
			return 1;
		}
	}
	fseek(in, 0, SEEK_SET);

	res = fread(vector, 4, 7, in);
	if (res != 7) {
		fprintf(stderr, "Couldn't read vector\n");
		res = 1;
		goto exit;
	}
	checksum = calculate_checksum(vector, 7);
	printf("Checksum: 0x%08x\n", checksum);

	if (out) {
		char c;
		fwrite(vector, 4, 7, out);
		fwrite(&checksum, 4, 1, out);
		/* Throw away the old checksum word */
		fread(&checksum, 4, 1, in);
		while (fread(&c, 1, 1, in) == 1)
			fwrite(&c, 1, 1, out);
	} else {
		fseek(in, 0x1c, SEEK_SET);
		fwrite(&checksum, 4, 1, in);
	}

	res = 0;

exit:
	fclose(in);
	if (out)
		fclose(out);
	return res;
}

