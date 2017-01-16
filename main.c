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
uint8_t *buflenPointer = &buflen;
char wiad[39] = "";
uint8_t i,j;
char tmp1[30] = "";


uint8_t checkEndForData(char *str){
  return strchr(str, 'z')?1:0;
}

void getTempFromDS18B20(){
  char wiad[39] = "";
  if(slDS18B20_ReturnTemp(wiad)){
    #if showDebugDataMain == 1
      slUART_WriteString("Error get temperature from slDS18B20\r\n");
    #endif
  }
  strcat(wiad,"|5.0|12|z");
  slUART_WriteString(wiad);
}

int main(void) {
  DDRD |= LED;
  slUART_SimpleTransmitInit();
  vw_setup(2000);   // Bits per sec
  vw_rx_start();//przestować czy będzie działać
  sei();
  LED_ON;
#if showDebugDataMain == 1
  slUART_WriteString("Start.\r\n");
  sprintf(tmp1, "VW_MAX_MESSAGE_LEN: %d \r\n", VW_MAX_MESSAGE_LEN);
  slUART_WriteString(tmp1);
#endif

  bufPointer = buf;
  strcpy(wiad, "");
  j=0;
  while (1) {
    LED_OFF;
    if (vw_get_message(buf, &buflen)) {
      bufPointer = buf;
#if showDebugDataMain == 1
      slUART_WriteString("message ok\r\n");
      slUART_WriteString(wiad);
      slUART_WriteString("\r\n");
#endif
      LED_TOG;
      for (i = 0; i < buflen; i++) {
        wiad[j] = *bufPointer;
        j++;
        bufPointer++;
      }
      if(checkEndForData(wiad)){
        slUART_WriteString("all data get\r\n");
        slUART_WriteString(wiad);
        getTempFromDS18B20();
        strcpy(wiad, "");
        j=0;
        slUART_WriteString("\r\n");
      }
      LED_TOG;
//#if showDebugDataMain == 1
//      slUART_WriteString("\r\nRead value: ");
//      slUART_WriteString(wiad);
//      slUART_WriteString("\r\n");
//#endif
    }
  }
  return 0;
}