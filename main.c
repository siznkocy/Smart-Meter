
/*========================================================================  
 *
 * SM_ELECTRICAL.c
 *
 * Created: 2020/03/30 09:34:52
 * Author : 8th
 *					
 *					 SMART ELECTRICAL METER
========================================================================*/


#define F_CPU 8000000UL								/* Define CPU Frequency e.g. here its 8MHz */

#include <avr/io.h>									/* Include AVR std. library file */
#include <util/delay.h>								/* Include inbuilt defined Delay header file */
#include <avr/interrupt.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "Keypad.h"

 /************************************************************************/
 /*							DEFINE FUNC			                         */
 /************************************************************************/

#define LCD_Dir DDRC								/* Define LCD data port direction */
#define LCD_Port PORTC								/* Define LCD data port */
#define RS 0										/* Define Register Select (data reg./command reg.) signal pin */
#define EN 1 										/* Define Enable signal pin */

#define BAUD	4800
#define BRC	F_CPU/16/BAUD-1

#define LED	   PB0
#define LED1   PB1
#define LED2   PB2	
#define BTN	   PD2
#define BTN1   PD3
#define RLY	   PB3	

/************************************************************************/
/*							 DECLARATION                                */
/************************************************************************/
 						   //ADC
char String[20];
int16_t Voltage, Current;
int32_t Power = 0;
int32_t Pr = 1000, P ;				//147483647
int Mode = 1, Enter = 0; 
						  //TIMER
int8_t count;
						//Denounce
int cliFlag = 0;
 /************************************************************************/
 /*								LCD FUNCTION                             */
 /************************************************************************/
 
void LCD_Command( unsigned char cmnd )
{
	LCD_Port = (LCD_Port & 0x0F) | (cmnd & 0xF0); /* sending upper nibble */
	LCD_Port &= ~ (1<<RS);				/* RS=0, command reg. */
	LCD_Port |= (1<<EN);				/* Enable pulse */
	_delay_us(1);
	LCD_Port &= ~ (1<<EN);

	_delay_us(200);

	LCD_Port = (LCD_Port & 0x0F) | (cmnd << 4);  /* sending lower nibble */
	LCD_Port |= (1<<EN);
	_delay_us(1);
	LCD_Port &= ~ (1<<EN);
	_delay_ms(2);
}

void LCD_Char( unsigned char data )
{
	LCD_Port = (LCD_Port & 0x0F) | (data & 0xF0); /* sending upper nibble */
	LCD_Port |= (1<<RS);				/* RS=1, data reg. */
	LCD_Port|= (1<<EN);
	_delay_us(1);
	LCD_Port &= ~ (1<<EN);

	_delay_us(200);

	LCD_Port = (LCD_Port & 0x0F) | (data << 4); /* sending lower nibble */
	LCD_Port |= (1<<EN);
	_delay_us(1);
	LCD_Port &= ~ (1<<EN);
	_delay_ms(2);
}

void LCD_Init (void)									/* LCD Initialize function */
{
	LCD_Dir = 0xFF;						/* Make LCD command port direction as o/p */
	_delay_ms(20);						/* LCD Power ON delay always >15ms */
	
	LCD_Command(0x33);
	LCD_Command(0x32);		    		/* send for 4 bit initialization of LCD  */
	LCD_Command(0x28);              	/* Use 2 line and initialize 5*7 matrix in (4-bit mode)*/
	LCD_Command(0x0c);              	/* Display on cursor off*/
	LCD_Command(0x06);              	/* Increment cursor (shift cursor to right)*/
	LCD_Command(0x01);              	/* Clear display screen*/
	_delay_ms(2);
	LCD_Command (0x80);					/* Cursor 1st row 0th position */
}

void LCD_String (char *str)								/* Send string to LCD function */
{
	int i;
	for(i=0;str[i]!=0;i++)				/* Send each char of string till the NULL */
	{
		LCD_Char (str[i]);
	}
}

void LCD_String_xy(char row, char pos, char *str)		/* Send string to LCD with xy position */
{
	if (row == 0 && pos<16)
	LCD_Command((pos & 0x0F)|0x80);		/* Command of first row and required position<16 */
	else if (row == 1 && pos<16)
	LCD_Command((pos & 0x0F)|0xC0);		/* Command of first row and required position<16 */
	LCD_String(str);					/* Call LCD string function */
}

void LCD_Clear()
{
	LCD_Command (0x01);					/* Clear display */
	_delay_ms(2);
	LCD_Command (0x80);					/* Cursor 1st row 0th position */
}
 
/************************************************************************/
/*					ADC FOR VOLTAGE AND CURRENT                         */
/************************************************************************/

