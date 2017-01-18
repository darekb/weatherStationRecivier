// VirtualWire.cpp
//
// Virtual Wire implementation for Arduino
//
// Author: Mike McCauley (mikem@airspayce.com)
// Copyright (C) 2008 Mike McCauley
// $Id: VirtualWire.cpp,v 1.18 2014/03/26 01:09:36 mikem Exp mikem $

#include "VirtualWire.h"
#include "slUart.h"
#include <util/crc16.h>

//	Platform specific dependencies
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdbool.h>

//	Define digitalRead, digitalWrite and digital pins for Arduino like platforms
#define __COMB(a, b, c) (a##b##c)
#define _COMB(a, b, c) __COMB(a,b,c)

#define vw_pinSetup()\
        VW_TX_DDR   |=  (1<<VW_TX_PIN);

#define vw_digitalWrite_ptt(value)

#define vw_digitalWrite_tx(value)\
      ((value) ? (VW_TX_PORT |= (1<<VW_TX_PIN)) : (VW_TX_PORT &= ~(1<<VW_TX_PIN)))

#define vw_delay_1ms()\
    _delay_ms(1)

#define VW_TIMER_VECTOR _COMB(TIMER,VW_TIMER_INDEX,_COMPA_vect)

#define vw_timerSetup(speed) \
      uint16_t nticks; \
      uint8_t prescaler; \
      prescaler = vw_timer_calc(speed, (uint16_t)-1, &nticks);\
      if (!prescaler) return; \
      _COMB(TCCR,VW_TIMER_INDEX,A)= 0; \
      _COMB(TCCR,VW_TIMER_INDEX,B)= _BV(WGM12); \
      _COMB(TCCR,VW_TIMER_INDEX,B)|= prescaler; \
      _COMB(OCR,VW_TIMER_INDEX,A)= nticks; \
      _COMB(TI,MSK,VW_TIMER_INDEX)|= _BV(_COMB(OCIE,VW_TIMER_INDEX,A));

#define vw_event_tx_done()

static uint8_t vw_tx_buf[(VW_MAX_MESSAGE_LEN * 2) + VW_HEADER_LEN]
    = {0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x38, 0x2c};

// Number of symbols in vw_tx_buf to be sent;
static uint8_t vw_tx_len = 0;

// Index of the next symbol to send. Ranges from 0 to vw_tx_len
static uint8_t vw_tx_index = 0;

// Bit number of next bit to send
static uint8_t vw_tx_bit = 0;

// Sample number for the transmitter. Runs 0 to 7 during one bit interval
static uint8_t vw_tx_sample = 0;

// Flag to indicated the transmitter is active
static volatile uint8_t vw_tx_enabled = 0;

// Total number of messages sent
static uint16_t vw_tx_msg_count = 0;


// 4 bit to 6 bit symbol converter table
// Used to convert the high and low nybbles of the transmitted data
// into 6 bit symbols for transmission. Each 6-bit symbol has 3 1s and 3 0s 
// with at most 3 consecutive identical bits
static uint8_t symbols[] =
    {
        0xd, 0xe, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
        0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
    };

// Compute CRC over count bytes.
// This should only be ever called at user level, not interrupt level
uint16_t vw_crc(uint8_t *ptr, uint8_t count) {
  uint16_t crc = 0xffff;

  while (count-- > 0)
    crc = _crc_ccitt_update(crc, *ptr++);
  return crc;
}

// Convert a 6 bit encoded symbol into its 4 bit decoded equivalent
uint8_t vw_symbol_6to4(uint8_t symbol) {
  uint8_t i;
  uint8_t count;

  // Linear search :-( Could have a 64 byte reverse lookup table?
  // There is a little speedup here courtesy Ralph Doncaster:
  // The shortcut works because bit 5 of the symbol is 1 for the last 8
  // symbols, and it is 0 for the first 8.
  // So we only have to search half the table
  for (i = (symbol >> 2) & 8, count = 8; count--; i++)
    if (symbol == symbols[i]) return i;

  return 0; // Not found
}




// Common function for setting timer ticks @ prescaler values for speed
// Returns prescaler index into {0, 1, 8, 64, 256, 1024} array
// and sets nticks to compare-match value if lower than max_ticks
// returns 0 & nticks = 0 on fault
uint8_t vw_timer_calc(uint16_t speed, uint16_t max_ticks, uint16_t *nticks) {
  // Clock divider (prescaler) values - 0/3333: error flag
  uint16_t prescalers[] = {0, 1, 8, 64, 256, 1024, 3333};
  uint8_t prescaler = 0; // index into array & return bit value
  unsigned long ulticks; // calculate by ntick overflow

  // Div-by-zero protection
  if (speed == 0) {
    // signal fault
    *nticks = 0;
    return 0;
  }

  // test increasing prescaler (divisor), decreasing ulticks until no overflow
  for (prescaler = 1; prescaler < 7; prescaler += 1) {
    // Integer arithmetic courtesy Jim Remington
    // 1/Amount of time per CPU clock tick (in seconds)
    unsigned long inv_clock_time = F_CPU / ((unsigned long) prescalers[prescaler]);
    // 1/Fraction of second needed to xmit one bit
    unsigned long inv_bit_time = ((unsigned long) speed) * 8;
    // number of prescaled ticks needed to handle bit time @ speed
    ulticks = inv_clock_time / inv_bit_time;

    // Test if ulticks fits in nticks bitwidth (with 1-tick safety margin)
    if ((ulticks > 1) && (ulticks < max_ticks)) {
      break; // found prescaler
    }
    // Won't fit, check with next prescaler value

  }

  // Check for error
  if ((prescaler == 6) || (ulticks < 2) || (ulticks > max_ticks)) {
    // signal fault
    *nticks = 0;
    return 0;
  }

  *nticks = ulticks;
  return prescaler;
}



