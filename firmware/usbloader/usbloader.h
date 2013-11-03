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


#ifndef __ASSEMBLER__

	#include <avr/eeprom.h>
	#include <avr/pgmspace.h>

	#define START_JUMPER_PIN 0
	#define digitalRead(pin) (PINB & _BV(pin))
	#define bootLoaderCondition() 1
	
	// Условие входа в загрузчик. Иначе будет выполнен переход на основную программу
	static inline uint8_t bootLoaderStartCondition()
	{
		// Если замкнута с землёй нога 0
		if( !digitalRead( START_JUMPER_PIN ) ) return 1;
		// Если в векторе INT0 записан NOP (это может быть только если память пуста)
		if( pgm_read_byte( 3 ) == 0xff ) return 1;
		// Если основная программа принудительно запустила bootloader
		if( TCNT1 == 0xff ) return 1;
		return 0;
	}

	// Инициализация загрузчика. Выполняется сразу после RESET-а
	static inline void  bootLoaderInit(void) 
	{
		// DeuxVis pin-0 pullup
		PORTB |= _BV(START_JUMPER_PIN); // has pullup enabled
	}
	
	// Очистка всех использованых выше регистров
	static inline void  bootLoaderExit(void) 
	{
		PORTB = 0;
		// DeuxVis pin-0 pullup
	}
	
	// Действие, выполняемое по комманде bootloaderexit
	static inline void leaveLoader()
	{
	}
	
#endif

// Чем больше единичек, тем больше вес бутлодера
// Оптимально проставлять только необходимые функции

// Может ли считать количество idle циклов
#define CAN_COUNT_POLLS 1
// Может ли тереть EEPROM
#define CAN_ERASE_EEPROM 1
// Может ли писать память
#define CAN_READ_FLASH 1
// Может ли покидать загрузчик по команде
#define CAN_LEAVE_LOADER 1
// Если объявлен, то в процессе записи будет мигать светодиодом.
#define LED_PIN 4



// Константы, свойственные протоколу и самой тиньке
#define LOADER_REPORT_SIZE ( SPM_PAGESIZE + 2 )
#define SPM_PAGESHIFT 6
#define REPORT_COMMAND 0
#define REPORT_DATA 2
#define REPORT_CRC 1

#define LOADER_VECTOR ( 0xC000 + ( BOOTLOADER_ADDRESS / 2 ) - 1 )
#define APP_RESET_SHIFT ( ( FLASHEND + 1 - BOOTLOADER_ADDRESS ) / 2 + 2 )
#define APP_PCINT_SHIFT ( ( FLASHEND + 1 - BOOTLOADER_ADDRESS ) / 2 + 1 + 2 )
#define APP_RESET_ADDR ( BOOTLOADER_ADDRESS - 4 )
#define APP_PCINT_ADDR ( BOOTLOADER_ADDRESS - 2 )
#define RESET_ADDR 0
#define PCINT_ADDR 4

#endif /* USBLOADER_H_ */