
#ifndef _WAV_H_
#define _WAV_H_

#include <stdint.h>

/*
 * Write the RIFF header.
 */
write_header(FILE *fp, size);

/*
 * Write the FMT chunk, which tells the reader the format of the
 * data to follow.
 */
write_fmt(FILE *fp, uin23_t rate);

/*
 * Write the DRDC logger metadata.
 * 
 * This custom chunk stores information about the recording device
 * that was used to create the WAV file.
 */
write_drdc(FILE *fp, char* buffer, size_t, bytes);

/*
 * Write the start of the data chunk.
 */
write_data(FILE *fp uint32_t size);

#endif
