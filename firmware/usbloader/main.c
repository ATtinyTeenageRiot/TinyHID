/* Name: main.c
 * Project: hid-data, example how to use HID for data transfer
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-11
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 */

/*
This example should run on most AVRs with only little changes. No special
hardware resources except INT0 are used. You may have to change usbconfig.h for
different I/O pins for USB. Please note that USB D+ must be the INT0 pin, or
at least be connected to INT0 as well.
*/

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */
#include <avr/eeprom.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include <util/crc16.h>

#include "usbdrv.h"
#include "usbloader.h"

char lastTimer0Value;

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

PROGMEM const char usbHidReportDescriptor[22] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, LOADER_REPORT_SIZE,      //   REPORT_COUNT (LOADER_REPORT_SIZE)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};
/* Since we define only one feature report, we don't use report-IDs (which
 * would be the first byte of the report). The entire report consists of 128
 * opaque data bytes.
 */

#define DO_READ_FLASH 0
#define DO_WRITE_FLASH 0x10	
#define DO_ERASE_FLASH 0x20
#define DO_ERASE_EEPROM 0x40
#define DO_LEAVE_BOOTLOADER 0x80

/* The following variables store the status of the current data transfer */
static uchar offset;

static uint16_t currentAddress = 0;
static uchar cmd = 0;
static uchar delay = 0;
static uint16_t vectors[2];

#if ( BOOTLOADER_ADDRESS % SPM_PAGESIZE ) != 0
#error Bootloader address must be aligned by page size
#endif


#if CAN_READ_FLASH
static uchar exchangeReport[70];
#endif
#if CAN_COUNT_POLLS
uint16_t idlePolls = 0;
#endif
#if CAN_CHECK_DATA
static crc_t crc;
static crc_t sign;

static crc_t crc_update( crc_t crc, uint8_t *data, uint8_t len )
{
	while( len ) {
		crc = CRC_FUNCTION( crc, *data );
		data++;
		len--;
	}
	return crc;
}

#define __boot_page_fill_clear()							\
(__extension__({											\
    __asm__ __volatile__									\
    (														\
        "sts %0, %1\n\t"									\
        "spm\n\t"											\
        :													\
        : "i" (_SFR_MEM_ADDR(__SPM_REG)),					\
          "r" ((uint8_t)(__BOOT_PAGE_FILL | (1 << CTPB)))	\
    );														\
}))
#endif

#if CAN_ERASE_EEPROM
static inline void eraseEeprom()
{
	uint16_t j;

	// Ничего особенного, просто заполняем eeprom значениями 0xff
	eeprom_busy_wait();
			
	for( j = 0; j < E2END; j++ ) {
		eeprom_write_byte( (uchar*)j, 0xff );
	}
}
#endif

static void writeWord( uint16_t word )
{
uint16_t addr = currentAddress;
	// Записываем программные вектора во flash, чтобы уметь на них уходить
	// Мы их предоставим-таки программе, но иначе
	if ( addr == APP_RESET_ADDR ) {
		word = vectors[0] + APP_RESET_SHIFT;
	} else if ( addr == APP_PCINT_ADDR ) {
		word = vectors[1] + APP_PCINT_SHIFT;
	} else if( addr == RESET_ADDR ) {
		vectors[0] = word;
		word = LOADER_VECTOR;
	} else if( addr == PCINT_ADDR ) {
		vectors[1] = word;
		word = LOADER_VECTOR;
	}
		
	cli();
	boot_page_fill( currentAddress, word );
	sei();
	currentAddress += 2;
}

static void writePage()
{
	// Мы не должны писать в самого себя
	if( currentAddress > BOOTLOADER_ADDRESS ) return;
	
#ifdef LED_PIN
	PORTB |= _BV(LED_PIN);
#endif		

	boot_page_write( currentAddress - SPM_PAGESIZE );

#ifdef LED_PIN
	PORTB &= ~_BV(LED_PIN);
#endif		
}

static void writeInitialPage()
{
	while( currentAddress < SPM_PAGESIZE ) {
		writeWord( 0xffff );
	}
	writePage();
}

static void eraseFlash()
{
	uint16_t addr = BOOTLOADER_ADDRESS;
    while( addr ) {
        addr -= SPM_PAGESIZE;
        boot_page_erase( addr );
    }

    // Если нас запросили записать страницу, то незачем писать её дважды
    if( ( cmd & DO_WRITE_FLASH ) == 0 ) 
		writeInitialPage();
		
}

