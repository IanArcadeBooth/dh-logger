
#include <daqhats/daqhats.h>

#include "wav.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static int run = 1;



// TODO install sigint handler to exit loop and close file gracefully.
void handle_sigint(int signal)
{
	run = 0;
}

int check_hardware(uint8_t address)
{
	int count = hat_list(HAT_ID_ANY, NULL);
	if (count == 0)
	{
		fprintf(stderr, "No DAQHAT found.\n");
	}
	else if (count > 1)
	{
		fprintf(stderr, "WARNING: Running multiple HATS; untested situation.\n");
	}

	struct HatInfo info;
	hat_list(address, &info);

	printf("Address: %d\n", info.address);
	printf("Type: %d\n", info.id);
	printf("Hardware version: %d\n", info.version);
	printf("Name: %s\n", info.product_name);

	if (info.id != HAT_ID_MCC_128)
	{
		fprintf(stderr, "WARNING: You are running an untested hat.\n");
	}
	return 0;
}

int print_mcc128_info()
{
	struct MCC128DeviceInfo *info = mcc128_info();
	printf("NUM_AI_MODES: %d\n", info->NUM_AI_MODES);
	printf("NUM_AI_CHANNELS: %d %d\n", info->NUM_AI_CHANNELS[0], info->NUM_AI_CHANNELS[1]);
	printf("AI_MIN_CODE: %d\n", info->AI_MIN_CODE);
	printf("NUM_AI_RANGES: %d\n", info->NUM_AI_RANGES);
	double vmin[4]; memcpy(vmin, info->AI_MIN_VOLTAGE, 4 * sizeof(double));
	double vmax[4]; memcpy(vmax, info->AI_MAX_VOLTAGE, 4 * sizeof(double));
	printf("AI_MIN_VOLTAGE: %f %f %f %f\n", vmin[0], vmin[1], vmin[2], vmin[3]);
	printf("AI_MAX_VOLTAGE: %f %f %f %f\n", vmax[0], vmax[1], vmax[2], vmax[3]);
	return 0;
}

void print_firmware_version(uint8_t address)
{
	uint8_t ver[2];
	int rv = mcc128_firmware_version(address, (uint16_t*)ver);
	if (rv != RESULT_SUCCESS)
	{
		printf("FAIL\n\n");
	}
	printf("Firmware version: %d.%d\n", ver[0], ver[1]);
}

/*
 * Read from board on address 0.
 * Analog data on channel 0.
 */
int main(int argc, char *argv[])
{
	int rv;
	int address = 0; // Could make this an input arg.
	int channel = 0; // Count make this an input arg.
	int data_rate = 10240; // Samples per second.

	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	rv = check_hardware(address);
	if (rv != 0)
	{	
		fprintf(stderr, "Unhandlable hardwware config. Bailing.\n");
		return rv;
	}
	
	rv = mcc128_open(address);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Can't connect to HAT 0\n");
		return rv;
	}

	print_firmware_version(address);	
	printf("\n");
	print_mcc128_info();

	uint8_t mode;
	rv = mcc128_a_in_mode_read(address, &mode);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Failed to get analog input mode\n");
		return rv;
	}
	if (mode == A_IN_MODE_SE)
	{
		printf("Analog read mode: SINGLE_ENDED\n");
	}
	else
	{
		printf("Analog read mode: DIFFERENTIAL\n");
	}

	uint8_t range;
	rv = mcc128_a_in_range_read(address, &range);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Failed to read analog range\n");
		return rv;
	}
	switch (range)
	{
	case A_IN_RANGE_BIP_10V: printf("Range: +-10V\n"); break;
	case A_IN_RANGE_BIP_5V: printf("Range: +-5V\n"); break;
	case A_IN_RANGE_BIP_2V: printf("Range: +-2V\n"); break;
	case A_IN_RANGE_BIP_1V: printf("Range: +-1V\n"); break;
	default: printf("INVALID RANGE\n");
	}

	char date[128];	
	rv = mcc128_calibration_date(address, date);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read calibration date\n");
		return rv;
	}
	printf("calibration date: %s\n", date);

	double slope, offset;
	rv = mcc128_calibration_coefficient_read(address, channel, &slope, &offset);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read coeffs\n");	
		return rv;
	}
	printf("Slope: %f\nOffset: %f\n", slope, offset);

	uint8_t ch_mask = 1 << channel;
	rv = mcc128_a_in_scan_start(address, ch_mask, 4096, data_rate, OPTS_CONTINUOUS);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't start scan.\n");
		return rv;
	}
	printf("Started scan\n");
	
	uint32_t bs;
	rv = mcc128_a_in_scan_buffer_size(address, &bs);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read buf size\n");	
		return rv;
	}
	printf("Buffer size: %d\n", bs);

	uint16_t status;
	uint32_t samp;
	rv = mcc128_a_in_scan_status(address, &status, &samp);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read scan status\n");
		return rv;
	}
	printf("Scan status:\n\tStatus: %d\n\tSamples: %d\n", status, samp);
	if (status != STATUS_RUNNING)
	{
		fprintf(stderr, "Not all is well in the buffer\n");
		// TODO: handle hardware and buffer overruns
		// ignore trigger?
	}
	// TODO: check number samples available and read them.


	double *buffer = malloc(data_rate * sizeof(double));
	uint32_t actual;

	while (run)
	{
		rv = mcc128_a_in_scan_read(address, &status, data_rate, 0.1, buffer, data_rate, &actual);
		if (rv != RESULT_SUCCESS)
		{
			fprintf(stderr, "read failed: resource unavailable.\n");
			return rv;
		}
		if (status != STATUS_RUNNING)
		{
			if (status == STATUS_HW_OVERRUN)
			{
				fprintf(stderr, "hardware overrun aughghghg\n");
				return status;
			}
			if (status == STATUS_BUFFER_OVERRUN)
			{
				fprintf(stderr, "buffer overrun askasdjf\n");
				return status;
			}
		}
		
		printf("read %d samples. Here's a few of them:\n\t%f %f %f %f\n\n", actual,
			buffer[0], buffer[256], buffer[512], buffer[1023]);
	}
	printf("Job stopped. Closing up.\n");

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
