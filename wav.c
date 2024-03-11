
#include "wav.h"
#include <string.h>

int write_header(FILE *fp, uint32_t size)
{
	fprintf(fp, "RIFF");
	fwrite(&size, 4, 1, fp);
	fprintf(fp, "WAVE");
}

int write_fmt(FILE *fp, uint16_t nch, uint32_t rate)
{
	uint32_t size = 18;
	uint16_t fmt = 0x0003;
	uint16_t block_align = 4 * nch;
	uint32_t bytes_per_sec = 4 * nch * rate;
	uint16_t bit_depth = 32;
	uint16_t cb_size = 0;
	
	fprintf(fp, "fmt ");
	fwrite(&size, 4, 1, fp);
	fwrite(&fmt, 2, 1, fp);
	fwrite(&nch, 2, 1, fp);
	fwrite(&rate, 4, 1, fp);
	fwrite(&bytes_per_sec, 4, 1, fp);
	fwrite(&block_align, 2, 1, fp);
	fwrite(&bit_depth, 2, 1, fp);
	fwrite(&cb_size, 2, 1, fp);
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

