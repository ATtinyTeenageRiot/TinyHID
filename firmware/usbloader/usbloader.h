/*
 * usbloader.h
 *
 * Created: 29.10.2013 21:03:45
 *  Author: Alexey
 */ 


#ifndef USBLOADER_H_
#define USBLOADER_H_
#include <avr/io.h>

#define USB_CFG_DMINUS_BIT      2
/* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
 * This may be any bit in the port.
 */
#define USB_CFG_DPLUS_BIT       1
/* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
 * This may be any bit in the port. Please note that D+ must also be connected
 * to interrupt pin INT0!
 */


// Optional capabilities. Read flash memory, erase eeprom and other
// Any capability increase bootloader size, so set 1 only for caps, that you need

// Set to 1 to bootloader could clear EEPROM, or 0 otherwise
#define CAN_ERASE_EEPROM 0
// Set to 1 to bootloader could read FLASH, or 0 otherwise
#define CAN_READ_FLASH 0
// Set to 1 to bootloader could exit by the USB command, or 0 otherwise
#define CAN_LEAVE_LOADER 1
// Set to 2 to bootloader can check crc, 1 to check sum of data bytes, 0 to no check.
#define CAN_CHECK_DATA 0
// Set to PIN number, to blink LED while FLASH write, or comment otherwise
// #define LED_PIN 4
// Set to PIN number, to use jumper for Loader Start Condition, or comment otherwise
#define START_JUMPER_PIN 0
// Set to 1 for using osccal, and adding some capabilities for USB hub support.
#define CAN_SUPPORT_HUB 0


#ifndef BOOTLOADER_ADDRESS
#define BOOTLOADER_ADDRESS ( BOOTLOADER_WADDRESS * 2 )
#endif

#ifndef __ASSEMBLER__

	#include <avr/eeprom.h>
	#include <avr/pgmspace.h>

	#define digitalRead(pin) (PINB & _BV(pin))
	#define bootLoaderCondition() 1
	
	// Bootloader start condition. Otherwise jump to application
	static inline uint8_t bootLoaderStartCondition()
	{
		// Start bootloader if START_JUMPER_PIN is connected to ground
#ifdef START_JUMPER_PIN		
		if( !digitalRead( START_JUMPER_PIN ) ) return 1;
#endif
		// Start bootloader if INT0 vector contains NOP command (which means than flash is empty)
		if( pgm_read_byte( BOOTLOADER_ADDRESS - 3 ) == 0xff ) return 1;
		// Start bootloader by following application code:
		// WRITE DOWN THIS CODE IN YOUR APP
		// cli();
		// TCCR1 = 0;
		// TCNT1 = 0xff;
		// asm volatile ("rjmp __vectors");
		// END CODE
		if( TCNT1 == 0xff ) return 1;
		return 0;
	}

	// Bootloader init. Typically used for pin pullup's
	static inline void  bootLoaderInit(void) 
	{
		// DeuxVis pin-0 pullup
#ifdef START_JUMPER_PIN
		PORTB |= _BV(START_JUMPER_PIN); // has pullup enabled
#endif
	}
	
	static inline void  bootLoaderInitiated(void) 
	{
		PORTB = 0;
	}
	
	// Bootloader clear. Typically used for clearing pullap's
	#define bootLoaderExit() 
	
	// Additional acion on program leaving bootloader
	#define leaveLoader()
	
#endif

// Protocol constants
// HID report length
#define LOADER_REPORT_SIZE ( SPM_PAGESIZE + 4 )
// Command offset in report
#define REPORT_COMMAND 0
// Data offset in report
#define REPORT_DATA 4
// CRC offset in report
#define REPORT_CRC 1
// Command check offset in report
#define REPORT_CMD_CHECK 3
// CRC algorithm
#define CRC_FUNCTION _crc16_update
// CRC data type (uint16_t or uint8_t)
#define crc_t uint16_t
// CRC initial value
#define CRC_INITIAL 0xffff



#define LOADER_VECTOR ( 0xC000 + ( BOOTLOADER_ADDRESS / 2 ) - 1 )
#define APP_RESET_SHIFT ( ( FLASHEND + 1 - BOOTLOADER_ADDRESS ) / 2 + 2 )
#define APP_PCINT_SHIFT ( ( FLASHEND + 1 - BOOTLOADER_ADDRESS ) / 2 + 1 + 2 )
#define APP_RESET_ADDR ( BOOTLOADER_ADDRESS - 4 )
#define APP_PCINT_ADDR ( BOOTLOADER_ADDRESS - 2 )
#define RESET_ADDR 0
#define PCINT_ADDR 4

#endif /* USBLOADER_H_ */