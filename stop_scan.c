
#include <daqhats/daqhats.h>

#include <stdio.h>

int main()
{
	int rv;
	int address = 0;
	
	rv = mcc128_open(address);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Can't connect to HAT 0\n");
		return rv;
	}

	rv = mcc128_a_in_scan_stop(address);
	if (rv != RESULT_SUCCESS)
	{
		printf("Failed to stop the scan!\n");
		return rv;
	}
		 
	rv = mcc128_close(0);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't close HAT 0\n");
		return rv;
	}
	return 0;
}

