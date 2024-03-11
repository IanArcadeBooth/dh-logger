
#include "wav.h"
#include <string.h>

int write_header(FILE *fp, uint32_t size)
{
	fprintf(fp, "RIFF");
	fwrite(&size, 4, 1, fp);
	fprintf(fp, "WAVE");
}

int write_fmt(FILE *fp, wav_format fmt, uint16_t nch, uint32_t rate)
{
	uint32_t size;
	uint32_t bps = 2*rate; // only for 16 bit uncompressed.	
	uint16_t block_align = 2;

	uint16_t bit_depth;
      	if (fmt == WAV_FORMAT_PCM)
	{
		size = 16;
		bit_depth = 16;	
	}
	else if (fmt == WAV_FORMAT_IEEE_FLOAT)
	{
		size = 18;
		bit_depth = 32;
	}
	
	fprintf(fp, "fmt ");
	fwrite(&size, 4, 1, fp);
	fwrite(&fmt, 2, 1, fp);
	fwrite(&nch, 2, 1, fp);
	fwrite(&rate, 4, 1, fp);
	fwrite(&bps, 4, 1, fp);
	fwrite(&block_align, 2, 1, fp);//TBD
	fwrite(&bit_depth, 2, 1, fp);
	if (fmt == WAV_FORMAT_IEEE_FLOAT)
	{
		uint16_t cb_size = 0;
		fwrite(&cb_size, 2, 1, fp);
	}
}

int write_fact(FILE *fp, uint32_t n)
{
	uint32_t sz = 4;
	fprintf(fp, "fact");
	fwrite(&sz, 4, 1, fp);
	fwrite(&n, 4, 1, fp);
}

int write_data(FILE *fp, uint32_t size)
{
	fprintf(fp, "data");
	fwrite(&size, 4, 1, fp);
}