void ADC_Init(void){
	
	DDRA	=0x0;
	ADCSRA	|=(7<<ADPS0);
	ADMUX	|=(1<<REFS0);
	//ADCSRA	|=(1<<ADIE);
	ADCSRA	|=(1<<ADEN);
}

int ADC_Read(char channel)
{
	int Ain,AinLow;
	
	ADMUX=ADMUX|(channel & 0x0f);	/* Set input channel to read */

	ADCSRA |= (1<<ADSC);		/* Start conversion */
	while((ADCSRA&(1<<ADIF))==0);	/* Monitor end of conversion interrupt */
	
	_delay_us(10);
	AinLow = (int)ADCL;		/* Read lower byte*/
	Ain = (int)ADCH*256;		/* Read higher 2 bits and 
					Multiply with weight */
	Ain = Ain + AinLow;				
	return(Ain);			/* Return digital value*/
}

void ADC_Mult_Read(void){
	switch(ADMUX)
	{

		case 0x40:
		LCD_Command(0x84);
		Voltage = (26)*((5*ADC_Read(0))/1023);
		itoa(Voltage,String,10);
		LCD_String_xy(0,4,String);
		LCD_String(" ");
		ADMUX = 0x41;
		break;

		case 0x41:
		LCD_Command(0xC4);
		Current = (135)*((5*ADC_Read(1))/1023);
		itoa(Current,String,10);
		LCD_String_xy(1,4,String);
		LCD_String(" ");
		ADMUX = 0x40;
		break;
	}
}

//==================== MULTICATION [Function] ===========================

long int Product(int multiplier, int multiplicand)
{
	
	int32_t productRegister = multiplier;
	

	for (int n = 0; n < 16; n++) {
		if ((productRegister & 1) == 1) {
			productRegister += ((int32_t)multiplicand) << 16;
		}
		productRegister >>= 1;
	}

	return productRegister;
}


/************************************************************************/
/*							uSART						 	            */
/************************************************************************/
void uSART_init(){

	UBRRH =(BRC)>>8;
	UBRRL = BRC;
	
	UCSRB = (1<<TXEN)|(1<<RXEN);
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); // >>>11
}

void uSART_TX(char x){
	UDR = x;
	while(!(UCSRA & (1<<UDRE)));
}

char uSART_RX(){
	while (!(UCSRA & (1<<RXC)));
	return UDR;
}

void uSART_Data(char *data){
	while(*data!= 0){
		uSART_TX(*data++);
	}
}

/************************************************************************/
/*                           MODE                                       */
/************************************************************************/
void Init_INT()
{
	DDRB	|=(1<<LED)|(1<<LED1)|(1<<LED2)|(1<<RLY);
	
	PORTB	|=(1<<LED)|(1<<LED1)|(0<<LED2)|(0<<RLY);
	PORTD	|=(1<<BTN)|(1<<BTN1);
	
	MCUCR	|=~(3<<ISC10)|~(3<<ISC00);
	GICR	|=(1<<INT1)|(1<<INT0);
}

void DebounceISR()
{
	if (cliFlag == 0 || ((TCNT1 + 1) && OCR1A))
	{
		sei();
	}
	else
	{
		cli();
		_delay_us(20);
		cliFlag = 1;
	}
}

void PrePaid(void)
{
	Power = Product((Current/100),Voltage);
	Pr = Pr - Power;
	sprintf(String, "%" PRIu32, Pr);
	LCD_Command(0x86);
	LCD_String_xy(1,9,String);
}

void PostPaid(void){
	Power = Product((Current/100),Voltage);
	P += Power;
	sprintf(String, "%" PRIu32, P);
	LCD_Command(0x86);
	LCD_String_xy(1,9,String);
}
/************************************************************************/
/*							 TIMER			                            */
/************************************************************************/
void TIMER1_Init(void){
	DDRB	|= (1<< LED);
	PORTD	|= (1<< LED);

	//TCCR1A	|=(1 << COM1A1)|(1<<COM1A1);				/* Clear on Output compare */
	TCCR1B	= (1 << WGM12); 							/* CTC Mode */
	OCR1A	= 23437.;										/* 8MHz 1 sec/cycle */
	TIMSK	= (1 << OCIE1A);

	sei();

	TCCR1B	|=(1<<CS12)|(1<<CS10);						/* 1024 Pre-scaler >>> 0.2Hz >> begin */
}

/*void Second_re(void){

		memset(String,0,strlen(String));
		ADC_Mult_Read();
		_delay_ms(10);
		strcpy(String," ");
		Power = Product(Current,Voltage);
		P -= Power;
		sprintf(String, "%" PRIu32, P);
		LCD_Command(0x86);
		LCD_String_xy(1,9,String);
		uSART_Data(String);
		uSART_TX(0x0d);
}	 */

