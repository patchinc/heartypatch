#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "bt.h"
#include "driver/ledc.h"
#include "driver/uart.h"

#include "max30003.h"
#include "esp_log.h"
#include "packet_format.h"

// Selects between interrupts and poll mode
#define USE_INTERRUPTS 1
int semaphore_timeout = 10 / portTICK_PERIOD_MS;

// STATS_INTERVAL Controls frequency of stats output in units of FIFO reads
#define STATS_INTERVAL 15000

#define TAG "heartypatch:"

uint8_t SPI_TX_Buff[4];
uint8_t SPI_RX_Buff[10];
uint8_t DataPacketHeader[OVERHEAD_SIZE+PAYLOAD_SIZE];

signed long ecgdata;
unsigned long data;
char SPI_temp_32b[4];
spi_device_handle_t spi;

// counters
int tally_etag[8];      // histogram of etag values
int tally_reset = 0;    // ECG FIFO resets since start of current connection
int stats_read_count = 0;     // read count for reporting purposes, reset after status output
int tally_interrupts = 0;
int tally_interrupt_wakeup = 0;
int tally_interrupt_timeout = 0;
int read_status = 0;

unsigned int packet_sequence_id = 0;  // packet sequence ID.  Set to 0 at start and after each FIFO reset
struct timeval timestamp;             // packet timestamp (seconds and microseconds)



#ifdef USE_INTERRUPTS

/*
 * Interrupt code
 */

static xQueueHandle max30003intSem;

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t mustYield=false;
    xSemaphoreGiveFromISR(max30003intSem, &mustYield);
    tally_interrupts += 1;
}


void max30003_interrupt_disable(void)
{
    gpio_isr_handler_remove(MAX30003_INT_PIN);   // disable interrupts
}


void max30003_drdy_interrupt_enable() // DRDY isr
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = 1 << MAX30003_INT_PIN;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_intr_type(MAX30003_INT_PIN, GPIO_PIN_INTR_POSEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_remove(MAX30003_INT_PIN);
    gpio_isr_handler_add(MAX30003_INT_PIN, gpio_isr_handler, (void*) MAX30003_INT_PIN);
}

void max30003_intr_prime()
{
    xSemaphoreGive(max30003intSem);
}


void max30003_initialize_interrupt_handler() // DRDY isr
{
    ESP_LOGI(TAG, "Calling max30003_initialize_interrupt_handler\n");
    if (max30003intSem == NULL) {
        max30003intSem = xSemaphoreCreateBinary();
    };
    max30003_drdy_interrupt_enable();
    max30003_intr_prime();
    ESP_LOGI(TAG, "Interrupts initialized\n");
}

void max30003_poll_wait(void)
{
    // wait for interrupt
    if (xSemaphoreTake(max30003intSem, semaphore_timeout) == pdTRUE) {
        tally_interrupt_wakeup++;
    } else {
        tally_interrupt_timeout++;
    }
}

#else

// use static delay if interrupts not configured
void max30003_poll_wait(void)
{
    vTaskDelay(2/portTICK_RATE_MS);
    //vTaskDelay(1);
    //taskYIELD();
}

// dummy version
void max30003_intr_prime()
{
    ;
}

// dummy version
void max30003_initialize_interrupt_handler()
{
    ;
}

#endif

//This function is called (in irq context!) just before a transmission starts.
void max30003_spi_pre_transfer_callback(spi_transaction_t *t)
{
;
}


