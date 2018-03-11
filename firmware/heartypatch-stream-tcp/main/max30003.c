#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define INCLUDE_STATS 1
#define TAG "heartypatch:"
//#define SAMPLES_PER_PACKET 3
//#define SAMPLES_PER_PACKET_LSB ((SAMPLES_PER_PACKET*4) &0xFF)
//#define SAMPLES_PER_PACKET_MSB (((SAMPLES_PER_PACKET*4) >> 8) &0xFF)
#define SAMPLES_PER_PACKET 1
#define SAMPLES_PER_PACKET_LSB 0x0C
#define SAMPLES_PER_PACKET_MSB 0

#define PROTOCOL_VERSION 0x02
#define PACKET_SOF1 0x0A
#define PACKET_SOF2 0xFA
#define PACKET_EOF 0x0b

uint8_t SPI_TX_Buff[4];
uint8_t SPI_RX_Buff[10];
uint8_t DataPacketHeader[20];

signed long ecgdata;
unsigned long data;
char SPI_temp_32b[4];
spi_device_handle_t spi;

int tally_etag[8];
int tally_reset = 0;
int read_count = 0;

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
    tally_reset++;
}

void init_counters() {
    int i;
    for (i=0; i < 8; i++)
        tally_etag[i] = 0;

    tally_reset = 0;
}

void print_counters() {
    int i;
    ESP_LOGI(TAG, "\n");
    ESP_LOGI(TAG, "Tally for Reset: %d", tally_reset);
    ESP_LOGI(TAG, "Tally for ETag");
    for (i=0; i < 8; i++)
        ESP_LOGI(TAG, "ETag: %x count %d", i, tally_etag[i]);
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

    max30003_sw_reset();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_GEN, 0x080004);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_CAL, 0x720000);  // 0x700000
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_EMUX,0x0B0000);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    unsigned long ecg_config = 0x001000;    // was 0x005000
#ifdef CONFIG_SPS_128
    ecg_config = ecg_config | 0x800000;   //  d[23:22] -- RATE[0:1]: 10 for 128sps
        ESP_LOGI(TAG, "max30003_initchip setting SPS to 128");
#endif
#ifdef CONFIG_SPS_256
    ecg_config = ecg_config | 0x400000;   //  d[23:22] -- RATE[0:1]: 01 for 256 sps
    ESP_LOGI(TAG, "max30003_initchip setting SPS to 256");
#endif
#ifdef CONFIG_SPS_512
    //ecg_config = ecg_config | 0x000000;   //  d[23:22] -- RATE[0:1]: 00 for 512sps
    ESP_LOGI(TAG, "max30003_initchip setting SPS to 512");
#endif

#ifdef CONFIG_DHPF_ENABLE
    ecg_config = ecg_config | 0x004000;   //  d[14] = enable 0.5Hz filter
    ESP_LOGI(TAG, "max30003_initchip DHPF Enabled");
#else
    ESP_LOGI(TAG, "max30003_initchip DHPF Disabled");
#endif

    MAX30003_Reg_Write(CNFG_ECG, ecg_config);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_RTOR1,0x3fc600);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(MNGR_INT, 0x000004);
    vTaskDelay(100 / portTICK_PERIOD_MS);

	MAX30003_Reg_Write(EN_INT,0x000400);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    max30003_synch();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    init_counters();
}

uint8_t* max30003_read_send_data(void)
{
    max30003_reg_read(STATUS);
    uint8_t status_bits = (SPI_temp_32b[0] >> 4) & 0xF;

    if ((status_bits & 0x4) == 0x4) {
        // Reset EOVF condition
        ESP_LOGI(TAG, "FIFO Reset");
        max30003_fifo_reset();
        return NULL;
    }

    if ((status_bits & 0x8) == 0) {
        // No data present in FIFO
        return NULL;
    }

    if (read_count > 1000) {
        print_counters();
        read_count = 0;
    }

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
      unsigned char ecg_etag = (SPI_temp_32b[2] >> 3) & 0x7;
      tally_etag[ecg_etag]++;
      read_count++;

      max30003_reg_read(RTOR);
      unsigned long RTOR_msb = (unsigned long) (SPI_temp_32b[0]);
      unsigned char RTOR_lsb = (unsigned char) (SPI_temp_32b[1]);

      unsigned long rtor = (RTOR_msb<<8 | RTOR_lsb);
      rtor = ((rtor >>2) & 0x3fff) ;

      float hr =  60 /((float)rtor*0.008);

      unsigned int HR = (unsigned int)hr;  // type cast to int
      unsigned int RR = (unsigned int)rtor*8 ;  //8ms

      //DataPacketHeader[2] = 0x0C;
      //DataPacketHeader[3] = 0;


    DataPacketHeader[0] = PACKET_SOF1;
    DataPacketHeader[1] = PACKET_SOF2;
    //DataPacketHeader[2] = (SAMPLES_PER_PACKET*4) &0xFF;        // packetsize LSB was 0x0C
    //DataPacketHeader[3] = ((SAMPLES_PER_PACKET*4) >> 8) &0xFF;  // packetsize MSB was 0
    DataPacketHeader[2] = SAMPLES_PER_PACKET_LSB;
    DataPacketHeader[3] = SAMPLES_PER_PACKET_MSB;

    DataPacketHeader[4] = PROTOCOL_VERSION;

      DataPacketHeader[5] = ecgdata;
      DataPacketHeader[6] = ecgdata>>8;
      DataPacketHeader[7] = ecgdata>>16;
      DataPacketHeader[8] = ecgdata>>24;

      DataPacketHeader[9] =  RR ;
      DataPacketHeader[10] = RR >>8;
      DataPacketHeader[11] = 0x00;
      DataPacketHeader[12] = 0x00;

      DataPacketHeader[13] = HR ;
      DataPacketHeader[14] = HR >>8;
      DataPacketHeader[15] = 0x00;
      DataPacketHeader[16] = 0x00;

      DataPacketHeader[17] = 0x00;
      DataPacketHeader[18] = PACKET_EOF;
    }
	return DataPacketHeader;
}
