/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */
#include "RF24.h"



#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "rom/crc.h"




#define PIN_NUM_CE   17 // ---> nRF(Pin3)

#define PIN_NUM_CSn  5  // ---> nRF(Pin4)
#define PIN_NUM_SCK  18 // ---> nRF(Pin5)
#define PIN_NUM_MOSI 23 // ---> nRF(Pin6)
#define PIN_NUM_MISO 19 // ---> nRF(Pin7)






static bool wide_band; /* 2Mbs data rate in use? */
static bool p_variant; /* False for RF24L01 and true for RF24L01P */
static uint8_t payload_size; /**< Fixed size of payloads */
static bool ack_payload_available; /**< Whether there is an ack payload waiting */
static bool dynamic_payloads_enabled; /**< Whether dynamic payloads are enabled. */ 
static uint8_t ack_payload_length; /**< Dynamic size of pending ack payload. */
static uint64_t pipe0_reading_address; /**< Last address set on pipe 0 for reading. */


esp_err_t ret;
spi_device_handle_t spi;
spi_transaction_t t;


/****************************************************************************/

void RF24_ce(char level)
{
  gpio_set_level(PIN_NUM_CE, level);  
}

/****************************************************************************/

uint8_t RF24_read_register_multi_len(uint8_t reg, uint8_t* buf, uint8_t len)
{
  // esp_err_t ret;
  // spi_device_handle_t spi;
  // spi_transaction_t t;

  memset(&t, 0, sizeof(t));       //Zero out the transaction

  t.length=8; 

  uint8_t val = R_REGISTER | ( REGISTER_MASK & reg );

  t.tx_buffer= &val; 

  val = 0xff; 

  ret=spi_device_transmit(spi, &t);  //Transmit Address!
  assert(ret==ESP_OK);            //Should have had no issues.

  while ( len-- )
  {
    t.tx_buffer=&(val) ;
    ret=spi_device_transmit(spi, &t);  //Transmit data!
    if(ret) *buf++ = *(uint8_t*)t.rx_buffer;
  }

  return ret;

}

/****************************************************************************/

uint8_t RF24_read_register_single_len(uint8_t reg)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;
  uint8_t val = R_REGISTER | ( REGISTER_MASK & reg );
  t.tx_buffer= &val ;  
  t.flags = SPI_TRANS_USE_RXDATA;



  ret=spi_device_transmit(spi, &t);  //Transmit!
  
  val = 0xff;
  t.tx_buffer=&val;
  spi_device_transmit(spi, &t);  //Transmit!

  assert(ret==ESP_OK);            //Should have had no issues.
  

  return *(uint8_t*)t.rx_data;  
}
/****************************************************************************/

uint8_t RF24_write_register_multi_len(uint8_t reg, const uint8_t* buf, uint8_t len)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = W_REGISTER | ( REGISTER_MASK & reg );

  t.tx_buffer= &val ;               
  ret=spi_device_transmit(spi, &t);  //Transmit!

  while ( len-- )
  {
    t.tx_buffer=buf++ ;
    spi_device_transmit(spi, &t);  //Transmit!
  }

  assert(ret==ESP_OK);            //Should have had no issues.  

  return ESP_OK;
}

/****************************************************************************/

