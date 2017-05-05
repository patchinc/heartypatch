#ifndef ble_h
#define ble_h

#define GATTS_SERVICE_UUID_HR   0x180D
#define GATTS_CHAR_UUID_HR      0x2A37
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_HR     4

#define GATTS_SERVICE_UUID_TEMP   0x1809
#define GATTS_CHAR_UUID_TEMP      0x2A6E
#define GATTS_DESCR_UUID_TEST_B     0x2222
#define GATTS_NUM_HANDLE_HR     4

#define TEST_DEVICE_NAME            "ADS1292R_PROTO"
#define TEST_MANUFACTURER_DATA_LEN  17
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PROFILE_NUM 2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define GATTS_TAG "PROTO_DEMO"

#define SENSOR CONTACT_DETECTED 0x06
#define SENSOR_CONTACT_NOT_DETECTED 0x02

//void ble_indicate(int i);
void kalam_ble_Init(void);

void update_ble_atts(void(*update)(uint16_t),uint16_t value);
void update_temp(uint16_t value);
void update_hr(uint16_t value);
void update_hr(uint16_t value);
void update_rr(uint16_t value);
#endif