void max30003_start_timer(void)
{
    ledc_timer_config_t ledc_timer =
    {
      .bit_num = LEDC_TIMER_10_BIT, //set timer counter bit number
      .freq_hz = 32768 ,             //set frequency of pwm
      .speed_mode = LEDC_HIGH_SPEED_MODE,   //timer mode,
      .timer_num = 0    //timer index
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel =
    {
        .channel = LEDC_CHANNEL_0,
        .duty = 512,
        .gpio_num = PIN_NUM_FCLK,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };

    ledc_channel_config(&ledc_channel);
}


void MAX30003_Reg_Write (unsigned char WRITE_ADDRESS, unsigned long data)
{
  uint8_t wRegName = (WRITE_ADDRESS<<1) | WREG;

  uint8_t txData[4];

  txData[0]=wRegName;
  txData[1]=(data>>16);
  txData[2]=(data>>8);
  txData[3]=(data);

  esp_err_t ret;
  spi_transaction_t t;

  memset(&t, 0, sizeof(t));       //Zero out the transaction

  t.length=32;                 //Len is in bytes, transaction length is in bits.
  t.tx_buffer=&txData;               //Data
  ret=spi_device_transmit(spi, &t);  //Transmit!
  assert(ret==ESP_OK);            //Should have had no issues.
}


void MAX30003_ReadID(void)
{
   uint8_t SPI_TX_Buff[4];
   uint8_t SPI_RX_Buff[10];

   uint8_t Reg_address=INFO;

   SPI_TX_Buff[0] = (Reg_address<<1 ) | RREG;
   SPI_TX_Buff[1]=0x00;
   SPI_TX_Buff[2]=0x00;
   SPI_TX_Buff[3]=0x00;

   esp_err_t ret;
   spi_transaction_t t;
   memset(&t, 0, sizeof(t));       //Zero out the transaction

   t.length=32;                     //Command is 8 bits
   t.rxlength=32;
   t.tx_buffer=&SPI_TX_Buff;               //The data is the cmd itself
   t.rx_buffer=&SPI_RX_Buff;

   t.user=(void*)0;                //D/C needs to be set to 0
   ret=spi_device_transmit(spi, &t);  //Transmit!
   assert(ret==ESP_OK);            //Should have had no issues.
}


void max30003_reg_read(unsigned char WRITE_ADDRESS)
{
    uint8_t Reg_address=WRITE_ADDRESS;

    SPI_TX_Buff[0] = (Reg_address<<1 ) | RREG;
    SPI_TX_Buff[1]=0x00;
    SPI_TX_Buff[2]=0x00;
    SPI_TX_Buff[3]=0x00;

    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction

    t.length=32;
    t.rxlength=32;
    t.tx_buffer=&SPI_TX_Buff;
    t.rx_buffer=&SPI_RX_Buff;

    t.user=(void*)0;
    ret=spi_device_transmit(spi, &t);
    assert(ret==ESP_OK);            //Should have had no issues.

    SPI_temp_32b[0] = SPI_RX_Buff[1];
    SPI_temp_32b[1] = SPI_RX_Buff[2];
    SPI_temp_32b[2] = SPI_RX_Buff[3];

}


void max30003_sw_reset(void)
{
    MAX30003_Reg_Write(SW_RST,0x000000);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}


void max30003_synch(void)
{
    MAX30003_Reg_Write(SYNCH,0x000000);
}


void max30003_fifo_reset(void)
{
    MAX30003_Reg_Write(FIFO_RST,0x000000);
    max30003_intr_prime();
    tally_reset++;
}


void MAX30003_init_sequence() {
    int i;
    max30003_fifo_reset();
    for (i=0; i < 8; i++)
        tally_etag[i] = 0;

    tally_reset = 0;
    stats_read_count = 0;
    packet_sequence_id = 0;
    tally_interrupts = 0;
    tally_interrupt_wakeup = 0;
    tally_interrupt_timeout = 0;
    read_status = 0;
}


void print_counters() {
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "Tallies FIFO Reset: %d  Read Status: %d  ETAG[0]: %d  ETAG[2]: %d",
        tally_reset, read_status, tally_etag[0], tally_etag[2]);
    ESP_LOGI(TAG, "Tally for Interrupts: %d  Wakeup: %d  Timeout: %d",
        tally_interrupts, tally_interrupt_wakeup, tally_interrupt_timeout);
    /*
    ESP_LOGI(TAG, "Tallies for Individual ETag");
    for (int i=0; i < 8; i++)
        ESP_LOGI(TAG, "ETag: %x count %d", i, tally_etag[i]);
        */
    ESP_LOGI(TAG, "\n");
}


