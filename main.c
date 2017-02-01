#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "main.h"
#include "slUart.h"
#include "VirtualWire.h"
#include "slDS18B20.h"

uint8_t buf[VW_MAX_MESSAGE_LEN];
uint8_t buflen = VW_MAX_MESSAGE_LEN;
char wiad[VW_MAX_MESSAGE_LEN] = "";
uint8_t i;
volatile uint8_t stage = 0;

void resetBuffer(char *buffer);
void sendDataToUART(char *buffer);
uint8_t checkEndForData(char *str);
void stage0_listenRX();
void stage1_collectData(char *buffer);
uint8_t stage2_getTempFromDS18B20(char *buffer);

int main(void) {

  //init UART transmition
  slUART_SimpleTransmitInit();

  //setup VirtualWire reciever
  //recivier is set on pin PB1 (9 in arduino)
  vw_setup(2000);
  vw_rx_start();

  //start interrupts
  sei();

  //init DS18B20 temperature sensor
  //DS18B20 is set on pin PB0 (8 in arduino)
  if (slDS18B20_Init()) {
    slUART_WriteString("Error init slDS18B20\r\n");
    return 1;
  }


  slUART_WriteString("Start.\r\n");

  //initialize data
  strcpy(wiad, "");
  stage = 0;
  while (1) {
    switch (stage) {
    case 0:
      stage0_listenRX();
      break;
    case 1:
      stage1_collectData(wiad);
      break;
    case 2:
      if(stage2_getTempFromDS18B20(wiad)){
        stage = 0;
      }
      break;
    }
  }
  return 0;
}

void resetBuffer(char *buffer){
  memset(buffer, 0, VW_MAX_MESSAGE_LEN * (sizeof buffer[0]) );
}

void sendDataToUART(char *buffer){
  slUART_WriteString("\r\n");
  slUART_WriteString(buffer);
  slUART_WriteString("\r\n");
}

uint8_t checkEndForData(char *str) {
  return strchr(str, 'z') ? 1 : 0;
}

void stage0_listenRX() {
  if (vw_get_message(buf, &buflen)) {
    stage = 1;
  }
}

void stage1_collectData(char *buffer) {
  resetBuffer(buffer);
  //collect data from buffer from transmitter
  cli();
  for (i = 0; i < buflen; i++) {
    buffer[i] = buf[i];
  }
  sei();
  //if (checkEndForData(buffor)) {
  sendDataToUART(buffer);
  stage = 2;
  //}
}

uint8_t stage2_getTempFromDS18B20(char *buffer) {
  resetBuffer(buffer);
  if (slDS18B20_ReturnTemp(buffer)) {
    slUART_WriteString("Error get temperature from slDS18B20\r\n");
    return 1;
  }
  strcat(buffer, "|5.0|12|z");
  sendDataToUART(buffer);
  stage = 0;
  return 0;
}