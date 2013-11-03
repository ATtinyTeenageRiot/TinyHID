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
static uchar currentAddress;
static uchar bytesRemaining;

static uint16_t workAddress = 0;
static uchar cmd;
static uchar delay = 0;
static uchar exchangeReport[70];
static uint16_t vectors[2];

#if CAN_COUNT_POLLS
uint16_t idlePolls = 0;
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

static void writePage()
{
	uchar i;
	uint16_t w, addr = workAddress;
	
	if( addr >= BOOTLOADER_ADDRESS ) return;

	// Дожидаемся окончания записи в EEPROM
#if CAN_ERASE_EEPROM
	eeprom_busy_wait();
#endif
	// И окончания записи в FLASH
			
	// И пишем помаленьку в буффер
	uchar * buf = exchangeReport + REPORT_DATA;
	
	for ( i = 0; i < SPM_PAGESIZE; i += 2 )
	{
		w = *buf++;
		w += ( *buf++ ) << 8;

		// Записываем программные вектора во flash, чтобы уметь на них уходить
		if ( addr == APP_RESET_ADDR ) {
			w = vectors[0] + APP_RESET_SHIFT;
		} else if ( addr == APP_PCINT_ADDR ) {
			w = vectors[1] + APP_PCINT_SHIFT;
		} else if( addr == RESET_ADDR || addr == PCINT_ADDR ) {
			// Мы их предоставим-таки программе, но иначе
			vectors[ i >> 2 ] = w;
			w = LOADER_VECTOR;
		}
		boot_page_fill( addr, w );
		addr += 2;
	}
	boot_page_write( workAddress );
	workAddress = addr;
}

static void writeInitialPage()
{
	uchar i;
	for( i = 0; i < LOADER_REPORT_SIZE; i++ ) {
		exchangeReport[i] = 0xff;
	}
		
	writePage();
}

static void eraseFlash()
{
    workAddress = BOOTLOADER_ADDRESS;
    while( workAddress ) {
        workAddress -= SPM_PAGESIZE;
        
        boot_page_erase( workAddress );
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
	uchar i, crc = 0;
	
    if( bytesRemaining == 0 ) {
        return 1;               /* end of transfer */
	}
    if( len > bytesRemaining ) {
        len = bytesRemaining;
	}
	for( i = 0; i < len; i++ ) {
		exchangeReport[ currentAddress + i ] = data[ i ];
	}
		
    currentAddress += len;
    bytesRemaining -= len;
	
	if( bytesRemaining == 0 ) {
		
		for( i = 0; i < REPORT_CRC; i++ ) {
			crc = _crc_ibutton_update( crc, exchangeReport[i] );
		}
		
		// Приступаем к работе только если: совпадает crc8, и адрес не перекрывает бутлоадер
		if( crc == exchangeReport[ REPORT_CRC ] && cmd == 0 ) {
#if CAN_READ_FLASH			
			if( exchangeReport[ REPORT_COMMAND ] == DO_READ_FLASH ) {
				workAddress = 0;
			} else
#endif 
			cmd = exchangeReport[ REPORT_COMMAND ];
			delay = 10;
			
		} else {
			
			// Не совпал crc - отвечаем ошибкой
			return 0xff;
		}
	}
	
    return bytesRemaining == 0; /* return 1 if this was the last chunk */
}

/* ------------------------------------------------------------------------- */

usbMsgLen_t usbFunctionSetup( uchar data[8] )
{
	usbRequest_t *rq = (void *)data;
#if CAN_READ_FLASH
	uchar i, crc = 0;
#endif
#if CAN_COUNT_POLLS
	idlePolls = 0;
#endif

    if( ( rq->bmRequestType & USBRQ_TYPE_MASK ) == USBRQ_TYPE_CLASS ) {    /* HID class request */
#if CAN_READ_FLASH
        if( rq->bRequest == USBRQ_HID_GET_REPORT ) {  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
			
			// Читаем репорт
			exchangeReport[0] = 0;
			
			uchar * ptr = exchangeReport + REPORT_DATA;
			// Передаём сами данные
			for( i = 0; i < SPM_PAGESIZE; i++ ) {
				ptr[i] = pgm_read_byte( workAddress++ );
			}
			// И наконец проставляем crc
			for( i = 0; i < REPORT_CRC; i++ ) {
				crc = _crc_ibutton_update( crc, exchangeReport[i] );
			}
			exchangeReport[ REPORT_CRC ] = crc;

			usbMsgPtr = exchangeReport;
			
            return LOADER_REPORT_SIZE;
			
        } else 
#endif
		if( rq->bRequest == USBRQ_HID_SET_REPORT ) {
            /* since we have only one report type, we can ignore the report-ID */
            bytesRemaining = LOADER_REPORT_SIZE;
            currentAddress = 0;
            return USB_NO_MSG;  /* use usbFunctionWrite() to receive data from host */
        }
    }
    return 0;
}

static inline void tinyFlashInit() {
	// Вектора сброса и INT0 должны вести к бутлодеру. Иначе страница очищена
    if( pgm_read_word( RESET_ADDR ) != LOADER_VECTOR ||
        pgm_read_word( PCINT_ADDR ) != LOADER_VECTOR ) {

		writeInitialPage();
    }
}

// Переход к пользовательской программе
static void leaveBootloader() __attribute__((__noreturn__));
static inline void leaveBootloader() {

	// Пользовательская фукнция очистки    
    bootLoaderExit();
	
    cli();
	// Очищаем регистры
    USB_INTR_ENABLE = 0;
    USB_INTR_CFG = 0;
	// Переключаем PCINT на приложение
	TCNT1 = 0;

    // И переходим по reset-вектору приложения
    asm volatile ("rjmp __vectors - 4");
}

static inline void initForUsbConnectivity() 
{
	uchar i = 0;
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
	tinyFlashInit();
    bootLoaderInit();

	if( bootLoaderStartCondition() ) {
		initForUsbConnectivity();
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