/************************************************************************/
/*							  MAIN                                      */
/************************************************************************/

int main(void)
{

	ADC_Init();
	_delay_us(50);

	TIMER1_Init();
	LCD_Init();											/* Initialization of LCD*/
	uSART_init();
	Init_INT();	
	
	// Need to be moved into a function										

// 	ADC_Mult_Read();
// 	_delay_ms(10);
// 	Power = Product(Current,Voltage);
// 	itoa(Power,String,10);
// 	LCD_Command(0x86);
// 	LCD_String_xy(1,9,String);
	
	while(1){
		//here:
			 DebounceISR();
// 			 LCD_Command(0x02);
// 			 LCD_String("V:");								/* Write string on 1st line of LCD*/
// 			 LCD_Command(0xC0);								/* Go to 2nd line*/
// 			 LCD_String("I:");								/* Write string on 2nd line*/
// 			 LCD_String_xy(0,8,"P:Pre");
					 if( Mode == 0 && Enter == 0)
					{
					  LCD_Command(0x02);
					  LCD_String("V:");								/* Write string on 1st line of LCD*/
					  LCD_Command(0xC0);								/* Go to 2nd line*/
					  LCD_String("I:");								/* Write string on 2nd line*/
					  LCD_String_xy(0,8,"P:Post");
					//	goto here
							ADC_Mult_Read();
							_delay_us(500);
							PostPaid();
							PORTD |=(0<<RLY);

							_delay_ms(100);
							uSART_Data(String);
							uSART_TX(0x0d);
					}
					else if(Mode == 1 && Enter == 0){
					  
								if(Pr <= Power)
									{
									  PORTB |=(1<<RLY);
									LCD_Command(0x02);
									LCD_String("V: 0   ");								/* Write string on 1st line of LCD*/
									LCD_Command(0xC0);								/* Go to 2nd line*/
									LCD_String("I: 0   ");								/* Write string on 2nd line*/
									LCD_String_xy(0,8,"P:Pre'\0'");
									//	goto here
									ADC_Mult_Read();
									LCD_Command(0x86);
									LCD_String_xy(1,9,"0 Watts");

								_delay_ms(100);
								uSART_Data("0");
								uSART_TX(0x0d);  
									}
								else
									{
									  PORTD |=(0<<RLY);
									  LCD_Command(0x02);
									  LCD_String("V:");								/* Write string on 1st line of LCD*/
									  LCD_Command(0xC0);								/* Go to 2nd line*/
									  LCD_String("I:");								/* Write string on 2nd line*/
									  LCD_String_xy(0,8,"P:Pre'\0'");
									  //	goto here
									  ADC_Mult_Read();
									  _delay_us(500);
									  PrePaid();
								_delay_ms(100);
								uSART_Data(String);
								uSART_TX(0x0d);
									}
								
		
					}
					else if(Mode == 1 && Enter == 1){
					LCD_Clear();
					LCD_Command(0x02);
					LCD_String("Voucher Valid");
					LCD_Command(0xC0);								/* Go to 2nd line*/
					LCD_String("70.2kW 'Enter'");
					Pr = 70200;
					}
 			_delay_ms(100);
		uSART_Data(String);
		uSART_TX(0x0d);
		}
 //return 0;
}

ISR(TIMER1_COMPA_vect){
	PORTB ^=(1<<LED);
	count++	;
	LCD_Clear();
	cli();
	if(!(Mode == 1 && Enter == 1));
	if(count == 1000000);
		{(LCD_Clear());
		  count=0;
		}
	 if(Pr <= Power)(PORTD =(1<<RLY));
}

ISR(INT0_vect)
{
   PORTB ^=(1<<LED1)|(1<<RLY);
   cliFlag = 0;
	Mode ^=1;

}

ISR(INT1_vect)
{
	PORTB ^=(1<<LED2);
	cliFlag = 0;
	Enter ^=1 ;
}
 /*
ISR(BADISR_vect){
	PORTB ^=(1<<LED2);	
}
  
void Diplay(void)
{
	if(mode == 0)
	{
	   TCCR1A	|=(1<<COM1A1)|(1<<COM1A1);		// Clear on Output compare 
	   TCCR1B	=(1<< WGM12); 					// CTC Mode 
	   OCR1A	= 7812;							// 8MHz 1 sec/cycle 

	   TCNT1	= 0;
	   TCCR1B	|=(1<<CS12)|(1<<CS10);			// 1024 Pre-scaler >>> 0.2Hz >> begin
	}

}
  */

	
	
