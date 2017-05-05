#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
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
#include <sys/socket.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"


#include "mdns.h"
#include "esp_log.h"
#include "kalam32.h"
#include "max30003.h"
#include "max30205.h"
#include "ble.h"


extern xSemaphoreHandle print_mux;

char uart_data[50];
const int uart_num = UART_NUM_1;
#define BUF_SIZE  1000
char HostAddress[255] = AWS_IOT_MQTT_HOST;
uint32_t port = AWS_IOT_MQTT_PORT;

#define HEARTYPATCH_TCP
/*********************** START TCP Server Code *****************/

int connectedflag = 0;
int total_data = 0;

/*socket*/
static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;

/*AP info and tcp_server info*/
#define DEFAULT_SSID CONFIG_TCP_PERF_WIFI_SSID
#define DEFAULT_PWD CONFIG_TCP_PERF_WIFI_PASSWORD
#define DEFAULT_PORT 4567
#define DEFAULT_SERVER_IP CONFIG_TCP_PERF_SERVER_IP
#define DEFAULT_PKTSIZE 150 //CONFIG_TCP_PERF_PKT_SIZE
#define MAX_STA_CONN 1 //how many sta can be connected(AP mode)
/*test options*/
#define ESP_WIFI_MODE_AP CONFIG_TCP_PERF_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define ESP_TCP_MODE_SERVER CONFIG_TCP_PERF_SERVER //TRUE:server FALSE:client
#define ESP_TCP_PERF_TX CONFIG_TCP_PERF_TX //TRUE:send FALSE:receive
#define ESP_TCP_DELAY_INFO CONFIG_TCP_PERF_DELAY_DEBUG //TRUE:show delay time info

#define KALAM32_WIFI_SSID     "circuitects 1"
#define KALAM32_WIFI_PASSWORD "open1234"

#define PACK_BYTE_IS 100
#define TAG "heartypatch:"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;


mdns_server_t * mdns = NULL;
//send data
void send_data(void *pvParameters)
{
    int len = 0;
    uint8_t* db;
    char databuff[DEFAULT_PKTSIZE];
    memset(databuff, PACK_BYTE_IS, DEFAULT_PKTSIZE);
    db=&databuff;
    vTaskDelay(100/portTICK_RATE_MS);
    ESP_LOGI(TAG, "start sending...");
    while(1)
    {
        db = max30003_read_data();
          //max30003_read_send_data();
  	     //send function
      	len = send(connect_socket, db, 19, 0);
        vTaskDelay(2/portTICK_RATE_MS);

    }
}


char* tcpip_get_reason(int err)
{
    switch (err) {
	case 0:
	    return "reason: other reason";
	case ENOMEM:
	    return "reason: out of memory";
	case ENOBUFS:
	    return "reason: buffer error";
	case EWOULDBLOCK:
	    return "reason: timeout, try again";
	case EHOSTUNREACH:
	    return "reason: routing problem";
	case EINPROGRESS:
	    return "reason: operation in progress";
	case EINVAL:
	    return "reason: invalid value";
	case EADDRINUSE:
	    return "reason: address in use";
	case EALREADY:
	    return "reason: conn already connected";
	case EISCONN:
	    return "reason: conn already established";
	case ECONNABORTED:
	    return "reason: connection aborted";
	case ECONNRESET:
	    return "reason: connection is reset";
	case ENOTCONN:
	    return "reason: connection closed";
	case EIO:
	    return "reason: invalid argument";
	case -1:
	    return "reason: low level netif error";
	default:
	    return "reason not found";
    }
}

int show_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    ESP_LOGI(TAG, "socket error %d reason: %s", result, tcpip_get_reason(result));
    return result;
}

int check_socket_error_code()
{
    int ret;
#if ESP_TCP_MODE_SERVER
    ESP_LOGI(TAG, "check server_socket");
    ret = show_socket_error_code(server_socket);
    if(ret == ECONNRESET)
	return ret;
#endif
    ESP_LOGI(TAG, "check connect_socket");
    ret = show_socket_error_code(connect_socket);
    if(ret == ECONNRESET)
	return ret;
    return 0;
}

