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
#include "bt.h"				//use this for esp-idf versions below 3.0
//#include "esp_bt.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/sdmmc_host.h"

#include "mdns.h"
#include "esp_log.h"
#include "kalam32.h"
#include "max30003.h"
#include "kalam32_tcp_server.h"
#include "ble.h"
#include "hpadc.h"

#define TAG "heartypatch:"
#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)
#define KALAM32_MDNS_ENABLED    FALSE
#define KALAM32_MDNS_NAME       "heartypatch"
#define BUF_SIZE  1000

#define HRV_BADGE

#define LOW 0
#define HIGH 1

extern xSemaphoreHandle print_mux;
char uart_data[50];
const int uart_num = UART_NUM_1;
uint8_t* db;
volatile bool lead_on = false;
extern uint8_t max30003_Status_Reg[5] ;

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT_kalam = BIT0;

uint8_t* db;
unsigned int global_heartRate ;

extern QueueHandle_t xQueue_tcp;

void protocentral_init_apa102(void);
void apa102_show(uint8_t numof_leds, uint32_t rbg_val);
void apa102_setcolours(uint8_t numof_leds, uint32_t rbg_val, uint8_t brightness);
void transfer(uint8_t b);

#define DATA_PIN   26
#define CLOCK_PIN   25

inline uint32_t setrgb_val(uint8_t r, uint8_t g, uint8_t b)
{
  uint32_t c;

  c  = (uint32_t) r<<16;
  c |= (uint32_t)g<<8;
  c |= (uint32_t)b;

  return c;
}

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
              .ssid = CONFIG_WIFI_SSID,
              .password = CONFIG_WIFI_PASSWORD,
          },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

#if KALAM32_MDNS_ENABLED == TRUE
    esp_err_t err = mdns_init();
    ESP_ERROR_CHECK( mdns_hostname_set(KALAM32_MDNS_NAME) );
    ESP_ERROR_CHECK( mdns_instance_name_set(KALAM32_MDNS_NAME) );
#endif

}

void rainbow(void *pvParameters)
{


  while (true){

    max30003_read_status(STATUS);
    if((max30003_Status_Reg[1] & 0x10) )
    {
      lead_on = false;
      printf("leadoff\n");
    }
    else
    {
      lead_on = true;
      max30003_synch();
      printf("leadon\n");
    }

    if(lead_on)
    {
        if(true)
        {
        }
    }
  }
}

void app_main(void)
{
    // Initialize NVS.
	  esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    gpio_pad_select_gpio(CLOCK_PIN);
  	gpio_set_direction(CLOCK_PIN, GPIO_MODE_OUTPUT);

  	gpio_pad_select_gpio(DATA_PIN);
  	gpio_set_direction(DATA_PIN, GPIO_MODE_OUTPUT);

  	gpio_pad_select_gpio(14);
  	gpio_set_direction(14, GPIO_MODE_OUTPUT);
  	gpio_set_level(14, LOW);

	xQueue_tcp = xQueueCreate(20, sizeof( struct Tcp_Message *));
	if( xQueue_tcp==NULL )
	{
		ESP_LOGI(TAG, "Failed to create Queue..!");
	}

    max30003_initchip(PIN_SPI_MISO,PIN_SPI_MOSI,PIN_SPI_SCK,PIN_SPI_CS);

    kalam_start_max30003();
    kalam32_adc_start();

    //uint32_t rgb_val = setrgb_val(0x1F,0x0,0x00);//printf("%x\n",rgb_val );
		uint8_t brightness = 10;
		uint8_t numofleds = 144;

		//apa102_setcolours(numofleds, rgb_val, brightness);

	vTaskDelay(2000/ portTICK_PERIOD_MS);		//give sometime for max to settle

#ifdef CONFIG_BLE_MODE_ENABLE
	kalam_ble_Init();
#endif


    xTaskCreate(rainbow, "ws2812 rainbow demo", 4096, NULL, 10, NULL);
#ifdef CONFIG_WIFIMODE_ENABLE					//configure the ssid/password under makemenuconfig/heartypatch configuration.
	kalam_wifi_init();
    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT_kalam,false, true, portMAX_DELAY);

	/*only hr and rr is sending through tcp, to plot ECG, you need to configure the max30003 to read ecg data */
	vTaskDelay(500/ portTICK_PERIOD_MS);
    kalam_tcp_start();
#endif

}

void transfer(uint8_t b)
{
  gpio_set_level(DATA_PIN, b >> 6 & 1);
  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 5 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 4 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 3 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 2 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 1 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

  gpio_set_level(DATA_PIN, b >> 0 & 1);

  gpio_set_level(CLOCK_PIN, HIGH);

  gpio_set_level(CLOCK_PIN, LOW);

}

void sendcolor(uint32_t rbg_val , uint8_t brightness)
{
  transfer(0b11100000 | brightness);
  transfer(rbg_val);
  transfer(rbg_val>>8);
  transfer(rbg_val>>16);
}

void init(void)
{
  gpio_set_level(DATA_PIN, LOW);
  gpio_set_level(DATA_PIN, HIGH);
}

void start_frame(void){

  transfer(0);
  transfer(0);
  transfer(0);
  transfer(0);
}

void endframe(uint8_t numof_leds){

  transfer(0xFF);
  for (uint16_t i = 0; i < 5 + numof_leds / 16; i++)
  {
    transfer(0);
  }
}

void apa102_setcolours(uint8_t numof_leds, uint32_t rbg_val, uint8_t brightness){

  init();
  start_frame();

  for(uint16_t i = 0; i < numof_leds; i++)
  {
    sendcolor(rbg_val, brightness);
  }

  endframe(numof_leds);
}
