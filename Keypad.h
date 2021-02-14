/*
 * Keypad.h
 *
 * Created: 21/07/2020 11:19:59 AM
 *  Author: 8th
 */ 


#ifndef KEYPAD_H_
#define KEYPAD_H_
//#define  F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>

#define KEY_PRT 	PORTB
#define KEY_DDR		DDRB
#define KEY_PIN		PINB

char keyfind();

#endif /* KEYPAD_H_ */