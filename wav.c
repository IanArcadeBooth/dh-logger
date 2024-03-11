
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
	uint32_t size = 16;
	uint16_t format = 0x0001; // PCM
	uint32_t bps = 2*rate; // only for 16 bit uncompressed.	
	uint16_t block_align = 2;
	uint16_t bit_depth = 16;
	uint16_t cb_size = 0;
	
	fprintf(fp, "fmt ");
	fwrite(&size, 4, 1, fp);
	fwrite(&format, 2, 1, fp);
	fwrite(&nch, 2, 1, fp);
	fwrite(&rate, 4, 1, fp);
	fwrite(&bps, 4, 1, fp);
	fwrite(&block_align, 2, 1, fp);//TBD
	fwrite(&bit_depth, 2, 1, fp);
}

int write_data(FILE *fp, uint32_t size)
{
	fprintf(fp, "DATA");
	fwrite(&size, 4, 1, fp);
}

