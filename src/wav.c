#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

struct chunkdesc {
	uint8_t id[5];
	uint32_t size; // Overall file size - 8 (little-endian)
	uint8_t fmt[5];
};

int main(int argc, char **argv)
{
	if (argc < 2)
		return EXIT_FAILURE;

	FILE *fp;
	size_t ret;
	struct chunkdesc header;

	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		perror("Could not open file\n");
		return EXIT_FAILURE;
	}

	// Read chunkID
	ret = fread(header.id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		return EXIT_FAILURE;
	}

	if (strncmp((const char *)header.id, "RIFF", 4) != 0) {
		perror("Not a WAV file (no RIFF header).\n");
		return EXIT_FAILURE;
	}
	header.id[4] = '\0';

	// Read file size - 8 (little-endian)
	ret = fread(&header.size, sizeof(header.size), 1, fp);
	if (ret != 1) {
		perror("Error reading chunk size\n");
		return EXIT_FAILURE;
	}
	header.size += 8;

	// Read file format
	ret = fread(header.fmt, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		return EXIT_FAILURE;
	}

	if (strncmp((const char *)header.fmt, "WAVE", 4) != 0) {
		perror("RIFF file is not a WAV format.\n");
		return EXIT_FAILURE;
	}
	header.fmt[4] = '\0';

	printf("ChunkID:'%s'\n", header.id);
	printf("File Size:%i bytes\n", header.size);
	printf("Format:'%s'\n", header.fmt);

	fclose(fp);
	return 0;
}
