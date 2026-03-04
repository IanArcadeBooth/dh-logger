
#include <daqhats/daqhats.h>

#include "wav.h"

#include <ctype.h>
#include <errno.h>
#include <gpiod.h>
#include <gps.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ini.h>

#define SW1_GPIO 17 // pair 1-5
#define SW2_GPIO 27 // pair 2-6
#define SW3_GPIO 22 // pair 3-7
#define SW4_GPIO 23 // pair 4-8

static int run = 1;

uint32_t buf_count = 0;
double *buffer; // Local store for samples.
float *wbuf;    // Local store for 32 bit float samples

// TODO install sigint handler to exit loop and close file gracefully.
void handle_sigint(int signal) { run = 0; }

typedef struct switch_state_s {
  int pair1;
  int pair2;
  int pair3;
  int pair4;
} switch_state_t;

typedef struct config_s {
  uint8_t address;
  uint8_t channels;
  uint8_t range;
  uint8_t mode;
  uint32_t sample_rate;
  uint16_t file_duration;
  char file_postfix[32];
} config_t;

typedef struct daq_hat_s {
  double slope;
  double offset;
  uint16_t ver_hw;
  float sample_rate;
  int8_t ver_maj;
  int8_t ver_min;
  uint8_t address;
  uint8_t channels;
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
static daq_info_t get_configuration(uint8_t address, uint8_t channels,
                                    uint16_t rate) {
  int rv;
  struct HatInfo info;
  int count = hat_list(address, &info);
  if (count == 0) {
    fprintf(stderr, "No DAQHat found.\n");
  }
  if (count > 1) {
    fprintf(stderr,
            "Multiple DAQHat found; careful you address the right one.\n");
  }
  if (info.id != HAT_ID_MCC_128) {
    fprintf(stderr, "WARNING: this software only designed for MCC 128, this "
                    "looks like something else\n");
  }

  daq_info_t cfg;
  cfg.address = address;
  cfg.channels = channels;
  strcpy(cfg.model, "HAT_ID_MCC_128");
  cfg.ver_hw = info.version;
  strcpy(cfg.product_name, info.product_name);

  uint8_t ver[2];
  rv = mcc128_firmware_version(address, (uint16_t *)ver);
  if (rv != RESULT_SUCCESS) {
    printf("Couldn't read firmware version.\n");
    exit(rv);
  }
  cfg.ver_maj = ver[0];
  cfg.ver_min = ver[1];

  rv = mcc128_calibration_date(address, cfg.calibration_date);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't read calibration date.\n");
    exit(rv);
  }

  uint8_t range;
  rv = mcc128_a_in_range_read(address, &range);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't read range.\n");
    exit(rv);
  }

  rv = mcc128_calibration_coefficient_read(address, range, &(cfg.slope),
                                           &(cfg.offset));
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't read calibration coefficients.\n");
    exit(rv);
  }
  cfg.sample_rate = rate;

  switch (range) {
  case A_IN_RANGE_BIP_10V:
    cfg.range = 10;
    break;
  case A_IN_RANGE_BIP_5V:
    cfg.range = 5;
    break;
  case A_IN_RANGE_BIP_2V:
    cfg.range = 2;
    break;
  case A_IN_RANGE_BIP_1V:
    cfg.range = 1;
    break;
  default:
    cfg.range = 255;
  }

  rv = mcc128_serial(address, cfg.serial);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't read serial number.\n");
    exit(rv);
  }

  uint8_t mode;
  rv = mcc128_a_in_mode_read(address, &mode);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Failed to get analog input mode\n");
    exit(rv);
  }
  switch (mode) {
  case A_IN_MODE_SE:
    strcpy(cfg.mode, "SINGLE_ENDED");
    break;
  case A_IN_MODE_DIFF:
    strcpy(cfg.mode, "DIFFERENTIAL");
    break;
  default:
    strcpy(cfg.mode, "UNKNOWN");
  }

  return cfg;
}

