
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "heartypatch_adc.h"
#include "heartypatch_ble.h"


#define ADC1_CHANNEL (0)

volatile int adc_val=0;
int battery=0;
float bat;
static bool startup_flag = true;
static int bat_prev=100;
QueueHandle_t xQueue_battery;
int bt_rem = 0;
static uint8_t bat_percent = 100;

void adc1task(void* arg)
{
	int bat_count=0;
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(ADC1_CHANNEL,ADC_ATTEN_11db);
	vTaskDelay(500/portTICK_PERIOD_MS);
	while(1)
	{
				adc_val=adc1_get_voltage(ADC1_CHANNEL);
				//bat=(1.2424*adc_val)*(REFERENCE_VOLT/4096)		
				battery += adc_val;
				if(bat_count == 9){						//taking average of 10 values.
				battery = (battery/10);
				
				battery=((battery*2)-400);
					
		if (battery > 4100)
					{
        				battery = 4100;
        			}
        			else if(battery < 3600 )
              		{
        				battery = 3600;
        			}

					if (startup_flag == true)
					{
						bat_prev = battery;
						startup_flag = false;
						
					}
		
        			bt_rem = (battery % 100);									
		
        			if(bt_rem>80 && bt_rem < 99 && (bat_prev != 0))
             		{
        				battery = bat_prev;
        			}

				
        			bat_prev=battery;

        			if((battery/100)>=41){
        				battery = 100;
        			}
        			else if((battery/100)==40){
        				battery = 80;
        			}
        			else if((battery/100)==39){
        				battery = 60;
        			}
        			else if((battery/100)==38){
        				battery=45;
        			}
        			else if((battery/100)==37){
        				battery=30;
        			}
        			else if((battery/100)<=36){
        				battery = 20;
        			}
		  bat_percent = (uint8_t) battery;
		  update_bat(bat_percent	);		
		  bat_count=0;
  		  battery=0;
		}
		else
		  bat_count++;

		  vTaskDelay(4000/portTICK_PERIOD_MS);
    }
}

void heartypatch_adc_start(void)
{
	xQueue_battery = xQueueCreate(2, sizeof( int ));
	if( xQueue_battery==NULL )
	{
		printf( "Failed to create xQueue_battery..!");
	}
	
	xTaskCreate(adc1task, "adc1task", 4096, NULL, 2, NULL);
}
