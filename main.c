#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#ifndef F_CPU
#define F_CPU 16000000UL
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
char wiad[255] = "";
uint8_t i, j;
volatile uint8_t stage = 0;


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
void collectData(char *buffor){
  LED_TOG;
  //collect data from buffer from transmitter
  for (i = 0; i < buflen; i++) {
    buffor[j] = buf[i];
    j++;
    bufPointer++;
  }
  if (checkEndForData(buffor)) {
    stage = 2;
  }
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
//    switch(stage){
//      case 1:
//        collectData();
//        if (checkEndForData(wiad)) {
//          stage = 2;
//        }
//        break;
//      case 2:
//        collectData();
//        break;
//    }
    if (vw_get_message(buf, &buflen)) {
      stage = 1;
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
        wiad[j] = buf[i];
        j++;
        bufPointer++;
      }
#if showDebugDataMain == 1
      slUART_WriteString("one transmit datat\r\n");
      //sending data from transmitter to uart
      slUART_WriteString(wiad);
#endif
//      if (checkEndForData(wiad)) {
//#if showDebugDataMain == 1
//        slUART_WriteString("all data from transmiter get\r\n");
//#endif
//        //sending data from transmitter to uart
//        slUART_WriteString(wiad);
//        slUART_WriteString("\r\n");
//        //reset data for new loop
////        strcpy(wiad, "");
////        j = 0;
//        _delay_ms(2000);
//        //geting data temperature from DS18B20
//        getTempFromDS18B20(wiad);
//        //sending temperature measure to uart
//        slUART_WriteString(wiad);
//        slUART_WriteString("\r\n");
//
//
//        //reset data for new loop
//        strcpy(wiad, "");
//        j = 0;
//
//      }
      LED_TOG;
    }
  }
  return 0;
}