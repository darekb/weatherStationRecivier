#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#ifndef F_CPU
#define F_CPU 12000000UL
#endif

#define showDebugDataMain 1

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
char wiad[39] = "";
uint8_t i, j;
char tmp1[30] = "";


uint8_t checkEndForData(char *str) {
  return strchr(str, 'z') ? 1 : 0;
}

void getTempFromDS18B20(char *wiad) {
  if (slDS18B20_ReturnTemp(wiad)) {
#if showDebugDataMain == 1
    slUART_WriteString("Error get temperature from slDS18B20\r\n");
#endif
  }
  strcat(wiad, "|5.0|12|z");
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

  //init DS18B20 temperature sensor
  //DS18B20 is set on pin PB0 (8 in arduino)
  if (slDS18B20_Init()) {
#if showDebugDataMain == 1
    slUART_WriteString("Error init slDS18B20\r\n");
#endif
    return 1;
  }

  //start 
  sei();

  LED_ON;
#if showDebugDataMain == 1
  slUART_WriteString("Start.\r\n");
#endif

  //initialize data 
  bufPointer = buf;
  strcpy(wiad, "");
  j = 0;

  while (1) {
    LED_OFF;
    if (vw_get_message(buf, &buflen)) {
      //reset pointer
      bufPointer = buf;
#if showDebugDataMain == 1
      slUART_WriteString("\r\nmessage ok\r\n");
      slUART_WriteString(wiad);
      slUART_WriteString("\r\n");
#endif
      LED_TOG;
      //collect data from buffer from transmitter
      for (i = 0; i < buflen; i++) {
        wiad[j] = *bufPointer;
        j++;
        bufPointer++;
      }
      if (checkEndForData(wiad)) {
#if showDebugDataMain == 1
        slUART_WriteString("all data from transmiter get\r\n");
#endif
        //sending data from transmitter to uart
        slUART_WriteString(wiad);
        slUART_WriteString("\r\n");

        //after getting data from transmitter geting data temperature from DS18B20
        getTempFromDS18B20(wiad);
        //sending temperature measure to uart 
        slUART_WriteString(wiad);
        slUART_WriteString("\r\n");

        //reset data for new loop
        strcpy(wiad, "");
        j = 0;

      }
      LED_TOG;
    }
  }
  return 0;
}