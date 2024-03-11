
#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>
#include <stdio.h>

/*
 * Write the RIFF header.
 */
int write_header(FILE *fp, uint32_t size);

/*
 * Write the FMT chunk, which tells the reader the format of the
 * data to follow.
 */
int write_fmt(FILE *fp, uint16_t nch, uint32_t rate);

/*
 * Write the start of the data chunk.
 */
int write_data(FILE *fp, uint32_t size);

#endif
