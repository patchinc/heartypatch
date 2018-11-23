#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

#include "heartypatch_max30003.h"
#include "heartypatch_ble.h"
#include "heartypatch_arrhythmia.h"

uint8_t SPI_TX_Buff[4];
uint8_t SPI_RX_Buff[10];
uint8_t DataPacketHeader[20];

signed long ecgdata;
unsigned long data;
char SPI_temp_32b[4];
int flag;
unsigned long ps;

spi_device_handle_t spi;
uint8_t rtor_detected  =0;
static xQueueHandle max30003intSem;
SemaphoreHandle_t updateRRSemaphore = NULL;
extern unsigned int global_heartRate ;

unsigned int array[MAX];
int rear = -1;
int i ;
float rmssd;
float mean_f;
float rmssd_f;
int count = 0;
float per_pnn;
int sqsum;
int hist[] = {0};
float sdnn;
float sdnn_f;
int k=0;
int min_f=0;
int max_f=0;
int max_t=0;
int min_t=0;
float pnn_f=0;
float tri =0;
float array_temp[MAX]={0.0};
extern uint8_t arrhythmiadetector;


void max30003_spi_pre_transfer_callback(spi_transaction_t *t)
{
;
}

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t mustYield=false;
    xSemaphoreGiveFromISR(max30003intSem, &mustYield);
    	    
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

void read_data(void *pvParameters)			// calls max30003read_data to update to aws_iot.
{

    while(1)
    {
        xSemaphoreTake(max30003intSem,  portMAX_DELAY);//Wait until slave is ready

        max30003_read_send_data();
        xSemaphoreGive(updateRRSemaphore);
        vTaskDelay(10/portTICK_RATE_MS);
    }
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

void max30003_initchip(int pin_miso, int pin_mosi, int pin_sck, int pin_cs )
{

    esp_err_t ret;
	//aws_data_update.flag =0;

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
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 0);
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

    MAX30003_Reg_Write(CNFG_ECG, 0x005000);  // d23 - d22 : 10 for 250sps , 00:500 sps
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(CNFG_RTOR1,0x3fc600);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    MAX30003_Reg_Write(MNGR_INT, 0x000024);  //MAX30003_Reg_Write(MNGR_INT, 0x000014);
    vTaskDelay(100 / portTICK_PERIOD_MS);
	
	  MAX30003_Reg_Write(EN_INT,0x000401);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    max30003_synch();
	
	  max30003_drdy_interrupt_enable();
}

uint8_t* max30003_read_send_data(void)
{	
    update_beat_blr();
    max30003_reg_read(RTOR);
     
    unsigned long RTOR_msb = (unsigned long) (SPI_temp_32b[0]);
    unsigned char RTOR_lsb = (unsigned char) (SPI_temp_32b[1]);

    unsigned long rtor = (RTOR_msb<<8 | RTOR_lsb);
    rtor = ((rtor >>2) & 0x3fff) ;

    float hr =  60 /((float)rtor*0.0078125);
    //float rtor_ms = ((float)rtor*0.0078125);
	  
    global_heartRate = (unsigned int)hr ; 
    HR = (unsigned int)hr;  // type cast to int
    RR = (unsigned int)((float)rtor*7.8125) ;  //8ms
    k++;
 
    update_hr(HR);
    update_rr(RR);

    if(rear == MAX-1)
    {
      for(i=0;i<(MAX-1);i++)
      {
       array[i]=array[i+1];
      }
       array[MAX-1] = RR;	  
    }else
    {		  
      rear++;
      array[rear] = RR;
    }
	
    if(k>=MAX)
    {	

      max_f = max(array);
      min_f = min(array);
      mean_f = mean(array);
      sdnn_f = sdnn_ff(array);
      pnn_f = pnn_ff(array);

      update_mean((int)(mean_f*100));
      update_sdnn((uint16_t)(sdnn_f*100));
      update_pnn((uint16_t)(per_pnn*100));


      for(i=0;i<MAX;i++)
      {
       array_temp[i] = ((float)(array[i])/1000);

      }

      challenge(array_temp);
      update_arrhythmia(arrhythmiadetector); 
    }

   return  DataPacketHeader;
}

int max(unsigned int array[])
{  
    for(i=0;i<MAX;i++)
    {
    if(array[i]>max_t)
    {
    	max_t = array[i];
    }
    }
    return max_t;
}

int min(unsigned int array[])
{   
  	min_t = max_f;
  	for(i=0;i<MAX;i++)

  	{ if(array[i]< min_t)
  		{
  		min_t = array[i];	
  		}
  	}
  	return min_t;
}

float mean(unsigned int array[])
{ 
    int sum = 0;
    float mean_rr;

    for(i=0;i<(MAX);i++)
    {
      sum = sum + array[i];
    }

    mean_rr = (((float)sum)/ MAX);	
    return mean_rr;
}	

float sdnn_ff(unsigned int array[])
{
  	int sumsdnn = 0;
  	int diff;
  	
    for(i=0;i<(MAX);i++)
    {
      diff = (array[i]-(mean_f));
      diff = diff*diff;
      sumsdnn = sumsdnn + diff;		
    }

    sdnn = (sqrt(sumsdnn/(MAX)));
    return	 sdnn;
}

float pnn_ff(unsigned int array[])
{ 
    unsigned int pnn50[MAX];
    count = 0;
    sqsum = 0;
    
    for(i=0;i<(MAX-2);i++)
    {
      pnn50[i]= abs(array[i+1] - array[i]);
      sqsum = sqsum + (pnn50[i]*pnn50[i]);
      if(pnn50[i]>50)
      {
       count = count + 1;		 
      }
    }

    per_pnn = ((float)count/MAX)*100;
    rmssd = sqrt(sqsum/(MAX-1));
    update_rmssd((uint16_t)(rmssd*100));
    return per_pnn;
}

void heartypatch_start_max30003(void)
{
    updateRRSemaphore = xSemaphoreCreateMutex();
    max30003intSem=xSemaphoreCreateBinary();
    rgbLedSem = xSemaphoreCreateBinary();
    xTaskCreate(&read_data, "read_data", 4096, NULL, 4, NULL);

}

void heartyPatch_send_data(uint8_t *dataToSend, int dataLength)
{

}
