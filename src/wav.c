#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>

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
		fprintf(stderr, "Could not allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Read chunkID
	ret = fread(chunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		exit(EXIT_FAILURE);
	}

	if (strncmp((const char *)chunk->id, "RIFF", 4) != 0) {
		fprintf(stderr, "Not a WAV file (no RIFF header).\n");
		exit(EXIT_FAILURE);
	}

	// Read file size - 8 (little-endian)
	ret = fread(&chunk->size, sizeof(chunk->size), 1, fp);
	if (ret != 1) {
		fprintf(stderr, "Error reading chunk size\n");
		exit(EXIT_FAILURE);
	}

	if (chunk->size + 8 <= BLOC_SIZE) {
		fprintf(stderr,
			"Not a valid WAV file. Chunk size is too small\n");
		exit(EXIT_FAILURE);
	}

	// Read file format
	ret = fread(chunk->fmt, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading format: %zu\n", ret);
		exit(EXIT_FAILURE);
	}

	if (strncmp((const char *)chunk->fmt, "WAVE", 4) != 0) {
		fprintf(stderr, "RIFF file is not a WAV format.\n");
		exit(EXIT_FAILURE);
	}

	return chunk;
}

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3

#define MONO 1
#define STEREO 2

struct fmt {
	// 24 bytes long
	uint8_t id[4]; // 4 bytes FormatBlocID
	uint32_t size; // 4 bytes BlocSize
	uint16_t fmt; // 2 bytes
	uint16_t nchannels; // 2 bytes
	uint32_t sample_rate; // 4 bytes
	uint32_t byte_rate; // 4 bytes (SampleRate * BitsPerSample * channels) / 8
	uint16_t blk_align; // 2 bytes (BitsPerSample * Channels) / 8 (Frames)
	uint16_t bits_per_sample; // 2 bytes
};