uint8_t RF24_write_register_single_len(uint8_t reg, uint8_t value)
{
  printf("RF24_write_register_single_len(%02x,%02x)\r\n", reg,value);

  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = W_REGISTER | ( REGISTER_MASK & reg );

  t.tx_buffer= &val ;
  ret=spi_device_transmit(spi, &t);  //Transmit!

  t.tx_buffer=&value ;
  spi_device_transmit(spi, &t);  //Transmit!

  assert(ret==ESP_OK);            //Should have had no issues.
  

  return ESP_OK;

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

uint8_t RF24_write_payload(const void* buf, uint8_t len)
{
  
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  const uint8_t* current = (const uint8_t *)(buf);

  uint8_t data_len = MIN(len,payload_size);
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;
  

  uint8_t val = W_TX_PAYLOAD;


  t.tx_buffer= &val ;               //Data
  ret=spi_device_transmit(spi, &t);  //Transmit!


  while ( data_len-- )
  {
    t.tx_buffer=current++;
    spi_device_transmit(spi, &t);  //Transmit!
  } 

  val = 0;

  while ( blank_len-- )
  {
    t.tx_buffer=&val;
    spi_device_transmit(spi, &t);  //Transmit!
  }

  assert(ret==ESP_OK);            //Should have had no issues.  

  return ret;

}

/****************************************************************************/

uint8_t RF24_read_payload(void* buf, uint8_t len)
{
  
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t* current = (uint8_t *)(buf);

  uint8_t data_len = MIN(len,payload_size);
  uint8_t blank_len = dynamic_payloads_enabled ? 0 : payload_size - data_len;
  

  uint8_t val = R_RX_PAYLOAD;

  t.tx_buffer= &val ;               //Data
  ret=spi_device_transmit(spi, &t);  //Transmit!

  val = 0xff;

  while ( data_len-- )
  {
    t.tx_buffer=&val;
    spi_device_transmit(spi, &t);  //Transmit!
    *(uint8_t*)t.rx_data=current++;
  } 

  while ( blank_len-- )
  {
    t.tx_buffer=&val;
    spi_device_transmit(spi, &t);  //Transmit!
  }

  assert(ret==ESP_OK);            //Should have had no issues.  

  return ret;
}

/****************************************************************************/

uint8_t RF24_flush_rx(void)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = FLUSH_RX;

  t.tx_buffer= &val ;     
  ret=spi_device_transmit(spi, &t);  //Transmit!

  return ret;
}

/****************************************************************************/

uint8_t RF24_flush_tx(void)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = FLUSH_TX;


  t.tx_buffer= &val ;     
  ret=spi_device_transmit(spi, &t);  //Transmit!

  return ret;
}

/****************************************************************************/

uint8_t RF24_get_status(void)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = NOP;

  t.tx_buffer= &val ;     
  ret=spi_device_transmit(spi, &t);  //Transmit!

  return ret;
}

/****************************************************************************/

void RF24_print_status(uint8_t status)
{
  printf_P(PSTR("RF24_STATUS\t\t = 0x%02x RX_DR=%x TX_DS=%x MAX_RT=%x RX_P_NO=%x TX_FULL=%x\r\n"),
           status,
           (status & _BV(RX_DR))?1:0,
           (status & _BV(TX_DS))?1:0,
           (status & _BV(MAX_RT))?1:0,
           ((status >> RX_P_NO) & 0x7),//B111
           (status & _BV(TX_FULL))?1:0
          );
}

/****************************************************************************/

void RF24_print_observe_tx(uint8_t value)
{
  printf_P(PSTR("OBSERVE_TX=%02x: POLS_CNT=%x ARC_CNT=%x\r\n"),
           value,
           (value >> PLOS_CNT) & 0xF,//B1111
           (value >> ARC_CNT) & 0xF //B1111
          );
}

/****************************************************************************/

void RF24_print_byte_register(const char* name, uint8_t reg, uint8_t qty)
{
  char extra_tab = strlen_P(name) < 8 ? '\t' : 0;
  printf_P(PSTR(PRIPSTR"\t%c ="),name,extra_tab);
  while (qty--)
    printf_P(PSTR(" 0x%02x"),RF24_read_register_single_len(reg++));
  printf_P(PSTR("\r\n"));
}

/****************************************************************************/

void RF24_print_address_register(const char* name, uint8_t reg, uint8_t qty)
{
  char extra_tab = strlen_P(name) < 8 ? '\t' : 0;
  printf_P(PSTR(PRIPSTR"\t%c ="),name,extra_tab);

  while (qty--)
  {
    uint8_t buffer[5];
    RF24_read_register_multi_len(reg++,buffer,sizeof buffer);

    printf_P(PSTR(" 0x"));
    uint8_t* bufptr = buffer + sizeof buffer;
    while( --bufptr >= buffer )
      printf_P(PSTR("%02x"),*bufptr);
  }

  printf_P(PSTR("\r\n"));
}

/****************************************************************************/

void RF24_setChannel(uint8_t channel)
{
  // TODO: This method could take advantage of the 'wide_band' calculation
  // done in RF24_setChannel() to require certain channel spacing.

  const uint8_t max_channel = 127;
  RF24_write_register_single_len(RF_CH, MIN(channel,max_channel));
}

/****************************************************************************/

