#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
//#include "esp_bt.h"
#include "bt.h"
//#include "bta_api.h"
#include "sdkconfig.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "ble.h"

uint8_t rr_service_uuid[16]= {0xd0,0x36,0xba,0x8c,0xda,0xd1,0x4c,0xae,0xb8,0x7d,0x48,0x44,0x91,0x74,0x5c,0xcd};		//custom 128bit service UUID for RR
uint8_t rr_char_uuid[16]= {0xdc,0xad,0x7f,0xc4,0x23,0x90,0x4d,0xd4,0x96,0x8d,0x0f,0x97,0x6f,0xa8,0xbf,0x01};


static void gatts_profile_hr_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_hrv_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_bat_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

uint8_t beat_flag = 0;

uint16_t attr_handle_hr = 0x002a;
uint16_t attr_handle_rr = 0x002a;
uint16_t attr_handle_bat = 0x002a;

uint16_t conn_id_indicate_hr=0;
uint16_t conn_id_indicate_rr=0;
uint16_t conn_id_indicate_bat=0;

esp_gatt_if_t gatts_if_for_hr = ESP_GATT_IF_NONE;
esp_gatt_if_t gatts_if_for_rr = ESP_GATT_IF_NONE;
esp_gatt_if_t gatts_if_for_bat = ESP_GATT_IF_NONE;

extern unsigned char DataPacketHeader[16];

struct ble_data_att
{
	uint16_t pnn;
	uint8_t bat;
	uint16_t stress;
	uint16_t hr;
	uint16_t rr;
	uint16_t rmssd;
	int mean;
	uint16_t sdnn;

}ble_data_update;

uint8_t char1_str[] = {0x11,0x22,0x33};

esp_attr_value_t gatts_demo_char1_val =
{
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
        0x02, 0x01, 0x06, 0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f, 0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};
static uint8_t raw_scan_rsp_data[] = {
        0x02, 0x01, 0x06, 0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f, 0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};

#else

static uint8_t hr_service_uuid128[32] =
{

    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0xAB, 0xCD,
};

static esp_ble_adv_data_t test_adv_data =
{
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 32,
	.p_service_uuid = hr_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
#endif

static esp_ble_adv_params_t test_adv_params =
{
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};


static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] =
{
    [PROFILE_A_APP_ID] =
	{
        .gatts_cb = gatts_profile_hr_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },

	[PROFILE_C_APP_ID] =
	{
        .gatts_cb = gatts_profile_hrv_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },

	[PROFILE_D_APP_ID] =
	{
        .gatts_cb = gatts_profile_bat_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },

};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
	{
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "Advertising start failed\n");
        }
        break;
    default:
        break;
    }
}

static void gatts_profile_hr_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event)
	{
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_HR;

        esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
#ifdef CONFIG_SET_RAW_ADV_DATA
        esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
#else
        esp_ble_gap_config_adv_data(&test_adv_data);
#endif

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_HR);
        break;

    case ESP_GATTS_READ_EVT:
		{
			ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
			esp_gatt_rsp_t rsp;
			memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
			rsp.attr_value.handle = param->read.handle;
			rsp.attr_value.len = 2;
			rsp.attr_value.value[0] = ( 0 );
			rsp.attr_value.value[1] = (uint8_t)ble_data_update.hr;

			esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
										ESP_GATT_OK, &rsp);
			break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value %08x\n", param->write.len, *(uint32_t *)param->write.value);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;
    }
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_HR;

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);

        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               &gatts_demo_char1_val, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;
        gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

		attr_handle_hr = param->add_char.attr_handle;
        esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_tab[PROFILE_A_APP_ID].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);

        break;
    }

    case ESP_GATTS_CONNECT_EVT:
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
		conn_id_indicate_hr = 	param->connect.conn_id;
		gatts_if_for_hr = gatts_if;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        esp_ble_gap_start_advertising(&test_adv_params);
		gatts_if_for_hr = ESP_GATT_IF_NONE;
        break;
    default:
        break;
    }
}