void max30003_initchip(int pin_miso, int pin_mosi, int pin_sck, int pin_cs )
{
    esp_err_t ret;

    spi_bus_config_t buscfg=
    {
        .miso_io_num=pin_miso,
        .mosi_io_num=pin_mosi,
        .sclk_io_num=pin_sck,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

    spi_device_interface_config_t devcfg=
    {
        .clock_speed_hz=4000000,             	  //Clock out at 10 MHz
        .mode=0,                               	 //SPI mode 0
        .spics_io_num=pin_cs,              		 //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=max30003_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 0);				//use 1 instead of 0 to enable dma
    assert(ret==ESP_OK);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);
    max30003_start_timer();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    max30003_initialize_interrupt_handler();
}

void max30003_configure(void)
{
    int sps_scale;

    max30003_sw_reset();
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_GEN, 0x080004);
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_CAL, 0x720000);  // 0x700000
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_EMUX,0x0B0000);
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    unsigned long ecg_config = 0x001000;    // was 0x005000
#ifdef CONFIG_SPS_128
    ecg_config = ecg_config | 0x800000;   //  d[23:22] -- RATE[0:1]: 10 for 128sps
        ESP_LOGI(TAG, "max30003_initchip setting SPS to 128");
    sps_scale = 4;
#endif
#ifdef CONFIG_SPS_256
    ecg_config = ecg_config | 0x400000;   //  d[23:22] -- RATE[0:1]: 01 for 256 sps
    ESP_LOGI(TAG, "max30003_initchip setting SPS to 256");
    sps_scale = 2;
#endif
#ifdef CONFIG_SPS_512
    //ecg_config = ecg_config | 0x000000;   //  d[23:22] -- RATE[0:1]: 00 for 512sps
    ESP_LOGI(TAG, "max30003_initchip setting SPS to 512");
    sps_scale = 1;
#endif

#ifdef CONFIG_DHPF_ENABLE
    ecg_config = ecg_config | 0x004000;   //  d[14] = enable 0.5Hz filter
    ESP_LOGI(TAG, "max30003_initchip DHPF Enabled");
#else
    ESP_LOGI(TAG, "max30003_initchip DHPF Disabled");
#endif

// below lines for debug only -- TODO: Delete me -- for debug purposes only -- 128sps
//ecg_config = ecg_config | 0x800000;   //  d[23:22] -- RATE[0:1]: 10 for 128sps
///sps_scale = 4;

// below lines for debug only -- TODO: Delete me -- for debug purposes only -- 512sps
//ecg_config = 0x001000;                //  d[23:22] -- RATE[0:1]: 00 for 128sps 512sps
///sps_scale = 1;

    MAX30003_Reg_Write(CNFG_ECG, ecg_config);
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    semaphore_timeout = semaphore_timeout * sps_scale;

    MAX30003_Reg_Write(CNFG_RTOR1,0x3fc600);
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    unsigned mngr_int = 0x4 | (SAMPLES_PER_PACKET - 1) << 19;
    MAX30003_Reg_Write(MNGR_INT, mngr_int);
    //vTaskDelay(100 / portTICK_PERIOD_MS);

	//MAX30003_Reg_Write(EN_INT,0x000400);
	MAX30003_Reg_Write(EN_INT,0xC00001); // interrupts for EINT and EOVF
    //vTaskDelay(100 / portTICK_PERIOD_MS);

    max30003_synch();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_init_sequence();
    max30003_intr_prime();
}


int max30003_read_ecg_data(int ptr)
{
      max30003_reg_read(ECG_FIFO);

      unsigned long data0 = (unsigned long) (SPI_temp_32b[0]);
      data0 = data0 <<24;
      unsigned long data1 = (unsigned long) (SPI_temp_32b[1]);
      data1 = data1 <<16;
      unsigned long data2 = (unsigned long) (SPI_temp_32b[2]);
      data2 = data2 & 0xc0;
      data2 = data2 << 8;
      data = (unsigned long) (data0 | data1 | data2);
      ecgdata = (signed long) (data);

      DataPacketHeader[ptr] = ecgdata;
      DataPacketHeader[ptr+1] = ecgdata>>8;
      DataPacketHeader[ptr+2] = ecgdata>>16;
      DataPacketHeader[ptr+3] = ecgdata>>24;

      unsigned char ecg_etag = (SPI_temp_32b[2] >> 3) & 0x7;
      tally_etag[ecg_etag]++;
      stats_read_count++;

      return SAMPLE_SIZE;
}


