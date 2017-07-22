#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "bt.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/sdmmc_host.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "mdns.h"
#include "esp_log.h"
#include "kalam32.h"
#include "max30003.h"
#include "max30205.h"
#include "aws.h"
#include "kalam32_tcp_server.h"
//#include "ble.h"

//void kalam_tcp_start(void);				//added for dbg

extern xSemaphoreHandle print_mux;
char uart_data[50];
const int uart_num = UART_NUM_1;
#define BUF_SIZE  1000
 	uint8_t* db;


#define KALAM32_WIFI_SSID       "circuitects 1"
#define KALAM32_WIFI_PASSWORD   "open1234"
#define KALAM32_MDNS_ENABLED    FALSE
#define KALAM32_MDNS_NAME       "heartypatch"

#define KALAM32_TCP_ENABLED     FALSE

/* FreeRTOS event group to signal when we are connected & ready to make a request */

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT_kalam = BIT0;

#define TAG "heartypatch:"

 	uint8_t* db;
mdns_server_t * mdns = NULL;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT_kalam);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT_kalam);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void kalam32_uart_init(void)
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, 1,3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart_num, BUF_SIZE * 2, 1000, 50, NULL, 0);
}

void kalam_wifi_init(void)
{
      tcpip_adapter_init();
      wifi_event_group = xEventGroupCreate();
      ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
      ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
      wifi_config_t wifi_config = {
          .sta = {
              .ssid = KALAM32_WIFI_SSID,
              .password = KALAM32_WIFI_PASSWORD,
          },
      };
      ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
      ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
      ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
      ESP_ERROR_CHECK( esp_wifi_start() );

#if KALAM32_MDNS_ENABLED == TRUE
      esp_err_t err = mdns_init(TCPIP_ADAPTER_IF_STA, &mdns);
      ESP_ERROR_CHECK( mdns_set_hostname(mdns, KALAM32_MDNS_NAME) );
      ESP_ERROR_CHECK( mdns_set_instance(mdns, KALAM32_MDNS_NAME) );
#endif

}


void app_main(void)
{
    float max_temp=0;
    int level = 0;

    nvs_flash_init();
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);

    max30003_initchip(PIN_SPI_MISO,PIN_SPI_MOSI,PIN_SPI_SCK,PIN_SPI_CS);
    max30205_initchip();

    kalam_max30205_start();
    kalam_start_max30003();
    //kalam32_uart_init();

    kalam_wifi_init();

    //kalam_ble_Init();
    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT_kalam,
                    false, true, portMAX_DELAY);

#if KALAM32_TCP_ENABLED == TRUE
    //kalam_tcp_start();
#endif
    kalam_aws_start();

    while (true)
    {

		    gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(50/ portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_5, 0);
		    vTaskDelay(2000/ portTICK_PERIOD_MS);
    }
}