static void print_configuration(daq_info_t cfg) {
  printf("MCCDAQ Configuration:\n");
  printf("  Product: %s\n", cfg.product_name);
  printf("  Model: %s\n", cfg.model);
  printf("  Hardware Version: %d\n", cfg.ver_hw);
  printf("  Firmware Version: %d.%d\n", cfg.ver_maj, cfg.ver_min);
  printf("  Serial Number: %s\n", cfg.serial);
  printf("  Address: %d\n", cfg.address);
  printf("  Channels: ");
  for (int i = 0; i < 8; i++) {
    if ((cfg.channels & (1 << i)) != 0) {
      printf("%d ", i);
    }
  }
  printf("\n");
  printf("  Mode: %s\n", cfg.mode);
  printf("  Range: +/- %dV\n", cfg.range);
  printf("  Sample Rate: %fHz\n", cfg.sample_rate);
  printf("  Calibration:\n");
  printf("    Date: %s\n", cfg.calibration_date);
  printf("    Slope: %f\n", cfg.slope);
  printf("    Offset: %f\n", cfg.offset);
}

typedef struct {
  int have_fix; // 1 if valid fix acquired
  int mode;     // 2=2D, 3=3D
  double lat;
  double lon;
  double alt;
} gps_fix_t;

static long long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static gps_fix_t get_gps_fix(int timeout_ms) {
  gps_fix_t out = {0};

  struct gps_data_t gps;
  if (gps_open("localhost", "2947", &gps) != 0) {
    return out;
  }

  gps_stream(&gps, WATCH_ENABLE | WATCH_JSON, NULL);

  long long start = now_ms();
  while (now_ms() - start < timeout_ms) {
    // wait up to 0.5s each loop
    if (gps_waiting(&gps, 500000)) {
      if (gps_read(&gps, NULL, 0) > 0) {
        // Require at least 2D fix and sane coordinates
        // Require that gpsd has actually provided mode + lat/lon in this report
        if ((gps.set & (MODE_SET | LATLON_SET)) == (MODE_SET | LATLON_SET) &&
            gps.fix.mode >= MODE_2D && isfinite(gps.fix.latitude) &&
            isfinite(gps.fix.longitude)) {
          out.have_fix = 1;
          out.mode = gps.fix.mode;
          out.lat = gps.fix.latitude;
          out.lon = gps.fix.longitude;

          // altitude may or may not be available
          out.alt = (gps.set & ALTITUDE_SET) && isfinite(gps.fix.altitude)
                        ? gps.fix.altitude
                        : NAN;
          break;
        }
      }
    }
  }

  gps_stream(&gps, WATCH_DISABLE, NULL);
  gps_close(&gps);
  return out;
}

static void serialize_configuration(FILE *fp, daq_info_t const cfg, char const* timestr, switch_state_t sw) {
  fprintf(fp, "<?xml version='1.0'?>\n");
  fprintf(fp, "<dataset>");

  fprintf(fp, "<hardware>");
  fprintf(fp, "<product role='daq'>");
  fprintf(fp, "<name>%s</name>", cfg.product_name);
  fprintf(fp, "<model version='%d'>%s</model>", cfg.ver_hw, cfg.model);
  fprintf(fp, "<firmware version='%d.%d' />", cfg.ver_maj, cfg.ver_min);
  fprintf(fp, "<item serial='%s' />", cfg.serial);
  fprintf(fp, "<address>%d</address>", cfg.address);
  fprintf(fp, "<channels>");
  int id = 0;
  for (int i = 0; i < 8; i++) {
    if ((cfg.channels & (1 << i)) != 0) {
      fprintf(fp, "<channel id='%d'>%d</channel>", id, i);
      id++;
    }
  }
  fprintf(fp, "</channels>");
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
  fprintf(fp, "<model version='4'>B Rev 1.2</model>"); // TODO should introspect
                                                       // from pi.
  fprintf(fp, "</product>");
  fprintf(fp, "</hardware>");
  fprintf(fp, "<time>%s</time>", timestr);
  if (gps && gps->have_fix) {
    fprintf(fp, "<location>");
    fprintf(fp, "<lat>%.7f</lat>", gps->lat);
    fprintf(fp, "<lon>%.7f</lon>", gps->lon);

    if (isfinite(gps->alt)) {
      fprintf(fp, "<alt_m>%.2f</alt_m>", gps->alt);
    }

    fprintf(fp, "<fix_mode>%d</fix_mode>", gps->mode); // 2 or 3
    fprintf(fp, "</location>");
  } else {
    fprintf(fp, "<location status='NO_FIX' />");
  }
  // Switch-based input pairing metadata (external diff->SE circuit)
fprintf(fp, "<input_pairs>");
fprintf(fp, "<pair id='1' a='1' b='5' type='%s'/>", sw.pair1 ? "DIFF_CONVERTED" : "SE");
fprintf(fp, "<pair id='2' a='2' b='6' type='%s'/>", sw.pair2 ? "DIFF_CONVERTED" : "SE");
fprintf(fp, "<pair id='3' a='3' b='7' type='%s'/>", sw.pair3 ? "DIFF_CONVERTED" : "SE");
fprintf(fp, "<pair id='4' a='4' b='8' type='%s'/>", sw.pair4 ? "DIFF_CONVERTED" : "SE");
fprintf(fp, "</input_pairs>");
  fprintf(fp, "</dataset>");
}

