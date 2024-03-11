
#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>
#include <stdio.h>


typedef enum wf {
	WAV_FORMAT_PCM = 0x0001,
	WAV_FORMAT_IEEE_FLOAT = 0x0003
} wav_format;

/*
 * Write the RIFF header.
 */
int write_header(FILE *fp, uint32_t size);

/*
 * Write the FMT chunk, which tells the reader the format of the
 * data to follow.
 */
int write_fmt(FILE *fp, wav_format fmt, uint16_t nch, uint32_t rate);

/*
 * Write fact chunk.
 *
 * Only needed for IEEE_FLOAT format
 */
int write_fact(FILE *fp, uint32_t n_samples);

/*
 * Write the start of the data chunk.
 */
int write_data(FILE *fp, uint32_t size);

#endif