static void gatts_profile_bat_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_D_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_D_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_D_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_D_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_BAT;
        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_D_APP_ID].service_id, GATTS_NUM_HANDLE_HR);
        break;
    case ESP_GATTS_READ_EVT: {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = ((ble_data_update.bat));

        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value %08x\n", param->write.len, *(uint32_t *)param->write.value);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;
    }
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_D_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_D_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_D_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_BAT;

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_D_APP_ID].service_handle);
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_D_APP_ID].service_handle, &gl_profile_tab[PROFILE_D_APP_ID].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT:

        gl_profile_tab[PROFILE_D_APP_ID].char_handle = param->add_char.attr_handle;
        gl_profile_tab[PROFILE_D_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_D_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_D_APP_ID].service_handle, &gl_profile_tab[PROFILE_D_APP_ID].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                     NULL, NULL);
		attr_handle_bat = param->add_char.attr_handle;

        break;
    case ESP_GATTS_CONNECT_EVT:
        gl_profile_tab[PROFILE_D_APP_ID].conn_id = param->connect.conn_id;
		gatts_if_for_bat = gatts_if;
		conn_id_indicate_bat = param->connect.conn_id;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
			gatts_if_for_bat = ESP_GATT_IF_NONE;
		break;
    default:
        break;
    }
}



static void gatts_profile_hrv_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        gl_profile_tab[PROFILE_C_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_C_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_C_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;

			for(int i=0; i<16; i++){

			gl_profile_tab[PROFILE_C_APP_ID].service_id.id.uuid.uuid.uuid128[i]=rr_service_uuid[i];
		}

        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_C_APP_ID].service_id, GATTS_NUM_HANDLE_HR);
        break;
    case ESP_GATTS_READ_EVT: {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = ((ble_data_update.mean));


        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value %08x\n", param->write.len, *(uint32_t *)param->write.value);
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;
    }
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_C_APP_ID].service_handle = param->create.service_handle;
        gl_profile_tab[PROFILE_C_APP_ID].char_uuid.len = ESP_UUID_LEN_128;

		for(int i=0; i<16; i++){

				gl_profile_tab[PROFILE_C_APP_ID].char_uuid.uuid.uuid128[i]=rr_char_uuid[i];
		}

        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_C_APP_ID].service_handle);
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_C_APP_ID].service_handle, &gl_profile_tab[PROFILE_C_APP_ID].char_uuid,
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        gl_profile_tab[PROFILE_C_APP_ID].char_handle = param->add_char.attr_handle;
        gl_profile_tab[PROFILE_C_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_tab[PROFILE_C_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

		attr_handle_rr = param->add_char.attr_handle;
		esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_C_APP_ID].service_handle, &gl_profile_tab[PROFILE_C_APP_ID].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        break;
    case ESP_GATTS_CONNECT_EVT:
        gl_profile_tab[PROFILE_C_APP_ID].conn_id = param->connect.conn_id;
		gatts_if_for_rr = gatts_if;
		conn_id_indicate_rr = param->connect.conn_id;

        break;
    case ESP_GATTS_DISCONNECT_EVT:
			gatts_if_for_rr = ESP_GATT_IF_NONE;
		break;

    default:
        break;
    }
}


