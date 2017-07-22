
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws.h"
#include "max30003.h"

static const char *TAG = "aws_iot_mqtt";
struct aws_data_atts aws_data_update;

static EventGroupHandle_t wifi_event_group;

const int CONNECTED_BIT1 = BIT0;

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)

static const char * DEVICE_CERTIFICATE_PATH = CONFIG_EXAMPLE_CERTIFICATE_PATH;
static const char * DEVICE_PRIVATE_KEY_PATH = CONFIG_EXAMPLE_PRIVATE_KEY_PATH;
static const char * ROOT_CA_PATH = CONFIG_EXAMPLE_ROOT_CA_PATH;

#else
#error "Invalid method for loading certs"
#endif

char HostAddress[255] = "a374lrqbppdr4m.iot.ap-southeast-1.amazonaws.com";		//enter your custom end point name of aws console
uint32_t port = 8883;														//default port for aws_iot

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,IoT_Publish_Message_Params *params, void *pData)
{
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data)
{
    ESP_LOGW(TAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if(NULL == pClient)
	{
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient))
	{
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    }
	else
	{
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);

		if(NETWORK_RECONNECTED == rc)
		{
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        }
		else
		{
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void aws_iot_task(void *param)
{
    char cPayload[200];

    int32_t i = 0;

    IoT_Error_t rc = FAILURE;

    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    IoT_Publish_Message_Params paramsQOS0;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false;
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char *)certificate_pem_crt_start;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)private_pem_key_start;

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)
    mqttInitParams.pRootCALocation = ROOT_CA_PATH;
    mqttInitParams.pDeviceCertLocation = DEVICE_CERTIFICATE_PATH;
    mqttInitParams.pDevicePrivateKeyLocation = DEVICE_PRIVATE_KEY_PATH;
#endif

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc)
	{
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;

    connectParams.pClientID = "myesp32";
    connectParams.clientIDLen = (uint16_t) strlen(connectParams.pClientID);
    connectParams.isWillMsgPresent = false;

    ESP_LOGI(TAG, "Connecting to AWS...");
    do  {
			rc = aws_iot_mqtt_connect(&client, &connectParams);

			if(SUCCESS != rc)
			{
				ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
   		} while(SUCCESS != rc);

    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);

    if(SUCCESS != rc)
	{
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

    const char *TOPIC = "HeartyPatch";				// for test only, change the topic
    const int TOPIC_LEN = strlen(TOPIC);

    ESP_LOGI(TAG, "Subscribing...");
    rc = aws_iot_mqtt_subscribe(&client, TOPIC, TOPIC_LEN, QOS0, iot_subscribe_callback_handler, NULL);

    if(SUCCESS != rc)
	{
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    }

    sprintf(cPayload, "%s : %d ", "hello from SDK", i);

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;



    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc))
	{
        rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc)
		{
            continue;
        }

        ESP_LOGI(TAG, "-->sleep");
        vTaskDelay(3000 / portTICK_RATE_MS);
	/*
		uint8_t* db;
		db = max30003_read_data();
		vTaskDelay(2/portTICK_RATE_MS);
		*/
    sprintf(cPayload, "%s\"%s\",\n", "{\n\"serial\":", "PCHP0001");
		sprintf(cPayload+strlen(cPayload), "%s\"%d\",\n", "\"heartrate\":", aws_data_update.hr);
		sprintf(cPayload+strlen(cPayload), "%s\"%d\",\n", "\"rri\":", aws_data_update.rr);
		sprintf(cPayload+strlen(cPayload), "%s\"%.2f\",\n", "\"temperature\":", aws_data_update.temperature);
		sprintf(cPayload+strlen(cPayload), "%s\"%d\",\n", "\"steps\":", aws_data_update.steps);
		sprintf(cPayload+strlen(cPayload), "%s\"%d\",\n", "\"battery\":", aws_data_update.battery);
		sprintf(cPayload+strlen(cPayload), "%s\"%d\"\n", "\"rssi\":", aws_data_update.rssi);
		sprintf(cPayload+strlen(cPayload), "%s\"%s\"\n}", "\"type\":", "HeartyPatch");

		if(aws_data_update.flag == 1) aws_data_update.flag=0;

		paramsQOS0.payloadLen = strlen(cPayload);
    rc = aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS0);
		vTaskDelay(3000 / portTICK_RATE_MS);

        if (rc == MQTT_REQUEST_TIMEOUT_ERROR)
		{
            ESP_LOGW(TAG, "publish ack not received.");
            rc = SUCCESS;
        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}


void update_AWS_atts(void(*update)(uint16_t),uint16_t value)
{
update(value);
}

void update_temp(float value)
{
aws_data_update.temperature=value;
}

void update_hr(uint16_t value)
{
aws_data_update.hr=value;
}

void update_rr(uint16_t value)
{
aws_data_update.rr=value;
}

void kalam_aws_start(void)
{

#ifdef CONFIG_MBEDTLS_DEBUG
    const size_t stack_size = 36*1024;
#else
    const size_t stack_size = 36*1024;
#endif
   xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", stack_size, NULL, 5, NULL, 1);

}
