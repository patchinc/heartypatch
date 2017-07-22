#ifndef aws_h
#define aws_h

#define EXAMPLE_WIFI_SSID "circuitects 1"
#define EXAMPLE_WIFI_PASS "open1234"
#define TRUE 1
#define FALSE 0

struct aws_data_atts
{

	float temperature;
	uint16_t hr;
	uint16_t rr;
	uint16_t accelerometer;
	int battery;
	int rssi;
	int steps;
	uint8_t flag;
};

void kalam_aws_start(void);
void update_AWS_atts(void(*update)(uint16_t),uint16_t value);
void update_temp(float value);
void update_hr(uint16_t value);
void update_rr(uint16_t value);

#endif
