#ifndef heartypatch_max30003_h_
#define heartypatch_max30003_h_

#define WREG 0x00
#define RREG 0x01

#define   STATUS          0x01
#define   EN_INT          0x02
#define   EN_INT2         0x03
#define   MNGR_INT        0x04
#define   MNGR_DYN        0x05
#define   SW_RST          0x08
#define   SYNCH           0x09
#define   FIFO_RST        0x0A
#define   INFO            0x0F
#define   CNFG_GEN        0x10
#define   CNFG_CAL        0x12
#define   CNFG_EMUX       0x14
#define   CNFG_ECG        0x15
#define   CNFG_RTOR1      0x1D
#define   CNFG_RTOR2      0x1E
#define   ECG_FIFO_BURST  0x20
#define   ECG_FIFO        0x21
#define   RTOR            0x25
#define   NO_OP           0x7F

#define PIN_NUM_FCLK      13
#define MAX30003_INT_PIN  27// for kalam board02
#define MAX 20
volatile unsigned int HR ,RR ;

xQueueHandle rgbLedSem;
QueueHandle_t xQueue_tcp;

void heartypatch_start_max30003(void);
void max30003_start_timer(void);
void MAX30003_ReadID(void);
void max30003_initchip(int pin_miso, int pin_mosi, int pin_sck, int pin_cs );
uint8_t* max30003_read_send_data(void );
float pnn_ff(unsigned int queue_array[MAX]);
float mean(unsigned int queue_array[MAX]);
float sdnn_ff(unsigned int queue_array[]);
int max(unsigned int array[]);
int min(unsigned int array[]);

#endif