void vw_setup(uint16_t speed) {
  vw_pinSetup();
  vw_digitalWrite_ptt(vw_ptt_inverted);
  vw_timerSetup(speed);
}


// Start the transmitter, call when the tx buffer is ready to go and vw_tx_len is
// set to the total number of symbols to send
void vw_tx_start() {
  vw_tx_index = 0;
  vw_tx_bit = 0;
  vw_tx_sample = 0;

  // Enable the transmitter hardware
  vw_digitalWrite_ptt(true ^ vw_ptt_inverted);

  // Next tick interrupt will send the first bit
  vw_tx_enabled = true;
}

// Stop the transmitter, call when all bits are sent
void vw_tx_stop() {
  // Disable the transmitter hardware
  vw_digitalWrite_ptt(false ^ vw_ptt_inverted);
  vw_digitalWrite_tx(false);

  // No more ticks for the transmitter
  vw_tx_enabled = false;
}

// Return true if the transmitter is active
uint8_t vw_tx_active() {
  return vw_tx_enabled;
}

// Wait for the transmitter to become available
// Busy-wait loop until the ISR says the message has been sent
void vw_wait_tx() {
  while (vw_tx_enabled);
}



// Wait until transmitter is available and encode and queue the message
// into vw_tx_buf
// The message is raw bytes, with no packet structure imposed
// It is transmitted preceded a byte count and followed by 2 FCS bytes
uint8_t vw_send(uint8_t *buf, uint8_t len) {
  uint8_t i;
  uint8_t index = 0;
  uint16_t crc = 0xffff;
  uint8_t *p = vw_tx_buf + VW_HEADER_LEN; // start of the message area
  uint8_t count = len + 3; // Added byte count and FCS to get total number of bytes

  if (len > VW_MAX_PAYLOAD)
    return false;

  // Wait for transmitter to become available
  vw_wait_tx();

  // Encode the message length
  crc = _crc_ccitt_update(crc, count);
  p[index++] = symbols[count >> 4];
  p[index++] = symbols[count & 0xf];

  // Encode the message into 6 bit symbols. Each byte is converted into
  // 2 6-bit symbols, high nybble first, low nybble second
  for (i = 0; i < len; i++) {
    crc = _crc_ccitt_update(crc, buf[i]);
    p[index++] = symbols[buf[i] >> 4];
    p[index++] = symbols[buf[i] & 0xf];
  }

  // Append the fcs, 16 bits before encoding (4 6-bit symbols after encoding)
  // Caution: VW expects the _ones_complement_ of the CCITT CRC-16 as the FCS
  // VW sends FCS as low byte then hi byte
  crc = ~crc;
  p[index++] = symbols[(crc >> 4) & 0xf];
  p[index++] = symbols[crc & 0xf];
  p[index++] = symbols[(crc >> 12) & 0xf];
  p[index++] = symbols[(crc >> 8) & 0xf];

  // Total number of 6-bit symbols to send
  vw_tx_len = index + VW_HEADER_LEN;

  // Start the low level interrupt handler sending symbols
  vw_tx_start();

  return true;
}



// This is the interrupt service routine called when timer1 overflows
// Its job is to output the next bit from the transmitter (every 8 calls)
// and to call the PLL code if the receiver is enabled
//ISR(SIG_OUTPUT_COMPARE1A)

ISR(VW_TIMER_VECTOR)
{
  if (vw_tx_enabled && vw_tx_sample++ == 0) {
    // Send next bit
    // Symbols are sent LSB first
    // Finished sending the whole message? (after waiting one bit period
    // since the last bit)
    if (vw_tx_index >= vw_tx_len) {
      vw_tx_stop();
      vw_tx_msg_count++;
    }
    else {
      vw_digitalWrite_tx(vw_tx_buf[vw_tx_index] & (1 << vw_tx_bit++));
      if (vw_tx_bit >= 6) {
        vw_tx_bit = 0;
        vw_tx_index++;
      }
    }
  }
  if (vw_tx_sample > 7)
    vw_tx_sample = 0;

}
