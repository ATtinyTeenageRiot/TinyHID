/*
 * config.h
 *
 * Created: 04.11.2013 10:56:57
 *  Author: Alexey
 */ 


#ifndef CONFIG_H_
#define CONFIG_H_

#include "avr/io.h"

// Next page after reloader
#define INFO_ADDRESS 0x0280

// New bootloader address
#define INFO_OFFSET INFO_ADDRESS
// New bootloader crc16
#define INFO_CRC ( INFO_ADDRESS + 2 )
// New bootloader data
#define BOOTLOADER_DATA ( INFO_ADDRESS + SPM_PAGESIZE )

// LED PIN. Comment if LED not present
#define LED_PIN 4

#endif /* CONFIG_H_ */