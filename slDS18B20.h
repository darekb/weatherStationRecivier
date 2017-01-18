#ifndef SLDS18B20_H_
#define SLDS18B20_H_

#define showDebugDataDS18B20 0

#define slDS18B20_PORT PORTB
#define slDS18B20_DDR  DDRB
#define slDS18B20_PIN  PINB
#define slDS18B20_DQ   PB0

#define slDS18B20_INPUT_MODE()  slDS18B20_DDR&=~(1<<slDS18B20_DQ)
#define slDS18B20_OUTPUT_MODE() slDS18B20_DDR|=(1<<slDS18B20_DQ)
#define slDS18B20_LOW()         slDS18B20_PORT&=~(1<<slDS18B20_DQ)
#define slDS18B20_HIGH()        slDS18B20_PORT|=(1<<slDS18B20_DQ)

#define slDS18B20_CMD_CONVERTTEMP     0x44
#define slDS18B20_CMD_RSCRATCHPAD     0xbe
#define slDS18B20_CMD_WSCRATCHPAD     0x4e
#define slDS18B20_CMD_CPYSCRATCHPAD   0x48
#define slDS18B20_CMD_RECEEPROM       0xb8
#define slDS18B20_CMD_RPWRSUPPLY      0xb4
#define slDS18B20_CMD_SEARCHROM       0xf0
#define slDS18B20_CMD_READROM         0x33
#define slDS18B20_CMD_MATCHROM        0x55
#define slDS18B20_CMD_SKIPROM         0xcc
#define slDS18B20_CMD_ALARMSEARCH     0xec

#define slDS18B20_DECIMAL_TEPS_12BIT  625

uint8_t slDS18B20_ReturnTemp(char *buffer);


#endif /* SLDS18B20_H_ */