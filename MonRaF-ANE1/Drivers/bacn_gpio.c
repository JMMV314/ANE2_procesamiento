
#include <errno.h>
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "bacn_gpio.h"

/* Request to change output pin selected*/
static struct gpiod_line_request *
request_output_line(const char *chip_path, unsigned int offset,
		    enum gpiod_line_value value, const char *consumer)
{
	struct gpiod_request_config *req_cfg = NULL;
	struct gpiod_line_request *request = NULL;
	struct gpiod_line_settings *settings;
	struct gpiod_line_config *line_cfg;
	struct gpiod_chip *chip;
	int ret;

	chip = gpiod_chip_open(chip_path);
	if (!chip)
		return NULL;

	settings = gpiod_line_settings_new();
	if (!settings)
		goto close_chip;

	gpiod_line_settings_set_direction(settings,
					  GPIOD_LINE_DIRECTION_OUTPUT);
	gpiod_line_settings_set_output_value(settings, value);

	line_cfg = gpiod_line_config_new();
	if (!line_cfg)
		goto free_settings;

	ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1,
						  settings);
	if (ret)
		goto free_line_config;

	if (consumer) {
		req_cfg = gpiod_request_config_new();
		if (!req_cfg)
			goto free_line_config;

		gpiod_request_config_set_consumer(req_cfg, consumer);
	}

	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

	gpiod_request_config_free(req_cfg);

free_line_config:
	gpiod_line_config_free(line_cfg);

free_settings:
	gpiod_line_settings_free(settings);

close_chip:
	gpiod_chip_close(chip);

	return request;
}

static struct gpiod_line_request *request_input_line(const char *chip_path,
						     unsigned int offset,
						     const char *consumer)
{
	struct gpiod_request_config *req_cfg = NULL;
	struct gpiod_line_request *request = NULL;
	struct gpiod_line_settings *settings;
	struct gpiod_line_config *line_cfg;
	struct gpiod_chip *chip;
	int ret;

	chip = gpiod_chip_open(chip_path);
	if (!chip)
		return NULL;

	settings = gpiod_line_settings_new();
	if (!settings)
		goto close_chip;

	gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
	gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_BOTH);
	gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_DISABLED);

	line_cfg = gpiod_line_config_new();
	if (!line_cfg)
		goto free_settings;

	ret = gpiod_line_config_add_line_settings(line_cfg, &offset, 1,
						  settings);
	if (ret)
		goto free_line_config;

	if (consumer) {
		req_cfg = gpiod_request_config_new();
		if (!req_cfg)
			goto free_line_config;

		gpiod_request_config_set_consumer(req_cfg, consumer);
	}

	request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

	gpiod_request_config_free(req_cfg);

free_line_config:
	gpiod_line_config_free(line_cfg);

free_settings:
	gpiod_line_settings_free(settings);

close_chip:
	gpiod_chip_close(chip);

	return request;
}

uint8_t status_LTE(void)
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = STATUS;

	struct gpiod_line_request *request;
	enum gpiod_line_value value;
	int ret;

	request = request_input_line(chip_path, line_offset,
				     "status-LTE");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}

	value = gpiod_line_request_get_value(request, line_offset);

	/* not strictly required here, but if the app wasn't exiting... */
	gpiod_line_request_release(request);

	return value;
}

uint8_t power_ON_LTE(void)
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = PWR_MODULE;

	enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	struct gpiod_line_request *request;

	request = request_output_line(chip_path, line_offset, value,
				      "power-LTE");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	
	printf("Turn on LTE\n", line_offset);		
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
	sleep(2);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);

	gpiod_line_request_release(request);

	return EXIT_SUCCESS;
}

uint8_t power_OFF_LTE(void)
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = PWR_MODULE;

	enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	struct gpiod_line_request *request;

	request = request_output_line(chip_path, line_offset, value,
				      "power-LTE");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	
	printf("Turn off LTE\n");		
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
	sleep(2);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);

	gpiod_line_request_release(request);

	return EXIT_SUCCESS;
}

uint8_t reset_LTE(void) 
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = PWR_MODULE;

	enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	struct gpiod_line_request *request;

	request = request_output_line(chip_path, line_offset, value,
				      "reset-LTE");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	
	printf("Reset LTE\n");		
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
	sleep(2);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);
	sleep(3);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
	sleep(2);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);

	gpiod_line_request_release(request);

	return EXIT_SUCCESS;
}

uint8_t switch_ANTENNA(bool RF) 
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = ANTENNA_SEL;

	enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	struct gpiod_line_request *request;

	request = request_output_line(chip_path, line_offset, value,
				      "switch-ANTENNA");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	
	if(RF) {
		gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
		printf("RF1 ON RF2 OFF\n");
	} else { 
		gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);
		printf("RF1 OFF RF2 ON\n");
	}
	
	gpiod_line_request_release(request);

	return EXIT_SUCCESS;
}

uint8_t real_time(void)
{
	static const char *const chip_path = "/dev/gpiochip0";
	static const unsigned int line_offset = 16;

	enum gpiod_line_value value = GPIOD_LINE_VALUE_ACTIVE;
	struct gpiod_line_request *request;

	request = request_output_line(chip_path, line_offset, value,
				      "realTime");
	if (!request) {
		fprintf(stderr, "failed to request line: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	
	printf("REal Time test\n", line_offset);		
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
	gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);

	gpiod_line_request_release(request);

	return EXIT_SUCCESS;

}