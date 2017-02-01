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

#define LED (1 << PD7)
#define LED_TOG PORTD ^= LED
#define LED_ON PORTD |= LED
#define LED_OFF PORTD &=~ LED

uint8_t buf[VW_MAX_MESSAGE_LEN];
uint8_t buflen = VW_MAX_MESSAGE_LEN;
uint8_t *bufPointer;
char wiad[VW_MAX_MESSAGE_LEN] = "";
uint8_t i, j;
volatile uint8_t stage = 0;


uint8_t checkEndForData(char *str) {
  return strchr(str, 'z') ? 1 : 0;
}
void stage0_listenRX() {
  if (vw_get_message(buf, &buflen)) {
    stage = 1;
  }
}
void stage1_collectData(char *buffer) {
  strcpy(buffer, "");
  LED_TOG;
  //collect data from buffer from transmitter
  for (i = 0; i < buflen; i++) {
    buffer[i] = buf[i];
  }
  LED_TOG;
  //if (checkEndForData(buffor)) {
  slUART_WriteString(buffer);
  stage = 2;
  //}
}
void stage2_getTempFromDS18B20(char *buffer) {
  strcpy(buffer, "");
  if (slDS18B20_ReturnTemp(buffer)) {
    slUART_WriteString("Error get temperature from slDS18B20\r\n");
    return 1;
  }
  strcat(buffer, "|5.0|12|z");
  slUART_WriteString(buffer);
  stage = 0;
}

int main(void) {
  //set direction of port for led
  DDRD |= LED;

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


  LED_ON;
  slUART_WriteString("Start.\r\n");

  //initialize data
  strcpy(wiad, "");
  j = 0;
  LED_OFF;
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