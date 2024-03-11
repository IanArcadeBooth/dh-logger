
#include <daqhats/daqhats.h>

#include "wav.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include <ini.h>

static int run = 1;

double *buffer;	// Local store for samples.
int16_t *tmp;	// Local store for 16 bit converted samples.

// TODO install sigint handler to exit loop and close file gracefully.
void handle_sigint(int signal)
{
	run = 0;
}

typedef struct config_s
{
	uint8_t address;
	uint8_t channel;
	uint8_t range;
	uint8_t mode;
	uint16_t sample_rate;
	uint16_t format;
	uint16_t file_duration;
} config_t;

typedef struct daq_hat_s 
{
	double slope;
	double offset;
	uint16_t ver_hw;
	float sample_rate;
	int8_t ver_maj;
	int8_t ver_min;
	uint8_t address;
	uint8_t channel;
	uint8_t range;
	char mode[14];
	char product_name[256];
	char model[32];
	char serial[9];
	char calibration_date[11];
} daq_info_t;

/*
 * Must open connection to the DAQ before calling this.
 */
static daq_info_t get_configuration(uint8_t address, uint8_t channel, uint16_t rate)
{
	int rv;
	struct HatInfo info;
	int count = hat_list(address, &info);
	if (count == 0)
	{
		fprintf(stderr, "No DAQHat found.\n");
	}
	if (count > 1)
	{
		fprintf(stderr, "Multiple DAQHat found; careful you address the right one.\n");
	}
	if (info.id != HAT_ID_MCC_128)
	{
		fprintf(stderr, "WARNING: this software only designed for MCC 128, this looks like something else\n");
	}

	daq_info_t cfg;
	cfg.address = address;
	cfg.channel = channel;
	strcpy(cfg.model, "HAT_ID_MCC_128");
	cfg.ver_hw = info.version;
	strcpy(cfg.product_name, info.product_name);

	uint8_t ver[2];
	rv = mcc128_firmware_version(address, (uint16_t*)ver);
	if (rv != RESULT_SUCCESS)
	{
		printf("Couldn't read firmware version.\n");
		exit(rv);
	}
	cfg.ver_maj = ver[0];
	cfg.ver_min = ver[1];

	rv = mcc128_calibration_date(address, cfg.calibration_date);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read calibration date.\n");
		exit(rv);
	}

	uint8_t range;
	rv = mcc128_a_in_range_read(address, &range);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read range.\n");
		exit(rv);
	}

	rv = mcc128_calibration_coefficient_read(address, range, &(cfg.slope), &(cfg.offset));
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read calibration coefficients.\n");
		exit(rv);
	}
	cfg.sample_rate = rate;

	switch (range)
	{
		case A_IN_RANGE_BIP_10V: cfg.range = 10; break;
		case A_IN_RANGE_BIP_5V:  cfg.range = 5;  break;
		case A_IN_RANGE_BIP_2V:  cfg.range = 2;  break;
		case A_IN_RANGE_BIP_1V:  cfg.range = 1;  break;
		default: cfg.range = 255;
	}

	rv = mcc128_serial(address, cfg.serial);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read serial number.\n");
		exit(rv);
	}

	uint8_t mode;	
	rv = mcc128_a_in_mode_read(address, &mode);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Failed to get analog input mode\n");
		exit(rv);
	}
	switch (mode)
	{
	case A_IN_MODE_SE:   strcpy(cfg.mode, "SINGLE_ENDED"); break;
	case A_IN_MODE_DIFF: strcpy(cfg.mode, "DIFFERENTIAL"); break;
	default: strcpy(cfg.mode, "UNKNOWN");
	}

	return cfg;
}

static void print_configuration(daq_info_t cfg)
{
	printf("MCCDAQ Configuration:\n");
	printf("  Product: %s\n", cfg.product_name);
	printf("  Model: %s\n", cfg.model);
	printf("  Hardware Version: %d\n", cfg.ver_hw);
	printf("  Firmware Version: %d.%d\n", cfg.ver_maj, cfg.ver_min);
	printf("  Serial Number: %s\n", cfg.serial);
	printf("  Address: %d\n", cfg.address);
	printf("  Channel: %d\n", cfg.channel);
	printf("  Mode: %s\n", cfg.mode);
	printf("  Range: +/- %dV\n", cfg.range);
	printf("  Sample Rate: %fHz\n", cfg.sample_rate);
	printf("  Calibration:\n");
	printf("    Date: %s\n", cfg.calibration_date);
	printf("    Slope: %f\n", cfg.slope);
	printf("    Offset: %f\n", cfg.offset);
}

