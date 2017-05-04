#include <stdio.h>
#include "driver/i2c.h"

#define DATA_LENGTH          512  /*!<Data buffer length for test buffer*/
#define RW_TEST_LENGTH       129  /*!<Data length for r/w test, any value from 0-DATA_LENGTH*/
#define DELAY_TIME_BETWEEN_ITEMS_MS   1234 /*!< delay time between different test items */


#define I2C_BUS_NUM           I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_EXAMPLE_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define MAX30205_ADDRESS        0x24  // 8bit address converted to 7bit

// Registers
#define MAX30205_TEMPERATURE    0x00  //  get temperature ,Read only
#define MAX30205_CONFIGURATION  0x01  //
#define MAX30205_THYST          0x02  //
#define MAX30205_TOS            0x03  //

#define WRITE_BIT  I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT   I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init(int pin_i2c_sda,int pin_i2c_scl)
{
    int i2c_master_port = I2C_BUS_NUM ;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = pin_i2c_sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = pin_i2c_scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                       I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Test code to write esp-i2c-slave
 *        Master device write data to slave(both esp32),
 *        the data will be stored in slave buffer.
 *        We can read them out from slave buffer.
 *
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write n bytes + ack  | stop |
 * --------|---------------------------|----------------------|------|
 *
 */
static esp_err_t max30205_write_byte(uint8_t wr_addr, uint8_t wr_val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( MAX30205_ADDRESS << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, wr_addr, wr_val);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin( I2C_BUS_NUM , cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

float max30205_readTemperature(void)
{
    uint8_t readRaw[2] = {0};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, MAX30205_ADDRESS<< 1 | READ_BIT, ACK_CHECK_DIS);
    i2c_master_read(cmd, readRaw, 2, NACK_VAL);
    //i2c_master_read_byte(cmd, readRaw[1], NACK_VAL);

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_BUS_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

	  int16_t raw = readRaw[0] << 8 | readRaw[1];  //combine two bytes
    float temperature = raw  * 0.00390625;     // convert to temperature
	  return  temperature;
}

void max30205_initchip(int pin_i2c_sda,int pin_i2c_scl)
{
    i2c_bus_init(pin_i2c_sda,pin_i2c_scl);

    max30205_write_byte(MAX30205_CONFIGURATION, 0x00); //mode config
    max30205_write_byte(MAX30205_THYST , 		 0x00); // set threshold
    max30205_write_byte(MAX30205_TOS, 			 0x00); //
}