int max30003_read_rtor_data(int ptr)
{
    max30003_reg_read(RTOR);
    unsigned long RTOR_msb = (unsigned long) (SPI_temp_32b[0]);
    unsigned char RTOR_lsb = (unsigned char) (SPI_temp_32b[1]);
    unsigned long rtor = (RTOR_msb<<8 | RTOR_lsb);
    rtor = ((rtor >>2) & 0x3fff) ;
    unsigned int RR = (unsigned int)rtor*8 ;  //8ms

    DataPacketHeader[ptr] = RR;
    DataPacketHeader[ptr+1] = RR>>8;
    DataPacketHeader[ptr+2] = 0x00;
    DataPacketHeader[ptr+3] = 0x00;

    /*
    float hr =  60 /((float)rtor*0.008);
    unsigned int HR = (unsigned int)hr;  // type cast to int
    DataPacketHeader[ptr+4] = HR;
    DataPacketHeader[ptr+5] = HR>>8;
    DataPacketHeader[ptr+6] = 0x00;
    DataPacketHeader[ptr+7] = 0x00;
    */

    return RR_SIZE;
}


int max30003_include_packet_sequence_id(int ptr)
{
    DataPacketHeader[ptr] = packet_sequence_id;
    DataPacketHeader[ptr+1] = packet_sequence_id>>8;
    DataPacketHeader[ptr+2] = packet_sequence_id>>16;
    DataPacketHeader[ptr+3] = packet_sequence_id>>24;
    packet_sequence_id++;

    return SEQ_SIZE;
}


int max30003_include_timestamp(int ptr)
{
    gettimeofday(&timestamp, NULL);

    DataPacketHeader[ptr] = timestamp.tv_sec;
    DataPacketHeader[ptr+1] = timestamp.tv_sec>>8;
    DataPacketHeader[ptr+2] = timestamp.tv_sec>>16;
    DataPacketHeader[ptr+3] = timestamp.tv_sec>>24;

    DataPacketHeader[ptr+4] = timestamp.tv_usec;
    DataPacketHeader[ptr+5] = timestamp.tv_usec>>8;
    DataPacketHeader[ptr+6] = timestamp.tv_usec>>16;
    DataPacketHeader[ptr+7] = timestamp.tv_usec>>24;

    return TIMESTAMP_SIZE ;
}


uint8_t* max30003_read_send_data(void)
{
    int size;
    int ptr;

    read_status++;
    max30003_reg_read(STATUS);
    uint8_t status_bits = (SPI_temp_32b[0] >> 4) & 0xF;

    if ((status_bits & 0x4) == 0x4) {
        // Reset EOVF condition
        ESP_LOGI(TAG, "FIFO Reset");
        max30003_fifo_reset();
        packet_sequence_id = 0;
        return NULL;
    }

    if ((status_bits & 0x8) == 0) {
        // No data present in FIFO
        return NULL;
    }

#if CONFIG_MAX30003_STATS_ENABLE
    if (stats_read_count > STATS_INTERVAL) {
        print_counters();
        stats_read_count = 0;
    }
#endif

    DataPacketHeader[0] = PACKET_SOF1;
    DataPacketHeader[1] = PACKET_SOF2;
    DataPacketHeader[2] = PAYLOAD_SIZE_LSB;
    DataPacketHeader[3] = PAYLOAD_SIZE_MSB;
    DataPacketHeader[4] = PROTOCOL_VERSION;
    ptr = HEADER_SIZE;

    size = max30003_include_packet_sequence_id(ptr);
    ptr += size;

    size = max30003_include_timestamp(ptr);
    ptr += size;


    // Fetch RR interval data
    size = max30003_read_rtor_data(ptr);
    ptr += size;


    // Fetch ECG data
    int i;
    for (i=0; i <SAMPLES_PER_PACKET; i++) {
        size = max30003_read_ecg_data(ptr);
        ptr += size;
    }

    DataPacketHeader[ptr] = 0xF0;   // 0x00;
    DataPacketHeader[ptr+1] = PACKET_EOF;

	return DataPacketHeader;
}