static void serialize_configuration(FILE *fp, daq_info_t const cfg, char const*timestr)
{	
	fprintf(fp, "<?xml version='1.0'?>\n");
	fprintf(fp, "<dataset>");
	fprintf(fp, "<hardware>");
	fprintf(fp, "<product role='daq'>");
	fprintf(fp, "<name>%s</name>", cfg.product_name);
	fprintf(fp, "<model version='%d'>%s</model>", cfg.ver_hw, cfg.model);
	fprintf(fp, "<firmware version='%d.%d' />", cfg.ver_maj, cfg.ver_min);
	fprintf(fp, "<item serial='%s' />", cfg.serial);
	fprintf(fp, "<address>%d</address>", cfg.address);
	fprintf(fp, "<channel>%d</channel>", cfg.address);
	fprintf(fp, "<mode>%s</mode>", cfg.mode);
	fprintf(fp, "<range>+/- %dV</range>", cfg.range);
	fprintf(fp, "<sample_rate>%fHz</sample_rate>", cfg.sample_rate);
	fprintf(fp, "<calibration date='%s'>", cfg.calibration_date);
	fprintf(fp, "<slope>%f</slope>", cfg.slope);
	fprintf(fp, "<offset>%f</offset>", cfg.offset);
	fprintf(fp, "</calibration>");
	fprintf(fp, "</product>");
	fprintf(fp, "<product role='logger'>");
	fprintf(fp, "<name>Raspberry Pi</name>");
	fprintf(fp, "<model version='4'>B Rev 1.2</model>"); // TODO should introspect from pi.
	fprintf(fp, "</product>");
	fprintf(fp, "</hardware>");
	fprintf(fp, "<time>%s</time>", timestr);
	fprintf(fp, "</dataset>");
}

/*
 * Write the DRDC logger metadata.
 * 
 * This custom chunk stores information about the recording device
 * that was used to create the WAV file.
 */
static int write_drdc(FILE *fp, daq_info_t const cfg, char const*timestr)
{
	uint8_t o = 0;
	uint32_t chunk_sz = 0;

	// Write the chunk header.
	fprintf(fp, "DRDC");
	int pos = ftell(fp);
	fwrite(&chunk_sz, 4, 1, fp);

	// Write the configuration data.
	serialize_configuration(fp, cfg, timestr);
	
	// Get the XML size.
	size_t lpos = ftell(fp);
	uint32_t size = lpos - pos - 4;
	uint32_t pad = size % 4;
	fwrite(&chunk_sz, pad, 1, fp); // pad to a 32-bit boundary.
	size_t fpos = ftell(fp);

	// Now fix the chunk size.
	fseek(fp, pos, SEEK_SET);
	chunk_sz = size + pad;
	fwrite(&chunk_sz, 1, 4, fp);

	// Get back to the end of the file.
	fseek(fp, fpos, SEEK_SET);
}

/*
 * Record DAQ samples to WAV file for duration d.
 *
 * Precondition: already scanning on address 0.
 *
 * Tries to read one second's worth of samples. The read times out after 2 seconds.
 * If we don't have the first second's samples by then, then something is wrong and
 * we need to handle the issue.
 */
static void record(daq_info_t const cfg, config_t const ini, float duration)
{

	// Determine current time.
	time_t t;
       	time(&t);
    	char timestr[sizeof "1970-01-01T00:00:00Z"];
    	strftime(timestr, sizeof(timestr), "%FT%TZ", gmtime(&t));

	// Open WAV file for writing.
	char filename[128];
	sprintf(filename, "%s-ntst.wav", timestr);
	FILE *fp = fopen(filename, "w");
	printf("start file \"%s\"\n", filename);

	int rv;
	uint16_t status;
	int32_t i = 0;
	int32_t iterations = ceil(duration);	// One read per second.
	int32_t actual;

	// Write the header and FMT chunk.
	write_header(fp, 100);
	write_fmt(fp, ini.format, 1, cfg.sample_rate);
	uint32_t guess_samples = iterations * cfg.sample_rate;
	uint32_t guess_bytes;
	if (ini.format == WAV_FORMAT_IEE_FLOAT)
	{
		write_fact(fp, guess_samples);
		guess_bytes = guess_samples * 8;
	}
	else
	{
		guess_bytes = guess_samples * 2;
	}
	write_drdc(fp, cfg, timestr); 

	write_data(fp, guess_bytes);
	int guess_pos = ftell(fp) - 4;

	// Record data.
	while (run && (i < iterations))
	{
		rv = mcc128_a_in_scan_read(cfg.address, &status, cfg.sample_rate, 2.0, buffer, cfg.sample_rate, &actual);
		if (rv != RESULT_SUCCESS)
		{
			fprintf(stderr, "read failed: resource unavailable.\n");
		}
		if (status != STATUS_RUNNING)
		{
			if (status & STATUS_HW_OVERRUN)
			{
				fprintf(stderr, "hardware overrun aughghghg\n");
			}
			if (status & STATUS_BUFFER_OVERRUN)
			{
				fprintf(stderr, "buffer overrun askasdjf\n");
			}
		}
		if (actual < cfg.sample_rate)
		{
			fprintf(stderr, "somethign wrong with read\n");
			exit(actual -1);
		}

		if (ini.format == WAV_FORMAT_PCM)
		{
			// Convert to 16 bit values.
			for (int j = 0; j < cfg.sample_rate; j++)
			{
				tmp[j] = round(buffer[j] - INT16_MAX);
			}
			rv = fwrite(tmp, sizeof(int16_t), cfg.sample_rate, fp);
		}
		else if (ini.format == WAV_FORMAT_IEE_FLOAT)
		{
			// We're actually recording doubles.
			rv = fwrite(buffer, sizeof(double), cfg.sample_rate, fp);
		}
		else
		{
			fprintf(stderr, "Invalid recording format\n");
			exit(1);
		}
		printf("recorded %d samples.\n", actual);
		i++;
	}

	// Fix any file size issues.
	printf("close file \"%s\"\n", filename);
	
	fclose(fp);
}