/*
 * Write the DRDC logger metadata.
 *
 * This custom chunk stores information about the recording device
 * that was used to create the WAV file.
 */
static int write_drdc(FILE *fp, daq_info_t const cfg, char const* timestr, switch_state_t sw) {
  uint8_t o = 0;
  uint32_t chunk_sz = 0;

  // Write the chunk header.
  fprintf(fp, "DRDC");
  int pos = ftell(fp);
  fwrite(&chunk_sz, 4, 1, fp);

  // Write the configuration data.
  gps_fix_t fix = get_gps_fix(3000); // wait up to 3 seconds for a fix
  serialize_configuration(fp, cfg, timestr, &fix);

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

static int parse_range(char const *value) {
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

static int parse_mode(char const *value) {
  if (strncasecmp("SE", value, 2) == 0) {
    return A_IN_MODE_SE;
  } else if (strncasecmp("DIFF", value, 4) == 0) {
    return A_IN_MODE_DIFF;
  } else {
    fprintf(stderr, "invalid mode: \"%s\"\n", value);
    exit(1);
  }
}

/*
 * Returns the channels to record.
 *
 * High bit - record channel
 * Low bit - don't.
 */
static uint8_t parse_channels(char const *value) {
  uint8_t result = 0x00;
  char *p, *q = strdup(value);
  p = q;
  while (*p) {
    if (isdigit(*p) == 0) {
      p++;
      continue;
    }
    errno = 0;
    long value = strtol(p, &p, 10);
    if (errno != 0) {
      fprintf(stderr, "channel parse failed\n");
      exit(1);
    }
    if ((value > 7) || (value < 0)) {
      fprintf(stderr, "channel values must be between 0-7\n");
      exit(1);
    }
    result |= (0x01 << value);
  }
  free(q);
  return result;
}

/*
 * Get the number of channels set high in the channel bitmask.
 */
int ch_count(uint8_t ch) {
  int n = 0;
  for (int i = 0; i < 8; i++) {
    if ((ch & (1 << i)) != 0)
      n++;
  }
  return n;
}

// This function reads the state of the gpio pins (switches)
static int read_gpio(int gpio) {
  struct gpiod_chip *chip;
  struct gpiod_line *line;
  int value;

  chip = gpiod_chip_open_by_name("gpiochip0");
  if (!chip)
    return -1;

  line = gpiod_chip_get_line(chip, gpio);
  if (!line) {
    gpiod_chip_close(chip);
    return -1;
  }

  if (gpiod_line_request_input(line, "dh_logger") < 0) {
    gpiod_chip_close(chip);
    return -1;
  }

  value = gpiod_line_get_value(line);

  gpiod_chip_close(chip);
  return value;
}
/*
 * Record DAQ samples to WAV file for duration d.
 *
 * Precondition: already scanning on address 0.
 *
 * Tries to read one second's worth of samples. The read times out after 2
 * seconds. If we don't have the first second's samples by then, then something
 * is wrong and we need to handle the issue.
 */
static void record(daq_info_t const cfg, config_t const ini) {
  // Determine current time.
  time_t t;
  time(&t);
  char timestr[sizeof "1970-01-01T00:00:00Z"];
  strftime(timestr, sizeof(timestr), "%FT%TZ", gmtime(&t));

  // Open WAV file for writing.
  char filename[128];
  sprintf(filename, "%s-%s.wav", timestr, ini.file_postfix);
  FILE *fp = fopen(filename, "w");
  printf("start file \"%s\"\n", filename);

  int rv;
  uint16_t status;
  int32_t i = 0;
  int32_t iterations = ceil(ini.file_duration);
  int32_t actual;

  int nch = ch_count(cfg.channels);

  // Write the header and FMT chunk.
  write_header(fp, 100);
  write_fmt(fp, ch_count(ini.channels), cfg.sample_rate);
  uint32_t guess_samples = iterations * cfg.sample_rate * nch;
  uint32_t guess_bytes = guess_samples * sizeof(float);
  write_fact(fp, guess_samples);
  write_drdc(fp, cfg, timestr);

  write_data(fp, guess_bytes);
  int guess_pos = ftell(fp) - sizeof(float);
  uint32_t bytes_written = 0;

  // Record data.
  while (run && (i < iterations)) {
    rv = mcc128_a_in_scan_read(cfg.address, &status, cfg.sample_rate, 2.0,
                               buffer, buf_count, &actual);
    if (rv != RESULT_SUCCESS) {
      fprintf(stderr, "read failed: resource unavailable.\n");
    }
    if (status != STATUS_RUNNING) {
      if (status & STATUS_HW_OVERRUN) {
        fprintf(stderr, "hardware overrun aughghghg\n");
      }
      if (status & STATUS_BUFFER_OVERRUN) {
        fprintf(stderr, "buffer overrun askasdjf\n");
      }
    }
    if (actual != cfg.sample_rate) {
      fprintf(stderr, "something wrong with read\n");
      printf("actual %d\n", actual);
      printf("sample_rate: %d\n", buf_count);
      exit(actual - 1);
    }

    for (int j = 0; j < buf_count; j++) {
      wbuf[j] = buffer[j] / (float)cfg.range;
    }
    rv = fwrite(wbuf, sizeof(float), cfg.sample_rate * nch, fp);
    bytes_written += rv * sizeof(float);
    i++;
  }

  if (bytes_written != guess_bytes) {
    printf("Fixing bytes in data:\n");
    printf("Guess: %d\nActual: %d\n", guess_bytes, bytes_written);
    fseek(fp, guess_pos, SEEK_SET);
    fwrite(&bytes_written, sizeof(uint32_t), 1, fp);
  }

  // Fix any file size issues.
  printf("close file \"%s\"\n", filename);

  fclose(fp);
}

static uint8_t parse_address(char const *value) {
  errno = 0;
  char *tmp;
  long result = strtol(value, &tmp, 10);
  if (errno != 0) {
    fprintf(stderr, "Error reading address. Must be numeric\n");
    exit(1);
  }
  if ((result < 0) || (result > 7)) {
    fprintf(stderr, "Invalid address: %d. Must be 0-7.\n", result);
    exit(1);
  }
  return result;
}

static int inih_handler(void *user, char const *section, char const *name,
                        char const *value) {
  config_t *c = (config_t *)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if (MATCH("mcc128", "address")) {
    c->address = parse_address(value);
  } else if (MATCH("mcc128", "channels")) {
    c->channels = parse_channels(value);
  } else if (MATCH("mcc128", "range")) {
    c->range = parse_range(value);
  } else if (MATCH("mcc128", "mode")) {
    c->mode = parse_mode(value);
  } else if (MATCH("mcc128", "sample_rate")) {
    c->sample_rate = atoi(value);
  } else if (MATCH("logger", "file_postfix")) {
    if (strlen(value) > 31) {
      fprintf(stderr, "file_postfix too long\n");
      exit(1);
    }
    strcpy(c->file_postfix, value);
  } else if (MATCH("logger", "file_duration")) {
    c->file_duration = atoi(value);
  } else {
    fprintf(stderr, "WARNING: unrecognized entry: \"%s:%s\"\n", section, name);
  }
}

int main(int argc, char *argv[]) {
  int rv;

  config_t ini;
  rv = ini_parse("logger.conf", inih_handler, &ini);
  if (rv < 0) {
    fprintf(stderr, "Error reading conf\n");
    return rv;
  }
  if ((ch_count(ini.channels) * ini.sample_rate) > 100000) {
    int nch = ch_count(ini.channels);
    int sr = ini.sample_rate;
    fprintf(stderr, "Requested more than 100kS/s.\n");
    fprintf(stderr, "\t%d channels x %d S/s = %d\n", nch, sr, nch * sr);
    fprintf(stderr, "\tCannot proceed.\n");
    return 1;
  }

  signal(SIGINT, handle_sigint);
  signal(SIGTERM, handle_sigint);

  switch_state_t sw;

  sw.pair1 = read_gpio(SW1_GPIO);
  sw.pair2 = read_gpio(SW2_GPIO);
  sw.pair3 = read_gpio(SW3_GPIO);
  sw.pair4 = read_gpio(SW4_GPIO);

  printf("Switch states:\n");
  printf("  Pair 1 (1-5): %s\n", sw.pair1 ? "DIFF" : "SE");
  printf("  Pair 2 (2-6): %s\n", sw.pair2 ? "DIFF" : "SE");
  printf("  Pair 3 (3-7): %s\n", sw.pair3 ? "DIFF" : "SE");
  printf("  Pair 4 (4-8): %s\n", sw.pair4 ? "DIFF" : "SE");

  // Convert requested data rate to actual data rate.
  double actual_rate;
  rv = mcc128_a_in_scan_actual_rate(1, ini.sample_rate, &actual_rate);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't read actual scan rate.\n");
    return rv;
  }
  if (ini.sample_rate != actual_rate) {
    fprintf(stderr, "Actual sample rate differs from requested:\n");
    fprintf(stderr, "\tactual: %f\n", actual_rate);
    fprintf(stderr, "\trequested: %f\n", ini.sample_rate);
    ini.sample_rate = actual_rate;
  }
  buf_count = actual_rate * ch_count(ini.channels);

  rv = mcc128_open(ini.address);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Can't connect to HAT %d\n", ini.address);
    return rv;
  }

  rv = mcc128_a_in_mode_write(ini.address, ini.mode);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Can't set mode.\n");
    return rv;
  }

  rv = mcc128_a_in_range_write(ini.address, ini.range);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Can't set range.\n");
    return rv;
  }

  buffer = (double *)malloc(buf_count * sizeof(double));
  wbuf = (float *)malloc(buf_count * sizeof(float));

  rv = mcc128_a_in_scan_start(ini.address, ini.channels, actual_rate,
                              actual_rate, OPTS_CONTINUOUS);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't start scan. %d\n", rv);
    if (rv == RESULT_RESOURCE_UNAVAIL)
      fprintf(stderr, "\tresource unavailable\n");
    if (rv == RESULT_BUSY)
      fprintf(stderr, "\tresource busy\n");
    return rv;
  }

  daq_info_t cfg = get_configuration(ini.address, ini.channels, actual_rate);
  print_configuration(cfg);

  while (run) {
    record(cfg, ini);
  }

  printf("stopping the service\n");
  rv = mcc128_a_in_scan_stop(ini.address);
  if (rv != RESULT_SUCCESS) {
    printf("Failed to stop the scan!\n");
    return rv;
  }
  free(buffer);

  rv = mcc128_close(0);
  if (rv != RESULT_SUCCESS) {
    fprintf(stderr, "Couldn't close HAT 0\n");
    return rv;
  }
  return 0;
}
