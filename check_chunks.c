
#include <stdint.h>
#include <stdio.h>

struct hdr
{
	char type[4];
	uint32_t size;
};

int main(int argc, char *argv[])
{
	struct hdr header;
	char buf[4096];

	fread(buf, 12, 1, stdin);
	while (feof(stdin) == 0)
	{
		fread(&header, sizeof(struct hdr), 1, stdin);
		printf("%c%c%c%c\n", header.type[0], header.type[1], header.type[2], header.type[3]);

		int n = 0;
		while (n < header.size)
		{
			if ((header.size - n) > 4096)
			{
				fread(buf, 4096, 1, stdin);
				n += 4096;
				continue;
			}
			else
			{
				fread(buf, header.size - n, 1, stdin);
				break;
			}
		}
	}
	return 0;
}