void RF24_setPayloadSize(uint8_t size)
{
  const uint8_t max_payload_size = 32;
  payload_size = MIN(size,max_payload_size);
}

/****************************************************************************/

uint8_t RF24_getPayloadSize(void)
{
  return payload_size;
}

/****************************************************************************/

static const char rf24_datarate_e_str_0[] PROGMEM = "1MBPS";
static const char rf24_datarate_e_str_1[] PROGMEM = "2MBPS";
static const char rf24_datarate_e_str_2[] PROGMEM = "250KBPS";
static const char * const rf24_datarate_e_str_P[] PROGMEM = {
  rf24_datarate_e_str_0,
  rf24_datarate_e_str_1,
  rf24_datarate_e_str_2,
};
static const char rf24_model_e_str_0[] PROGMEM = "nRF24L01";
static const char rf24_model_e_str_1[] PROGMEM = "nRF24L01+";
static const char * const rf24_model_e_str_P[] PROGMEM = {
  rf24_model_e_str_0,
  rf24_model_e_str_1,
};
static const char rf24_crclength_e_str_0[] PROGMEM = "Disabled";
static const char rf24_crclength_e_str_1[] PROGMEM = "8 bits";
static const char rf24_crclength_e_str_2[] PROGMEM = "16 bits" ;
static const char * const rf24_crclength_e_str_P[] PROGMEM = {
  rf24_crclength_e_str_0,
  rf24_crclength_e_str_1,
  rf24_crclength_e_str_2,
};
static const char rf24_pa_dbm_e_str_0[] PROGMEM = "PA_MIN";
static const char rf24_pa_dbm_e_str_1[] PROGMEM = "PA_LOW";
static const char rf24_pa_dbm_e_str_2[] PROGMEM = "LA_MED";
static const char rf24_pa_dbm_e_str_3[] PROGMEM = "PA_HIGH";
static const char * const rf24_pa_dbm_e_str_P[] PROGMEM = { 
  rf24_pa_dbm_e_str_0,
  rf24_pa_dbm_e_str_1,
  rf24_pa_dbm_e_str_2,
  rf24_pa_dbm_e_str_3,
};

void RF24_printDetails(void)
{
  RF24_print_status(RF24_get_status());

  RF24_print_address_register(PSTR("RX_ADDR_P0-1"),RX_ADDR_P0,2);
  RF24_print_byte_register(PSTR("RX_ADDR_P2-5"),RX_ADDR_P2,4);
  RF24_print_address_register(PSTR("TX_ADDR"),TX_ADDR, 1);

  RF24_print_byte_register(PSTR("RX_PW_P0-6"),RX_PW_P0,6);
  RF24_print_byte_register(PSTR("EN_AA"),EN_AA, 1);
  RF24_print_byte_register(PSTR("EN_RXADDR"),EN_RXADDR, 1);
  RF24_print_byte_register(PSTR("RF_CH"),RF_CH, 1);
  RF24_print_byte_register(PSTR("RF_SETUP"),RF_SETUP, 1);
  RF24_print_byte_register(PSTR("CONFIG"),CONFIG, 1);
  RF24_print_byte_register(PSTR("DYNPD/FEATURE"),DYNPD,2);

  printf(PSTR("Data Rate\t = %s\r\n"),pgm_read_word(&rf24_datarate_e_str_P[RF24_getDataRate()]));
  printf(PSTR("Model\t\t = %s\r\n"),pgm_read_word(&rf24_model_e_str_P[RF24_isPVariant()]));
  printf(PSTR("CRC Length\t = %s\r\n"),pgm_read_word(&rf24_crclength_e_str_P[RF24_getCRCLength()]));
  printf(PSTR("PA Power\t = %s\r\n"),pgm_read_word(&rf24_pa_dbm_e_str_P[RF24_getPALevel()]));
}

/****************************************************************************/