void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{

    if (event == ESP_GATTS_REG_EVT)
	{
        if (param->reg.status == ESP_GATT_OK)
		{
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        }
		else
		{
            ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }


    do {
        int idx;

        for (idx = 0; idx < PROFILE_NUM; idx++)
			{
				if (gatts_if == ESP_GATT_IF_NONE ||  gatts_if == gl_profile_tab[idx].gatts_if)
				{
					if (gl_profile_tab[idx].gatts_cb)
					{
						gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
					}
				}
			}
    	} while (0);
}

void update_ble_atts(void(*update)(uint16_t),uint16_t value)
{
	update(value);
}

void update_stress(uint16_t value)
{
	ble_data_update.stress=value;
}

void update_rmssd(uint16_t value)
{
	ble_data_update.rmssd=value;
}

void update_hr(uint16_t value)
{
	ble_data_update.hr=value;

}

void update_rr(uint16_t value)
{
	ble_data_update.rr=value;

}

void update_mean(int value)
{
	ble_data_update.mean=value;
}

void update_sdnn(uint16_t value)
{
	ble_data_update.sdnn=value;
}

void update_bat(uint8_t value)
{
	ble_data_update.bat=value;
	esp_ble_gatts_send_indicate(gatts_if_for_bat, conn_id_indicate_bat, attr_handle_bat, 1, &(ble_data_update.bat), false);		//updates in every 40s
}

void update_pnn(uint16_t value)
{
	ble_data_update.pnn=value;
}

void update_beat_blr(void)
{

	beat_flag = 1;
}


static void notify_task(void* arg) {

	  while (true) {

		while (gatts_if_for_hr == ESP_GATT_IF_NONE) {			//checking the connection
				vTaskDelay(5000/ portTICK_PERIOD_MS);
		}

		vTaskDelay(1500/ portTICK_PERIOD_MS);

		uint8_t value_arr_hr[2];
		uint8_t value_arr_hrv_anls[15];

		value_arr_hr[0] =( 0x10 );
		value_arr_hr[1] = (uint8_t)ble_data_update.hr;
		value_arr_hr[2] = ble_data_update.rr;
		value_arr_hr[3] = ble_data_update.rr>>8;

		value_arr_hrv_anls[0] = (ble_data_update.mean);
		value_arr_hrv_anls[1] = (ble_data_update.mean>>8);
		value_arr_hrv_anls[2] = (ble_data_update.mean>>16);
		value_arr_hrv_anls[3] = (ble_data_update.mean>>24);
		value_arr_hrv_anls[4] = (ble_data_update.sdnn);
		value_arr_hrv_anls[5] = (ble_data_update.sdnn>>8);

		value_arr_hrv_anls[6] = (ble_data_update.pnn);
 		value_arr_hrv_anls[7] = (ble_data_update.pnn>>8);
 		value_arr_hrv_anls[8] = (ble_data_update.stress);
 		value_arr_hrv_anls[9] = (ble_data_update.stress>>8);
 		value_arr_hrv_anls[10] = (ble_data_update.rmssd);
 		value_arr_hrv_anls[11] = (ble_data_update.rmssd>>8);
		value_arr_hrv_anls[12] = (uint8_t)ble_data_update.hr;

		if(beat_flag) {

				  esp_ble_gatts_send_indicate(gatts_if_for_hr, conn_id_indicate_hr, attr_handle_hr, 4, value_arr_hr, false);
				  esp_ble_gatts_send_indicate(gatts_if_for_rr, conn_id_indicate_rr, attr_handle_rr, 13, value_arr_hrv_anls, false);

				  beat_flag = 0;
		}

	}
}



void kalam_ble_Init(void)
{

	esp_err_t ret;

	//esp_bt_controller_init();

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();		//for new idf versions
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s initialize controller failed\n", __func__);
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret)
	{
        ESP_LOGE(GATTS_TAG, "%s enable controller failed\n", __func__);
        return;
    }
    ret = esp_bluedroid_init();
    if (ret)
	{
        ESP_LOGE(GATTS_TAG, "%s init bluetooth failed\n", __func__);
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret)
	{
        ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed\n", __func__);
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(PROFILE_A_APP_ID);
	esp_ble_gatts_app_register(PROFILE_C_APP_ID);
	esp_ble_gatts_app_register(PROFILE_D_APP_ID);

	xTaskCreate(notify_task, "notify_task", 1024*4, NULL, 3, NULL);

}