struct fmt *unpack_fmt(FILE *fp)
{
	size_t ret;
	struct fmt *subchunk = malloc(sizeof(struct fmt));
	if (subchunk == NULL) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Read chunkID
	ret = fread(subchunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr, "Error reading: %zu\n", ret);
		exit(EXIT_FAILURE);
	}

	if (strncmp((const char *)subchunk->id, "fmt ", 4) != 0) {
		fprintf(stderr, "Invalid WAV. No `fmt` subchunk id.\n");
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->size, 4, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading subchunk size in subchunk `fmt`.\n");
		exit(EXIT_FAILURE);
	}

	if (subchunk->size + 8 != FMT_CHUNKSIZE) {
		fprintf(stderr, "Invalid subchunk size: %i\n", subchunk->size);
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->fmt, 2, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading audio format in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	if (subchunk->fmt != WAVE_FORMAT_PCM &&
	    subchunk->fmt != WAVE_FORMAT_IEEE_FLOAT) {
		fprintf(stderr, "Unsupported audio format: %i\n",
			subchunk->fmt);
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->nchannels, 2, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading number of channels in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	if (subchunk->nchannels != MONO && subchunk->nchannels != STEREO) {
		fprintf(stderr, "Invalid number of audio channels: %i.\n",
			subchunk->nchannels);
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->sample_rate, 4, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading sample rate in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->byte_rate, 4, 1, fp);
	if (ret != 1) {
		fprintf(stderr, "Error reading byte rate in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->blk_align, 2, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading block align in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->bits_per_sample, 2, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading bits per sample in `fmt` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	if (subchunk->byte_rate * 8 !=
	    (subchunk->bits_per_sample * subchunk->nchannels *
	     subchunk->sample_rate)) {
		fprintf(stderr, "Invalid Byte Rate: %i\n", subchunk->byte_rate);
		exit(EXIT_FAILURE);
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
		fprintf(stderr,
			"Could not allocate memory in `data` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	size_t ret = fread(subchunk->id, 1, 4, fp);
	if (ret != 4) {
		fprintf(stderr,
			"Error reading subchunk id in `data` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	if (strncmp((const char *)subchunk->id, "data", 4) != 0) {
		fprintf(stderr, "Invalid WAV. No `data` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	ret = fread(&subchunk->size, 4, 1, fp);
	if (ret != 1) {
		fprintf(stderr,
			"Error reading subchunk size in `data` subchunk.\n");
		exit(EXIT_FAILURE);
	}

	if (subchunk->size < 1) {
		fprintf(stderr, "Invalid size: %i\n", subchunk->size);
		exit(EXIT_FAILURE);
	}

	subchunk->stream = malloc(sizeof(unsigned char) * subchunk->size);
	if (subchunk->stream == NULL) {
		fprintf(stderr, "Could not allocate memory for data stream.\n");
		exit(EXIT_FAILURE);
	}
	ret = fread(subchunk->stream, 1, subchunk->size, fp);
	if (ret != subchunk->size) {
		fprintf(stderr, "Error reading stream in `data` subchunk\n");
		exit(EXIT_FAILURE);
	}

	return subchunk;
}

void print_metadata(struct fmt *chk)
{
	printf("BitsPerSample: %i\n", chk->bits_per_sample);
	printf("SampleRate: %i\n", chk->sample_rate);
	printf("NumberOfChannels: %i\n", chk->nchannels);
	printf("ByteRate: %i\n", chk->byte_rate);
	printf("BlockAlign: %i\n", chk->blk_align);
	printf("Format(PCM=1, IEEE FLOAT=3): %i\n", chk->fmt);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "USAGE: wav <input.wav>\n");
		return 1;
	}
	FILE *fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file\n");
		return 1;
	}

	// Unpacking WAV file in order
	struct descriptor *chunk = unpack_desc(fp);
	struct fmt *subchunk1 = unpack_fmt(fp);
	struct data *subchunk2 = unpack_data(fp);

	if (subchunk2->size % subchunk1->blk_align != 0) {
		fprintf(stderr,
			"Data chunk length is not a multiple of block align.\n");
		return 1;
	}

	print_metadata(subchunk1);
	printf("Duration: %.02fs\n",
	       (double)subchunk2->size / (double)subchunk1->byte_rate);

	// Play sound through alsa-lib
	snd_pcm_t *handle;
	snd_pcm_sframes_t frames;
	int err;

	if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK,
				0)) < 0) {
		fprintf(stderr, "Playback open error: %s.\n",
			snd_strerror(err));
		goto free_resources;
	}

	int format;
	if (subchunk1->fmt == WAVE_FORMAT_PCM) {
		switch (subchunk1->bits_per_sample) {
		case 8:
			format = SND_PCM_FORMAT_U8;
			break;
		case 16:
			format = SND_PCM_FORMAT_S16_LE;
			break;
		case 24:
			if (3 * subchunk1->nchannels == subchunk1->blk_align)
				format = SND_PCM_FORMAT_S24_3LE;
			else
				format = SND_PCM_FORMAT_S24_LE;
			break;
		case 32:
			format = SND_PCM_FORMAT_S32_LE;
			break;
		default:
			fprintf(stderr, "Unsupported bit depth\n");
			exit(EXIT_FAILURE);
		}
	} else {
		format = SND_PCM_FORMAT_FLOAT_LE;
	}

	if ((err = snd_pcm_set_params(
		     handle, format, SND_PCM_ACCESS_RW_INTERLEAVED,
		     subchunk1->nchannels, subchunk1->sample_rate, 1,
		     100 * 1000)) < 0) {
		fprintf(stderr, "Playback open error: %s.\n",
			snd_strerror(err));
		goto free_resources;
	}

	long int framesCount = subchunk2->size / subchunk1->blk_align;
	frames = snd_pcm_writei(handle, subchunk2->stream, framesCount);

	if (frames < 0)
		frames = snd_pcm_recover(handle, frames, 0);
	if (frames < 0) {
		fprintf(stderr, "snd_pcm_writei failed: %s\n",
			snd_strerror(frames));
		goto free_resources;
	}

	if (frames > 0 && frames < framesCount)
		fprintf(stderr, "Short write: expected %li wrote %li\n",
			framesCount, frames);

	err = snd_pcm_drain(handle);
	if (err < 0)
		fprintf(stderr, "snd_pcm_drain failed: %s\n",
			snd_strerror(err));

	snd_pcm_close(handle);
free_resources:
	free(chunk);
	free(subchunk1);
	free(subchunk2->stream);
	free(subchunk2);
	fclose(fp);
	return 0;
}