/* usbFunctionWrite() is called when the host sends a chunk of data to the
 * device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar usbFunctionWrite( uchar *data, uchar len )
{
	// offset - позиция в report-е.
	offset += len;
	// Если это первая порция
	if( offset == len ) {
		// Если у нас на очереди есть команда, которая ещё не выполнена
		// то мы вынуждены отказать
		if( cmd ) return 0xff;
		cmd = data[ REPORT_COMMAND ];
		
#if CAN_CHECK_DATA
		crc = CRC_INITIAL;
		sign = *(crc_t*)(data + REPORT_CRC);
		if( data[ REPORT_CMD_CHECK ] + cmd != 0xff ) {
			cmd = 0;
			return 0xff;
		}
#endif

		if( cmd != DO_WRITE_FLASH ) {
			currentAddress = 0;
		}

		data += REPORT_DATA;
		len -= REPORT_DATA;
	}
	
#if CAN_CHECK_DATA
	crc = crc_update( crc, data, len );
#endif
	
	if( cmd & DO_WRITE_FLASH ) {
		while( len ) {
			writeWord( *(int16_t*)data );
			data += 2;
			len -= 2;
		}
	}
	if( offset == LOADER_REPORT_SIZE ) {
#if CAN_CHECK_DATA
		if( crc != sign ) {
			cli();
			__boot_page_fill_clear();
			sei();
			return 0xff;
		}
#endif		
		delay = 10;
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup( uchar data[8] )
{
usbRequest_t *rq = (void *)data;
#if CAN_READ_FLASH
uchar i;
#if CAN_CHECK_DATA
crc_t crc = CRC_INITIAL;
#endif

	// wValue: ReportType (highbyte), ReportID (lowbyte)
    if( rq->bRequest == USBRQ_HID_GET_REPORT ) {
			
		// Читаем репорт
		exchangeReport[0] = 0;
			
		uchar * ptr = exchangeReport + REPORT_DATA;
		// Передаём сами данные
		for( i = 0; i < SPM_PAGESIZE; i++ ) {
			ptr[i] = pgm_read_byte( currentAddress++ );
		}
#if CAN_CHECK_DATA
		// And calculate crc
		crc = crc_update( crc, exchangeReport + REPORT_DATA, SPM_PAGESIZE );
		*(crc_t*)(exchangeReport + REPORT_CRC) = crc;
#endif
		usbMsgPtr = exchangeReport;
			
        return LOADER_REPORT_SIZE;
			
    } else 
#endif
	if( rq->bRequest == USBRQ_HID_SET_REPORT ) {
        // since we have only one report type, we can ignore the report-ID
		offset = 0;
		// use usbFunctionWrite() to receive data from host
        return USB_NO_MSG;
    }
    return 0;
}

static inline void tinyFlashInit() 
{
	// Вектора сброса и INT0 должны вести к бутлодеру. Иначе страница очищена
    if( pgm_read_word( RESET_ADDR ) != LOADER_VECTOR ) {
			
		writeInitialPage();
    }
}

// Переход к пользовательской программе
static void leaveBootloader() __attribute__((__noreturn__));
static inline void leaveBootloader() 
{
    cli();

	// Пользовательская фукнция очистки    
    bootLoaderExit();
	
	// Очищаем регистры
    USB_INTR_ENABLE = 0;
    USB_INTR_CFG = 0;
	// Переключаем PCINT на приложение
	TCNT1 = 0;
	TCNT0 = 0;

#ifdef LED_PIN
	DDRB = 0;
	PORTB = 0;
#endif
	TCCR0B = 0;

    // И переходим по reset-вектору приложения
    asm volatile ("rjmp __vectors - 4");
}

static inline void initForUsbConnectivity() 
{
    usbInit();
	// Переподключаемся
    usbDeviceDisconnect();
	_delay_ms( 500 );
    usbDeviceConnect();
	TCNT1 = 0xff;
    sei();
}

int main()
{
	wdt_disable();
    bootLoaderInit();
	tinyFlashInit();

	if( bootLoaderStartCondition() ) {
		bootLoaderInitiated();
		initForUsbConnectivity();
#ifdef LED_PIN
		DDRB |= _BV(LED_PIN);
#endif		
		TCCR0B = _BV(CS01) | _BV(CS00);
		do
		{
			usbPoll();
#		if CAN_COUNT_POLLS
			idlePolls++;
#		endif
			_delay_us( 100 );
			
			if( delay != 0 ) {
				if( --delay == 0 ) {

					// На время таинства записи отключаемся от внешнего.
					cli();
					if( cmd & DO_ERASE_FLASH ) {
						eraseFlash();
					} 
					if( cmd & DO_WRITE_FLASH ) {
						writePage();
					} 
#				if CAN_ERASE_EEPROM
					if( cmd & DO_ERASE_EEPROM ) {
						eraseEeprom();
					} 
#				endif
#				if CAN_LEAVE_LOADER
					if( cmd & DO_LEAVE_BOOTLOADER ) {
						leaveLoader();
						break;
					}
#				endif
					// Возвращаемя в мир
					cmd = 0;
					sei();
				}
			}
		} while( bootLoaderCondition() );
	}
	
	leaveBootloader();
}

/* ------------------------------------------------------------------------- */