void close_socket()
{
    close(connect_socket);
    close(server_socket);
}

//use this esp32 as a tcp server. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_server()
{
    ESP_LOGI(TAG, "server socket....port=%d\n", DEFAULT_PORT);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
    	 show_socket_error_code(server_socket);
	     return ESP_FAIL;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
    	 show_socket_error_code(server_socket);
	     close(server_socket);
	      return ESP_FAIL;
    }
    if (listen(server_socket, 5) < 0)
    {
    	  show_socket_error_code(server_socket);
	      close(server_socket);
	      return ESP_FAIL;
    }

    connect_socket = accept(server_socket, (struct sockaddr*)&client_addr, &socklen);
    if (connect_socket<0)
    {
    	  show_socket_error_code(connect_socket);
	      close(server_socket);
	      return ESP_FAIL;
    }
    /*connection established，now can send/recv*/
    ESP_LOGI(TAG, "tcp connection established!");
    return ESP_OK;
}

//this task establish a TCP connection and receive data from TCP
static void tcp_conn(void *pvParameters)
{
    ESP_LOGI(TAG, "task tcp_conn start.");

    /*create tcp socket*/
    int socket_ret;

    vTaskDelay(3000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "create_tcp_server.");
    socket_ret=create_tcp_server();
    if(ESP_FAIL == socket_ret)
    {
    	ESP_LOGI(TAG, "create tcp socket error,stop.");
    	vTaskDelete(NULL);
    }

    /*create a task to tx/rx data*/
    TaskHandle_t tx_rx_task;
    xTaskCreate(&send_data, "send_data", 4096, NULL, 4, &tx_rx_task);

    while (1)
    {
      	vTaskDelay(3000 / portTICK_RATE_MS);//every 3s
  	    int err_ret = check_socket_error_code();
  	    if (err_ret == ECONNRESET)
        {
      		ESP_LOGI(TAG, "disconnected... stop.");
          close_socket();
          socket_ret=create_tcp_server();
          //break;
  	    }
    }

    close_socket();
    vTaskDelete(tx_rx_task);
    vTaskDelete(NULL);
}
/*********************** END TCP Server Code *****************/

//this task establish a TCP connection and receive data from TCP
static void task_mqtt(void *pvParameters)
{

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

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "Waiting for IP...");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "IP Received !");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
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
              .ssid = KALAM32_WIFI_SSID,
              .password = KALAM32_WIFI_PASSWORD,
          },
      };
      ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
      ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
      ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
      ESP_ERROR_CHECK( esp_wifi_start() );
}

static void initialise_wifi(void)
{

}

void app_main(void)
{
    float max_temp=0;

    nvs_flash_init();

    //kalam32_uart_init();
    kalam_wifi_init();
	kalam_ble_Init();

    max30003_initchip(PIN_SPI_MISO,PIN_SPI_MOSI,PIN_SPI_SCK,PIN_SPI_CS);
    //max30205_initchip(PIN_I2C_SDA,PIN_I2C_SCL);

    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    int level = 0;

    esp_err_t err = mdns_init(TCPIP_ADAPTER_IF_STA, &mdns);

    ESP_ERROR_CHECK( mdns_set_hostname(mdns, "heartypatch") );
    ESP_ERROR_CHECK( mdns_set_instance(mdns, "heartypatch") );

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                    false, true, portMAX_DELAY);
#ifdef HEARTYPATCH_TCP
    xTaskCreate(&tcp_conn, "tcp_conn", 4096, NULL, 5, NULL);
#else

#endif
	
	print_mux = xSemaphoreCreateMutex();   
    i2c_example_master_init();
    xTaskCreate(i2c_read_temp_max30205, "readTemperature", 1024 * 2, (void* ) 0, 10, NULL);

    while (true)
    {
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(50/ portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(2000/ portTICK_PERIOD_MS);

        //max_temp=max30205_readTemperature();
        //printf("Temp: %f\n", max_temp);

        //MAX30003_ReadID();
        //max30003_read_send_data();
        //uart_write_bytes(uart_num, (const char *) uart_data, 25);
        //uart_tx_chars(uart_num, (const char *) uart_data, 5);
        //putc('a',stdout);
    }
}
