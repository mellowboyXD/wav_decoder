#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

struct descriptor {
	// 12 bytes long
	uint8_t id[4];
	uint32_t size; // Overall file size - 8 (little-endian)
	uint8_t fmt[4];
};

struct fmt {
	// 24 bytes long
	uint8_t id[4]; // 4 bytes
	uint32_t size; // 4 bytes
	uint8_t fmt[2]; // 2 bytes
	uint16_t channels; // 2 bytes
	uint32_t sample_rate; // 4 bytes
	uint32_t byte_rate; // 4 bytes
	uint16_t blk_align; // 2 bytes
	uint16_t bits_per_sample; // 2 bytes
};

struct descriptor *unpack_desc(FILE *fp)
{
	size_t ret;
	struct descriptor *chunk = malloc(sizeof(struct descriptor));

	if (chunk == NULL) {
		perror("Could not allocate memory.\n");
		exit(1);
	}

	// Read chunkID
	ret = fread(chunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		exit(1);
	}

	if (strncmp((const char *)chunk->id, "RIFF", 4) != 0) {
		perror("Not a WAV file (no RIFF header).\n");
		exit(1);
	}

	// Read file size - 8 (little-endian)
	ret = fread(&chunk->size, sizeof(chunk->size), 1, fp);
	if (ret != 1) {
		perror("Error reading chunk size\n");
		exit(1);
	}

	if (chunk->size + 8 <= 44) {
		perror("Not a valid WAV file\n");
		exit(1);
	}
	chunk->size += 8;

	// Read file format
	ret = fread(chunk->fmt, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading format: %zu\n", ret);
		exit(1);
	}

	if (strncmp((const char *)chunk->fmt, "WAVE", 4) != 0) {
		perror("RIFF file is not a WAV format.\n");
		exit(1);
	}

	return chunk;
}

struct fmt *unpack_fmt(FILE *fp)
{
	size_t ret;
	struct fmt *chunk = malloc(sizeof(struct fmt));

	if (chunk == NULL) {
		perror("Could not allocate memory.\n");
		exit(1);
	}

	// Read chunkID
	ret = fread(chunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		exit(1);
	}

	if (strncmp((const char *)chunk->id, "fmt ", 4) != 0) {
		perror("No `fmt` subchunk id.\n");
		exit(1);
	}

	return chunk;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 1;

	FILE *fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		perror("Could not open file\n");
		return 1;
	}

	struct descriptor *chunk = unpack_desc(fp);
	struct fmt *subchunk1 = unpack_fmt(fp);
	printf("SubChunkID: `%c%c%c%c`\n", subchunk1->id[0], subchunk1->id[1],
	       subchunk1->id[2], subchunk1->id[3]);

	free(chunk);
	free(subchunk1);
	fclose(fp);
	return 0;
}
