/*
 * reloader.c
 *
 * Created: 04.11.2013 10:42:36
 *  Author: Alexey
 */ 


#include <avr/io.h>
#include <util/crc16.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include "config.h"

static void switchBack(uint8_t code)
{
#ifdef LED_PIN
	for( uint8_t i = 0; i < 8; i++ ) {
		PORTB |= _BV( LED_PIN );
		if( code & 0x1 ) _delay_ms( 500 );
		else _delay_ms( 100 );
		PORTB &= ~_BV( LED_PIN );
		_delay_ms( 100 );
		code >>= 1;
	}
#endif
	TCNT1 = 0xff;
	uint16_t vect = 0;
    asm volatile ("ijmp" :: "z"( vect ));
}

#define boot_sig_byte_get(addr) \
(__extension__({                      \
      uint8_t __result;                         \
      __asm__ __volatile__                      \
      (                                         \
        "sts %1, %2\n\t"                        \
        "lpm %0, Z" "\n\t"                      \
        : "=r" (__result)                       \
        : "i" (_SFR_MEM_ADDR(__SPM_REG)),       \
          "r" ((uint8_t)_BV(__SPM_ENABLE)),		\
          "z" ((uint16_t)(addr))                \
      );                                        \
      __result;                                 \
}))

int main(void)
{
#ifdef LED_PIN
	// +/- 50% does not matter, this is LED blink time
	OSCCAL = boot_sig_byte_get( 1 );
	DDRB |= _BV( LED_PIN );
#endif
	
///////////////////////////////////////////////////////////////////////////////
//                      Phase 1: perform data check                          //
///////////////////////////////////////////////////////////////////////////////

	uint16_t address = pgm_read_word( INFO_OFFSET );
	uint16_t size = FLASHEND - address + 1;
	
	// Empty reset vector - skip first phases, no way back
	if( pgm_read_word( 0 ) != 0xffff ) {
	
		// Bootloader address must be in available flash space
		if( address >= FLASHEND - 1 ) switchBack( 0x01 );
		// Bootloader address must be aligned by page.
		if( address % SPM_PAGESIZE != 0 ) switchBack( 0x05 );
	
		// Bootloader must fit into available memory.
		if( size > ( FLASHEND - BOOTLOADER_DATA + 1 ) / 2 - SPM_PAGESIZE ) switchBack( 0x07 );
	
		uint16_t crc = 0xffff;
	
		uint16_t end = BOOTLOADER_DATA + size;
		for( uint16_t addr = BOOTLOADER_DATA; addr < end; addr++ ) {
			crc = _crc16_update( crc, pgm_read_byte( addr ) );
		}
		// CRC16 must be correct
		if( crc != pgm_read_word( INFO_CRC ) ) switchBack( 0x21 );
	
		uint16_t op = pgm_read_word( BOOTLOADER_DATA );
		// First instruction must be rjmp to valid address
		if( op < 0xc000 && op >= 0xc000 + ( size / 2 ) ) switchBack( 0x30 );
	

///////////////////////////////////////////////////////////////////////////////
//                     Phase 2: clear first data page                        //
///////////////////////////////////////////////////////////////////////////////

		// No way back. Reloader starts on device reset.
		boot_page_erase( 0 );
	}
	
	
///////////////////////////////////////////////////////////////////////////////
//                     Phase 3: write new bootloader                         //
///////////////////////////////////////////////////////////////////////////////

#ifdef LED_PIN
	PORTB |= _BV( LED_PIN );
	_delay_ms( 2000 );
	PORTB &= ~_BV( LED_PIN );
	_delay_ms( 1000 );
#endif

	// Erase application vectors
	boot_page_erase( address - SPM_PAGESIZE );

	uint16_t src = BOOTLOADER_DATA;
	uint16_t dst = address;
	while( dst <= FLASHEND - SPM_PAGESIZE + 1 ) {
		// First - erase page
		boot_page_erase( dst );
		boot_spm_busy_wait();
		
		// Next - fill page
		uint16_t end = src + SPM_PAGESIZE;
		while( src < end ) {
			uint16_t word = pgm_read_word( src );
			boot_page_fill( dst, word );
			boot_spm_busy_wait();
			src += 2;
			dst += 2;
		}
		// Final - write page
		boot_page_write( dst - SPM_PAGESIZE );
	}
	
	uint16_t vect = address / 2;
    asm volatile ("ijmp" :: "z"( vect ));
}