void RF24_begin(void)
{
  // Initialize CE of Radio
  gpio_set_direction(PIN_NUM_CE, GPIO_MODE_OUTPUT);

  init_SPI_for_RF24();

  // Must allow the radio time to settle else configuration bits will not necessarily stick.
  // This is actually only required following power up but some settling time also appears to
  // be required after resets too. For full coverage, we'll always assume the worst.
  // Enabling 16b CRC is by far the most obvious case if the wrong timing is used - or skipped.
  // Technically we require 4.5ms + 14us as a worst case. We'll just call it 5ms for good measure.
  // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.

  vTaskDelay(5 / portTICK_PERIOD_MS); //5 ms delay

  // Set 1500uS (minimum for 32B payload in ESB@250KBPS) timeouts, to make testing a little easier
  // WARNING: If this is ever lowered, either 250KBS mode with AA is broken or maximum packet
  // sizes must never be used. See documentation for a more complete explanation.
  // RF24_write_register_single_len(SETUP_RETR,(0x4 << ARD) | (0xF << ARC)); //B0100 //B1111





  // Restore our default PA level
  RF24_setPALevel( RF24_PA_MAX ) ;

  // Determine if this is a p or non-p RF24 module and then
  // reset our data rate back to default value. This works
  // because a non-P variant won't allow the data rate to
  // be set to 250Kbps.
  if( RF24_setPALevel( RF24_250KBPS ) )
  {
    p_variant = true ;
  }
  
  // Then set the data rate to the slowest (and most reliable) speed supported by all
  // hardware.
  RF24_setPALevel( RF24_1MBPS ) ;

  // Initialize CRC and request 2-byte (16bit) CRC
  RF24_setCRCLength( RF24_CRC_16 ) ;
  
  // Disable dynamic payloads, to match dynamic_payloads_enabled setting
  RF24_write_register_single_len(DYNPD,0);

  // Reset current status
  // Notice reset and flush is the last thing we do
  RF24_write_register_single_len(RF24_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Set up default configuration.  Callers can always change it later.
  // This channel should be universally safe and not bleed over into adjacent
  // spectrum.
  RF24_setChannel(76);

  // Flush buffers
  RF24_flush_rx();
  RF24_flush_tx();
}

/****************************************************************************/

void RF24_startListening(void)
{
  RF24_write_register_single_len(CONFIG, RF24_read_register_single_len(CONFIG) | _BV(PWR_UP) | _BV(PRIM_RX));
  RF24_write_register_single_len(RF24_STATUS, _BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

  // Restore the pipe0 adddress, if exists
  if (pipe0_reading_address)
    RF24_write_register_multi_len(RX_ADDR_P0, (const uint8_t *)(&pipe0_reading_address), 5);

  // Flush buffers
  RF24_flush_rx();
  RF24_flush_tx();

  // Go!
  RF24_ce(1);

  // wait for the radio to come up (130us actually only needed)
    vTaskDelay(1 / portTICK_PERIOD_MS); //5 ms delay

}

/****************************************************************************/

void RF24_stopListening(void)
{
  RF24_ce(1);
  RF24_flush_tx();
  RF24_flush_rx();
}

/****************************************************************************/

void RF24_powerDown(void)
{
  RF24_write_register_single_len(CONFIG,RF24_read_register_single_len(CONFIG) & ~_BV(PWR_UP));
}

/****************************************************************************/

void RF24_powerUp(void)
{
  RF24_write_register_single_len(CONFIG,RF24_read_register_single_len(CONFIG) | _BV(PWR_UP));
}

// /******************************************************************/

// bool RF24_write( const void* buf, uint8_t len )
// {
//   bool result = false;

//   // Begin the write
//   RF24_startWrite(buf,len);

//   // ------------
//   // At this point we could return from a non-blocking write, and then call
//   // the rest after an interrupt

//   // Instead, we are going to block here until we get TX_DS (transmission completed and ack'd)
//   // or MAX_RT (maximum retries, transmission failed).  Also, we'll timeout in case the radio
//   // is flaky and we get neither.

//   // IN the end, the send should be blocking.  It comes back in 60ms worst case, or much faster
//   // if I tighted up the retry logic.  (Default settings will be 1500us.
//   // Monitor the send
//   uint8_t observe_tx;
//   uint8_t status;
//   uint32_t sent_at = millis();
//   const uint32_t timeout = 500; //ms to wait for timeout
//   do
//   {
//     status = RF24_read_register_multi_len(OBSERVE_TX,&observe_tx,1);
//     IF_SERIAL_DEBUG(Serial.print(observe_tx,HEX));
//   }
//   while( ! ( status & ( _BV(TX_DS) | _BV(MAX_RT) ) ) && ( millis() - sent_at < timeout ) );

//   // The part above is what you could recreate with your own interrupt handler,
//   // and then call this when you got an interrupt
//   // ------------

//   // Call this when you get an interrupt
//   // The status tells us three things
//   // * The send was successful (TX_DS)
//   // * The send failed, too many retries (MAX_RT)
//   // * There is an ack packet waiting (RX_DR)
//   bool tx_ok, tx_fail;
//   RF24_whatHappened(tx_ok,tx_fail,ack_payload_available);
  
//   //printf("%u%u%u\r\n",tx_ok,tx_fail,ack_payload_available);

//   result = tx_ok;
//   IF_SERIAL_DEBUG(Serial.print(result?"...OK.":"...Failed"));

//   // Handle the ack packet
//   if ( ack_payload_available )
//   {
//     ack_payload_length = RF24_getDynamicPayloadSize();
//     IF_SERIAL_DEBUG(Serial.print("[AckPacket]/"));
//     IF_SERIAL_DEBUG(Serial.println(ack_payload_length,DEC));
//   }

//   // Yay, we are done.

//   // Power down
//   RF24_powerDown();

//   // Flush buffers (Is this a relic of past experimentation, and not needed anymore??)
//   RF24_flush_tx();

//   return result;
// }
/****************************************************************************/

void RF24_startWrite( const void* buf, uint8_t len )
{
  // Transmitter power-up
  RF24_write_register_single_len(CONFIG, ( RF24_read_register_single_len(CONFIG) | _BV(PWR_UP) ) & ~_BV(PRIM_RX) );
  vTaskDelay(1 / portTICK_PERIOD_MS); //5 ms delay

  // Send the payload
  RF24_write_payload( buf, len );

  // Allons!
  RF24_ce(1);
  vTaskDelay(1 / portTICK_PERIOD_MS); //5 ms delay
  RF24_ce(0);
}

/****************************************************************************/

uint8_t RF24_getDynamicPayloadSize(void)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;


  uint8_t val = R_RX_PL_WID;

  t.tx_buffer= &val ;  
  t.flags = SPI_TRANS_USE_RXDATA;


  ret=spi_device_transmit(spi, &t);  //Transmit!

  val = 0xff;

  t.tx_buffer=&val;
  spi_device_transmit(spi, &t);  //Transmit!

  assert(ret==ESP_OK);            //Should have had no issues.
  

  return *(uint8_t*)t.rx_data;  
}

/****************************************************************************/

bool RF24_available(void)
{
  return available(NULL);
}

/****************************************************************************/

bool available(uint8_t* pipe_num)
{
  uint8_t status = RF24_get_status();

  // Too noisy, enable if you really want lots o data!!
  //IF_SERIAL_DEBUG(RF24_print_status(status));

  bool result = ( status & _BV(RX_DR) );

  if (result)
  {
    // If the caller wants the pipe number, include that
    if ( pipe_num )
      *pipe_num = ( status >> RX_P_NO ) & 0x7;// B111

    // Clear the status bit

    // ??? Should this REALLY be cleared now?  Or wait until we
    // actually READ the payload?

    RF24_write_register_single_len(RF24_STATUS,_BV(RX_DR) );

    // Handle ack payload receipt
    if ( status & _BV(TX_DS) )
    {
      RF24_write_register_single_len(RF24_STATUS,_BV(TX_DS));
    }
  }

  return result;
}

/****************************************************************************/

bool RF24_read( void* buf, uint8_t len )
{
  // Fetch the payload
  RF24_read_payload( buf, len );

  // was this the last of the data available?
  return RF24_read_register_single_len(FIFO_STATUS) & _BV(RX_EMPTY);
}

/****************************************************************************/

// void RF24_whatHappened(bool tx_ok, bool tx_fail,bool rx_ready)
// {
//   // Read the status & reset the status in one easy call
//   // Or is that such a good idea?
//   uint8_t status = RF24_write_register_single_len(RF24_STATUS,_BV(RX_DR) | _BV(TX_DS) | _BV(MAX_RT) );

//   // Report to the user what happened
//   tx_ok = status & _BV(TX_DS);
//   tx_fail = status & _BV(MAX_RT);
//   rx_ready = status & _BV(RX_DR);
// }

/****************************************************************************/

void RF24_openWritingPipe(uint64_t value)
{
  // Note that AVR 8-bit uC's store this LSB first, and the NRF24L01(+)
  // expects it LSB first too, so we're good.

  RF24_write_register_multi_len(RX_ADDR_P0, (uint8_t *)(&value), 5);
  RF24_write_register_multi_len(TX_ADDR, (uint8_t *)(&value), 5);

  const uint8_t max_payload_size = 32;
  RF24_write_register_single_len(RX_PW_P0,MIN(payload_size,max_payload_size));
}

/****************************************************************************/

static const uint8_t child_pipe[] PROGMEM =
{
  RX_ADDR_P0, RX_ADDR_P1, RX_ADDR_P2, RX_ADDR_P3, RX_ADDR_P4, RX_ADDR_P5
};

static const uint8_t child_payload_size[] PROGMEM =
{
  RX_PW_P0, RX_PW_P1, RX_PW_P2, RX_PW_P3, RX_PW_P4, RX_PW_P5
};

static const uint8_t child_pipe_enable[] PROGMEM =
{
  ERX_P0, ERX_P1, ERX_P2, ERX_P3, ERX_P4, ERX_P5
};

// void RF24_openReadingPipe(uint8_t child, uint64_t address)
// {
//   // If this is pipe 0, cache the address.  This is needed because
//   // RF24_openWritingPipe() will overwrite the pipe 0 address, so
//   // startListening() will have to restore it.
//   if (child == 0)
//     pipe0_reading_address = address;

//   if (child <= 6)
//   {
//     // For pipes 2-5, only write the LSB
//     if ( child < 2 )
//       RF24_write_register_multi_len(pgm_read_byte(&child_pipe[child]), (const uint8_t *)(&address), 5);
//     else
//       RF24_write_register_multi_len(pgm_read_byte(&child_pipe[child]), (const uint8_t *)(&address), 1);

//     RF24_write_register_single_len(pgm_read_byte(&child_payload_size[child]),payload_size);

//     // Note it would be more efficient to set all of the bits for all open
//     // pipes at once.  However, I thought it would make the calling code
//     // more simple to do it this way.
//     RF24_write_register_single_len(EN_RXADDR,RF24_read_register_single_len(EN_RXADDR) | _BV(pgm_read_byte(&child_pipe_enable[child])));
//   }
// }

/****************************************************************************/

void RF24_toggle_features(void)
{
  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = ACTIVATE;


  t.tx_buffer= &val ;               //Data
  ret=spi_device_transmit(spi, &t);  //Transmit!

  val = 0x73;

  t.tx_buffer=&val ;
  spi_device_transmit(spi, &t);  //Transmit!

  assert(ret==ESP_OK);            //Should have had no issues.  
}

/****************************************************************************/

void RF24_enableDynamicPayloads(void)
{
  // Enable dynamic payload throughout the system
  RF24_write_register_single_len(FEATURE,RF24_read_register_single_len(FEATURE) | _BV(EN_DPL) );

  // If it didn't work, the features are not enabled
  if ( ! RF24_read_register_single_len(FEATURE) )
  {
    // So enable them and try again
    RF24_toggle_features();
    RF24_write_register_single_len(FEATURE,RF24_read_register_single_len(FEATURE) | _BV(EN_DPL) );
  }

  IF_SERIAL_DEBUG(printf("FEATURE=%i\r\n",RF24_read_register_single_len(FEATURE)));

  // Enable dynamic payload on all pipes
  //
  // Not sure the use case of only having dynamic payload on certain
  // pipes, so the library does not support it.
  RF24_write_register_single_len(DYNPD,RF24_read_register_single_len(DYNPD) | _BV(DPL_P5) | _BV(DPL_P4) | _BV(DPL_P3) | _BV(DPL_P2) | _BV(DPL_P1) | _BV(DPL_P0));

  dynamic_payloads_enabled = true;
}

/****************************************************************************/

void RF24_enableAckPayload(void)
{
  //
  // enable ack payload and dynamic payload features
  //

  RF24_write_register_single_len(FEATURE,RF24_read_register_single_len(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );

  // If it didn't work, the features are not enabled
  if ( ! RF24_read_register_single_len(FEATURE) )
  {
    // So enable them and try again
    RF24_toggle_features();
    RF24_write_register_single_len(FEATURE,RF24_read_register_single_len(FEATURE) | _BV(EN_ACK_PAY) | _BV(EN_DPL) );
  }

  IF_SERIAL_DEBUG(printf("FEATURE=%i\r\n",RF24_read_register_single_len(FEATURE)));

  //
  // Enable dynamic payload on pipes 0 & 1
  //

  RF24_write_register_single_len(DYNPD, RF24_read_register_single_len(DYNPD) | _BV(DPL_P1) | _BV(DPL_P0));
}

/****************************************************************************/

void RF24_writeAckPayload(uint8_t pipe, const void* buf, uint8_t len)
{
  const uint8_t* current = (uint8_t *)(buf);

  memset(&t, 0, sizeof(t));       //Zero out the transaction
  t.length=8;

  uint8_t val = W_ACK_PAYLOAD | ( pipe & 0x7 );

  t.tx_buffer= &val ;               
  ret=spi_device_transmit(spi, &t);  //Transmit!

  const uint8_t max_payload_size = 32;
  uint8_t data_len = MIN(len,max_payload_size);

  while ( data_len-- )
  {
    t.tx_buffer=current++ ;
    spi_device_transmit(spi, &t);  //Transmit!
  }

  assert(ret==ESP_OK);            //Should have had no issues.  

  return ESP_OK;  
}

/****************************************************************************/

bool RF24_isAckPayloadAvailable(void)
{
  bool result = ack_payload_available;
  ack_payload_available = false;
  return result;
}

/****************************************************************************/

bool RF24_isPVariant(void)
{
  return p_variant ;
}

/****************************************************************************/

void RF24_setAutoAck(bool enable)
{
  if ( enable )
    RF24_write_register_single_len(EN_AA, 0x3F); //B111111
  else
    RF24_write_register_single_len(EN_AA, 0);
}

/****************************************************************************/

void RF24_setAutoAck_pipe( uint8_t pipe, bool enable )
{
  if ( pipe <= 6 )
  {
    uint8_t en_aa = RF24_read_register_single_len( EN_AA ) ;
    if( enable )
    {
      en_aa |= _BV(pipe) ;
    }
    else
    {
      en_aa &= ~_BV(pipe) ;
    }
    RF24_write_register_single_len( EN_AA, en_aa ) ;
  }
}

/****************************************************************************/

bool RF24_testCarrier(void)
{
  return ( RF24_read_register_single_len(CD) & 1 );
}

/****************************************************************************/

bool RF24_testRPD(void)
{
  return ( RF24_read_register_single_len(RPD) & 1 ) ;
}

/****************************************************************************/

bool RF24_setPALevel(rf24_pa_dbm_e level)
{
  uint8_t setup = RF24_read_register_single_len(RF_SETUP) ;
  setup &= ~(_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;

  // switch uses RAM (evil!)
  if ( level == RF24_PA_MAX )
  {
    setup |= (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;
  }
  else if ( level == RF24_PA_HIGH )
  {
    setup |= _BV(RF_PWR_HIGH) ;
  }
  else if ( level == RF24_PA_LOW )
  {
    setup |= _BV(RF_PWR_LOW);
  }
  else if ( level == RF24_PA_MIN )
  {
    // nothing
  }
  else if ( level == RF24_PA_ERROR )
  {
    // On error, go to maximum PA
    setup |= (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;
  }

  return RF24_write_register_single_len( RF_SETUP, setup ) ;


}

/****************************************************************************/

rf24_pa_dbm_e RF24_getPALevel(void)
{
  rf24_pa_dbm_e result = RF24_PA_ERROR ;
  uint8_t power = RF24_read_register_single_len(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) ;

  // switch uses RAM (evil!)
  if ( power == (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH)) )
  {
    result = RF24_PA_MAX ;
  }
  else if ( power == _BV(RF_PWR_HIGH) )
  {
    result = RF24_PA_HIGH ;
  }
  else if ( power == _BV(RF_PWR_LOW) )
  {
    result = RF24_PA_LOW ;
  }
  else
  {
    result = RF24_PA_MIN ;
  }

  return result ;
}

/****************************************************************************/

bool RF24_setDataRate(rf24_datarate_e speed)
{
  bool result = false;
  uint8_t setup = RF24_read_register_single_len(RF_SETUP) ;

  // HIGH and LOW '00' is 1Mbs - our default
  wide_band = false ;
  setup &= ~(_BV(RF_DR_LOW) | _BV(RF_DR_HIGH)) ;
  if( speed == RF24_250KBPS )
  {
    // Must set the RF_DR_LOW to 1; RF_DR_HIGH (used to be RF_DR) is already 0
    // Making it '10'.
    wide_band = false ;
    setup |= _BV( RF_DR_LOW ) ;
  }
  else
  {
    // Set 2Mbs, RF_DR (RF_DR_HIGH) is set 1
    // Making it '01'
    if ( speed == RF24_2MBPS )
    {
      wide_band = true ;
      setup |= _BV(RF_DR_HIGH);
    }
    else
    {
      // 1Mbs
      wide_band = false ;
    }
  }
  RF24_write_register_single_len(RF_SETUP,setup);

  // Verify our result
  if ( RF24_read_register_single_len(RF_SETUP) == setup )
  {
    result = true;
  }
  else
  {
    wide_band = false;
  }

  return result;
}

/****************************************************************************/

rf24_datarate_e RF24_getDataRate( void )
{
  rf24_datarate_e result ;
  uint8_t dr = RF24_read_register_single_len(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));
  
  // switch uses RAM (evil!)
  // Order matters in our case below
  if ( dr == _BV(RF_DR_LOW) )
  {
    // '10' = 250KBPS
    result = RF24_250KBPS ;
  }
  else if ( dr == _BV(RF_DR_HIGH) )
  {
    // '01' = 2MBPS
    result = RF24_2MBPS ;
  }
  else
  {
    // '00' = 1MBPS
    result = RF24_1MBPS ;
  }
  return result ;
}

/****************************************************************************/

void RF24_setCRCLength(rf24_crclength_e length)
{
  uint8_t config = RF24_read_register_single_len(CONFIG) & ~( _BV(CRCO) | _BV(EN_CRC)) ;
  
  // switch uses RAM (evil!)
  if ( length == RF24_CRC_DISABLED )
  {
    // Do nothing, we turned it off above. 
  }
  else if ( length == RF24_CRC_8 )
  {
    config |= _BV(EN_CRC);
  }
  else
  {
    config |= _BV(EN_CRC);
    config |= _BV( CRCO );
  }
  RF24_write_register_single_len( CONFIG, config ) ;
}

/****************************************************************************/

rf24_crclength_e RF24_getCRCLength(void)
{
  rf24_crclength_e result = RF24_CRC_DISABLED;
  uint8_t config = RF24_read_register_single_len(CONFIG) & ( _BV(CRCO) | _BV(EN_CRC)) ;

  if ( config & _BV(EN_CRC ) )
  {
    if ( config & _BV(CRCO) )
      result = RF24_CRC_16;
    else
      result = RF24_CRC_8;
  }

  return result;
}

/****************************************************************************/

void RF24_disableCRC( void )
{
  uint8_t disable = RF24_read_register_single_len(CONFIG) & ~_BV(EN_CRC) ;
  RF24_write_register_single_len( CONFIG, disable ) ;
}

/****************************************************************************/
void RF24_setRetries(uint8_t delay, uint8_t count)
{
 RF24_write_register_single_len(SETUP_RETR,(delay&0xf)<<ARD | (count&0xf)<<ARC);
}

/****************************************************************************/

void init_SPI_for_RF24(void){



  memset(&t, 0, sizeof(t));       //Zero out the transaction

  t.length=8; 
  
  spi_bus_config_t buscfg = {

    .miso_io_num=PIN_NUM_MISO,
    .mosi_io_num=PIN_NUM_MOSI,
    .sclk_io_num=PIN_NUM_SCK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1,
    .max_transfer_sz=0,
  };
  
  spi_device_interface_config_t devcfg={
    .clock_speed_hz=4000000,           //Clock out at 4 MHz

    .mode=0,                                //SPI mode 0
    .spics_io_num=PIN_NUM_CSn,               //CS pin
    .queue_size=1,                          //*Rizwan: Need to review
        // .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
  };
  //Initialize the SPI bus
  ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);
  ESP_ERROR_CHECK(ret);

  //Attach the RF24-radio to the SPI bus
  ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);

  printf("SPI initialized\n");

}
/****************************************************************************/
