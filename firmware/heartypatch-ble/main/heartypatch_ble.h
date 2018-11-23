#ifndef heartypatch_ble_h
#define heartypatch_ble_h

#define GATTS_SERVICE_UUID_HR   0x180D
#define GATTS_CHAR_UUID_HR      0x2A37
#define GATTS_DESCR_UUID_TEST_A     0x3333
#define GATTS_NUM_HANDLE_HR     4

#define GATTS_SERVICE_UUID_TEMP   0x0001
#define GATTS_CHAR_UUID_TEMP      0x0002
#define GATTS_DESCR_UUID_TEST_B     0x2222

#define GATTS_SERVICE_UUID_BAT   0x180F
#define GATTS_CHAR_UUID_BAT      0x2A19
#define GATTS_DESCR_UUID_TEST_C     0x4444

#define TEST_DEVICE_NAME         "HeartyPatch"
#define TEST_MANUFACTURER_DATA_LEN  17
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

#define PROFILE_NUM 4
#define PROFILE_A_APP_ID 0
#define PROFILE_C_APP_ID 1
#define PROFILE_D_APP_ID 2

#define GATTS_TAG "HeartyPatch"

#define SENSOR_CONTACT_DETECTED 0x06
#define SENSOR_CONTACT_NOT_DETECTED 0x04

void heartypatch_ble_Init(void);

void update_ble_atts(void(*update)(uint16_t),uint16_t value);
void update_temp(uint16_t value);
void update_hr(uint16_t value);
void update_mean(int value);
void update_sdnn(uint16_t value);
void update_bat(uint8_t value);
void update_leadoff(uint8_t value);
void update_stress(uint16_t value);
void update_rmssd(uint16_t value);
void update_pnn(uint16_t value);
void update_rr(uint16_t value);
void update_beat_blr(void);
void update_arrhythmia(uint8_t value);


#endif
