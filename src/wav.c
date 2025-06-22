#include <asm-generic/errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOC_SIZE 44
#define FMT_CHUNKSIZE 24

struct descriptor {
	// 12 bytes long
	uint8_t id[4];
	uint32_t size; // Overall file size - 8 (little-endian)
	uint8_t fmt[4];
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

	if (chunk->size + 8 <= BLOC_SIZE) {
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

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3

#define MONO 1
#define STEREO 2

struct fmt {
	// 24 bytes long
	uint8_t id[4]; // 4 bytes
	uint32_t size; // 4 bytes
	uint16_t fmt; // 2 bytes
	uint16_t nchannels; // 2 bytes
	uint32_t sample_rate; // 4 bytes
	uint32_t byte_rate; // 4 bytes (SampleRate * BitsPerSample * channels) / 8
	uint16_t blk_align; // 2 bytes (BitsPerSample * Channels) / 8
	uint16_t bits_per_sample; // 2 bytes
};

struct fmt *unpack_fmt(FILE *fp)
{
	size_t ret;
	struct fmt *subchunk = malloc(sizeof(struct fmt));

	if (subchunk == NULL) {
		perror("Could not allocate memory.\n");
		exit(1);
	}

	// Read chunkID
	ret = fread(subchunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		exit(1);
	}

	if (strncmp((const char *)subchunk->id, "fmt ", 4) != 0) {
		perror("Invalid WAV. No `fmt` subchunk id.\n");
		exit(1);
	}

	ret = fread(&subchunk->size, 4, 1, fp);
	if (ret != 1) {
		perror("Error reading subchunk size in subchunk `fmt`.\n");
		exit(1);
	}

	if (subchunk->size + 8 != FMT_CHUNKSIZE) {
		fprintf(stderr, "Invalid subchunk size: %i\n", subchunk->size);
		exit(1);
	}

	ret = fread(&subchunk->fmt, 2, 1, fp);
	if (ret != 1) {
		perror("Error reading audio format in `fmt` subchunk.\n");
		exit(1);
	}

	if (subchunk->fmt != WAVE_FORMAT_PCM &&
	    subchunk->fmt != WAVE_FORMAT_IEEE_FLOAT) {
		fprintf(stderr, "Unsupported audio format: %i\n",
			subchunk->fmt);
		exit(1);
	}

	ret = fread(&subchunk->nchannels, 2, 1, fp);
	if (ret != 1) {
		perror("Error reading number of channels in `fmt` subchunk.\n");
		exit(1);
	}

	if (subchunk->nchannels != MONO && subchunk->nchannels != STEREO) {
		fprintf(stderr, "Invalid number of audio channels: %i.\n",
			subchunk->nchannels);
		exit(1);
	}

	ret = fread(&subchunk->sample_rate, 4, 1, fp);
	if (ret != 1) {
		perror("Error reading sample rate in `fmt` subchunk.\n");
		exit(1);
	}

	ret = fread(&subchunk->byte_rate, 4, 1, fp);
	if (ret != 1) {
		perror("Error reading byte rate in `fmt` subchunk.\n");
		exit(1);
	}

	ret = fread(&subchunk->blk_align, 2, 1, fp);
	if (ret != 1) {
		perror("Error reading block align in `fmt` subchunk.\n");
		exit(1);
	}

	ret = fread(&subchunk->bits_per_sample, 2, 1, fp);
	if (ret != 1) {
		perror("Error reading bits per sample in `fmt` subchunk.\n");
		exit(1);
	}

	if (subchunk->byte_rate * 8 !=
	    (subchunk->bits_per_sample * subchunk->nchannels *
	     subchunk->sample_rate)) {
		fprintf(stderr, "Invalid Byte Rate: %i\n", subchunk->byte_rate);
		exit(1);
	}

	if (subchunk->blk_align * 8 !=
	    subchunk->nchannels * subchunk->bits_per_sample) {
		fprintf(stderr, "Invalid Block Align: %i\n",
			subchunk->blk_align);
	}

	return subchunk;
}

struct data {
	// 8 bytes + size of data
	uint8_t id[4]; // 4 bytes
	uint32_t size; // 4 bytes
	unsigned char *stream;
};

struct data *unpack_data(FILE *fp)
{
	struct data *subchunk = malloc(sizeof(struct data));
	if (subchunk == NULL) {
		perror("Could not allocate memory in `data` subchunk.\n");
		exit(1);
	}

	size_t ret = fread(subchunk->id, 1, 4, fp);
	if (ret != 4) {
		perror("Error reading subchunk id in `data` subchunk.\n");
		exit(1);
	}

	if (strncmp((const char *)subchunk->id, "data", 4) != 0) {
		perror("Invalid WAV. No `data` subchunk.\n");
		exit(1);
	}

	ret = fread(&subchunk->size, 4, 1, fp);
	if (ret != 1) {
		perror("Error reading subchunk size in `data` subchunk.\n");
		exit(1);
	}

	if (subchunk->size < 1) {
		fprintf(stderr, "Invalid size: %i\n", subchunk->size);
		exit(1);
	}

	subchunk->stream = malloc(sizeof(unsigned char) * subchunk->size);
	if (subchunk->stream == NULL) {
		perror("Could not allocate memory for data stream.\n");
		exit(1);
	}
	ret = fread(subchunk->stream, 1, subchunk->size, fp);
	if (ret != subchunk->size) {
		perror("Error reading stream in `data` subchunk\n");
		exit(1);
	}

	return subchunk;
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
	struct data *subchunk2 = unpack_data(fp);

	free(chunk);
	free(subchunk1);
	free(subchunk2->stream);
	free(subchunk2);
	fclose(fp);
	return 0;
}
