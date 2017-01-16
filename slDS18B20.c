#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "slDS18B20.h"
#include "slUart.h"

uint8_t slDS18B20_Reset(){
  uint8_t i;
  
  //pull line low and wait 480us
  slDS18B20_LOW();
  slDS18B20_OUTPUT_MODE();
  _delay_us(480);

  //relase line and wait for 60us
  slDS18B20_INPUT_MODE();
  _delay_us(60);

  //store line value and wait until the completion of 480us period
  i=(slDS18B20_PIN & (1<<slDS18B20_DQ));
  _delay_us(480);

  //return the value read from the presence pulse (0=OK, 1=WRONG)
  return i;
}

uint8_t slDS18B20_WriteBit(uint8_t bit){
  cli();
  //pull line low for 1us
  slDS18B20_LOW();
  slDS18B20_OUTPUT_MODE();
  _delay_us(1);

  //if we want write 1, relase the line (if not will keep low)
  if(bit) slDS18B20_INPUT_MODE();

  //wait for 60us and release the line
  _delay_us(60);
  slDS18B20_INPUT_MODE();
  sei();
  return 0;
}


uint8_t slDS18B20_ReadBit(){
  uint8_t bit=0;
  
  cli();
  //pull line low for 1us
  slDS18B20_LOW();
  slDS18B20_OUTPUT_MODE();
  _delay_us(1);

  //Relase the line and wait for 14us
  slDS18B20_INPUT_MODE();
  _delay_us(14);

  //read line value
  if((slDS18B20_PIN & (1<<slDS18B20_DQ))){
    bit = 1;
  }

  //wait for 45us and return value
  _delay_us(45);
  sei();
  return bit;
}

uint8_t slDS18B20_ReadByte(){
  uint8_t i=8;
  uint8_t n=0;
  while(i--){
    //shift one position right and store read value
    n = n>>1;
    n|=slDS18B20_ReadBit();
  }
  return n;
}
uint8_t slDS18B20_WriteByte(uint8_t byte){
  uint8_t i=8;
  while(i--){
    //write actual bit and shift new position right to make the next bit ready
    slDS18B20_WriteBit(byte&1);
    byte = byte>>1;
  }
  return 0;
}

uint8_t slDS18B20_ReturnTemp(char *buffer){

  uint8_t  temperature[2];
  int8_t   digit;
  uint16_t decimal;

  //reset skip ROM and start temperature conversion
  if(slDS18B20_Reset()){
    slUART_WriteString("error reset\r\n");
    return 1;
  }
  slDS18B20_WriteByte(slDS18B20_CMD_SKIPROM);
  slDS18B20_WriteByte(slDS18B20_CMD_RSCRATCHPAD);

  //store temperature integer digitd and decimal digits
  temperature[0] = slDS18B20_ReadByte();
  temperature[1] = slDS18B20_ReadByte();
  if(slDS18B20_Reset()){
    return 1;
  }

  //store temperature integer digits and decimal digits
  digit = temperature[0]>>4;
  digit |= (temperature[1]&0x07)<<4;
  decimal = temperature[0]&0xf;
  decimal *= slDS18B20_DECIMAL_TEPS_12BIT;
  sprintf(buffer,"%+d.%04u", digit, decimal);

  return 0;
}