static int parse_range(char const* value)
{
	if (strncmp("10", value, 2) == 0) {
		return A_IN_RANGE_BIP_10V;
	} else if (strncmp("5", value, 1) == 0) {
		return A_IN_RANGE_BIP_5V;
	} else if (strncmp("2", value, 1) == 0) {
		return A_IN_RANGE_BIP_2V;
	} else if (strncmp("1", value, 1) == 0) {
		return A_IN_RANGE_BIP_1V;
	} else {
		fprintf(stderr, "invalid range: \"%s\"\n", value);
		exit(1);
	}
}

static int parse_mode(char const* value)
{
	if (strncasecmp("SE", value, 2) == 0) {
		return A_IN_MODE_SE;
	} else if (strncasecmp("DIFF", value, 4) == 0) {
		return A_IN_MODE_DIFF;
	} else {
		fprintf(stderr, "invalid mode: \"%s\"\n", value);
		exit(1);
	}
}

static int parse_format(char const* value)
{
	if (strncasecmp("PCM", value, 3) == 0) {
		return WAV_FORMAT_PCM;
	} else if (strncasecmp("FLOAT", value, 5) == 0) {
		return WAV_FORMAT_IEE_FLOAT;
	} else {
		fprintf(stderr, "invalid format: \"%s\"\n", value);
		exit(1);
	}
}

static int inih_handler(void *user, char const* section, char const* name, char const* value)
{
	config_t *c = (config_t*)user;

	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	if (MATCH("mcc128", "address")) {
		c->address = atoi(value);		
	} else if (MATCH("mcc128", "channel")) {
		c->channel = atoi(value);
	} else if (MATCH("mcc128", "range")) {
		c->range = parse_range(value);
	} else if (MATCH("mcc128", "mode")) {
		c->mode = parse_mode(value);
	} else if (MATCH("mcc128", "sample_rate")) {
		c->sample_rate = atoi(value);
	} else if (MATCH("mcc128", "format")) {
		c->format = parse_format(value);
	} else if (MATCH("mcc128", "file_duration")) {
		c->file_duration = atoi(value);
	} else {
		fprintf(stderr, "Unrecognized entry: \"%s:%s\"\n", section, name);
	}
}

int main(int argc, char *argv[])
{
	int rv;
	
	
	config_t ini;
	rv = ini_parse("logger.conf", inih_handler, &ini);
	if (rv < 0)
	{
		fprintf(stderr, "Error reading conf\n");
		return rv;
	}

	size_t buf_size = 4096;

	signal(SIGINT, handle_sigint);
	signal(SIGTERM, handle_sigint);

	// Convert requested data rate to actual data rate.
	double actual_rate;
	rv = mcc128_a_in_scan_actual_rate(1, ini.sample_rate, &actual_rate);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't read actual scan rate.\n");
		return rv;
	}
	if (ini.sample_rate != actual_rate)
	{
		fprintf(stderr, "Actual sample rate differs from requested:\n");
		fprintf(stderr, "\tactual: %f\n", actual_rate);
		fprintf(stderr, "\trequested: %f\n", ini.sample_rate);
		ini.sample_rate = actual_rate;
	}
	printf("actual %f\nini %d\n", actual_rate, ini.sample_rate);

	rv = mcc128_open(ini.address);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Can't connect to HAT 0\n");
		return rv;
	}

	rv = mcc128_a_in_mode_write(ini.address, ini.mode);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Can't set mode.\n");
		return rv;
	}

	rv = mcc128_a_in_range_write(ini.address, ini.range);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Can't set range.\n");
	      	return rv;	
	}

	buffer = malloc(actual_rate * sizeof(double));
	tmp = malloc(actual_rate * sizeof(uint16_t));

	rv = mcc128_a_in_scan_start(ini.address, 1 << ini.channel, buf_size, actual_rate, OPTS_CONTINUOUS | OPTS_NOSCALEDATA);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't start scan.\n");
		return rv;
	}
	
	daq_info_t cfg = get_configuration(ini.address, ini.channel, actual_rate);
	print_configuration(cfg);

	while (run)
	{
		record(cfg, ini, 60.0);
	}

	rv = mcc128_a_in_scan_stop(ini.address);
	if (rv != RESULT_SUCCESS)
	{
		printf("Failed to stop the scan!\n");
		return rv;
	}
	free(buffer);
		 
	rv = mcc128_close(0);
	if (rv != RESULT_SUCCESS)
	{
		fprintf(stderr, "Couldn't close HAT 0\n");
		return rv;
	}
	return 0;
}
