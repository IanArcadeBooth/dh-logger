
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

	uint16_t status;
	uint32_t samp;
	rv = mcc128_a_in_scan_status(address, &status, &samp);
	if (rv == RESULT_RESOURCE_UNAVAIL)
	{
		printf("NOT RUNNING\n");
	}
	else
	{
	if (status && STATUS_RUNNING)
	{
		printf("RUNNING\n");
	}
	if (status && STATUS_HW_OVERRUN)
	{
		printf("HW_OVERRUN\n");
	}
	if (status && STATUS_BUFFER_OVERRUN)
	{ 
		printf("BUFFER_OVERRUN\n");
	}
	if (status && STATUS_TRIGGERED)
	{
		printf("TRIGGERED\n");
	}
	}
		 
	rv = mcc128_close(0);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't close HAT 0\n");
		return rv;
	}
	return 0;
}

