/*
* powerSupplyArduino.c
*
* Created: 26.12.2019 19:34:45
* Author : Nataly
*/

#define F_CPU 8000000L

#include <avr/io.h>
#include <util/delay.h>
#include "lcd.h"
#include <stdlib.h>
#include "avr/interrupt.h"
#include "string.h"
#include "avr/eeprom.h"

enum modes {NORMAL_MODE, VOLTAGE_SET_MODE, CURRENT_SET_MODE} mode; // 0 - work mode (display cur volumes), 1 - set voltage, 2 - set current

#define VAL2PWM10(val, maxval) val* 1023/maxval
#define CUR_LIMIT_ON PORTB |= 0x01
#define CUR_LIMIT_OFF PORTB&=0xFE
#define CUR_LIMIT_INV PORTB^=0x01
#define BLINK_POST_SCAL 25
#define ENCODER_STEP 10

#define  VOLTAGE_MAX 2500
#define  CUR_MAX 1700



uint8_t blink = 1;

uint8_t blinkPostScal =  BLINK_POST_SCAL;

uint16_t  settedVoltage = 50;
uint16_t  settedVoltageEeprom EEMEM = 100;
uint16_t  settedCurrent = 20;
uint16_t  settedCurrentEeprom EEMEM = 100 ;
uint16_t  CurrentCurrent = 10;
uint16_t  CurrentVoltage = 2550;
//uint8_t test = 0;

//uint8_t mode = 0; // 0 - work mode (display cur volumes), 1 - set voltage, 2 - set current
uint8_t LcdNeed2refresh = 0;

uint8_t lastEncoder;

char lcdStr0[16+1];
char lcdStr1[16+1];
char bufStr[8+1];


void readEncoder(void){
    //mode = PORTB;
    uint8_t curEncoder = PINB & (7<<5);
    if (lastEncoder  == 0 || curEncoder == lastEncoder) {lastEncoder = curEncoder; return;}
    // check press button
    if (curEncoder & (1 << PINB7) && !(lastEncoder & (1 << PINB7) )) {
        mode = (mode+1) % 3;
        switch (mode) { 
            case NORMAL_MODE: eeprom_write_word(&settedCurrentEeprom, settedCurrent);
            case CURRENT_SET_MODE: eeprom_write_word(&settedVoltageEeprom, settedVoltage);
            default: break;
            }
    }
    
    // to left
    if ((curEncoder==128 && lastEncoder == 192 )|| (curEncoder == 160 && lastEncoder == 128)) {
        switch (mode) {
            case VOLTAGE_SET_MODE:
            if (settedVoltage>=ENCODER_STEP) settedVoltage-=10;  break;
            case CURRENT_SET_MODE:
            if (settedCurrent>=ENCODER_STEP) settedCurrent-=10; break;
            default: break;
        }

    }
    // to right
    if ((curEncoder == 128 && lastEncoder == 160) || (curEncoder == 192 && lastEncoder == 128)) {
        switch (mode) {
            case VOLTAGE_SET_MODE:
            if (settedVoltage < VOLTAGE_MAX - ENCODER_STEP) settedVoltage+=10; break;
            case CURRENT_SET_MODE:
            if (settedCurrent < CUR_MAX - ENCODER_STEP)  settedCurrent+=10; break;
            default: break;
        }
        

    }
    // set value to PWM
    OCR1B = VAL2PWM10(settedVoltage, VOLTAGE_MAX);
    OCR1A = VAL2PWM10(settedCurrent , CUR_MAX);
    // fix that display should be refreshed
    LcdNeed2refresh = 1;
    

    lastEncoder = curEncoder;
}

//interrupts
ISR(TIMER0_OVF_vect){
    blinkPostScal--;
    readEncoder();
    if (!blinkPostScal){
        CUR_LIMIT_INV;
        blink=~blink;
        blinkPostScal = BLINK_POST_SCAL;
        //displeyCurVal();
        ++LcdNeed2refresh;
        /*  if (blink){
        lcd_return_home();
        lcd_puts("50");
        } else
        {
        lcd_return_home();
        lcd_puts("  ");
        }*/
    }
}

void displeyVal(void){
    if (LcdNeed2refresh == 0) {return;} else {LcdNeed2refresh = 0;};
    lcd_return_home();

    itoa(mode==1? settedVoltage:CurrentVoltage, bufStr, 10);

    // display voltage in first line
    for (int i = 0 ; i < LCD_COL_COUNT; i++) lcdStr0[i] = ' ';
    char len;
    if (mode != 1 || blink==1 )
    {
        
        lcdStr0[0] = 'V';
        lcdStr0[1] = ':';
        
        len = strlen(bufStr);
        switch (len) {
            case 4: lcdStr0[2] = bufStr[0]; lcdStr0[3] = bufStr[1]; lcdStr0[4] = ','; lcdStr0[5] = bufStr[2]; lcdStr0[6] = bufStr[3]; break;
            case 3: lcdStr0[2] = bufStr[0]; lcdStr0[3] = ','; lcdStr0[4] = bufStr[1]; lcdStr0[5] = bufStr[2];  break;
            case 1:
            case 2: lcdStr0[2] = '0'; lcdStr0[3] = ','; lcdStr0[4] = bufStr[0]; lcdStr0[5] = bufStr[1]; break;
            
        }

        lcdStr0[len==4?7:6] = ' ';
        lcdStr0[len==4?8:7] = 'B';
    }
    // display current in second line
    itoa(mode==2 ? settedCurrent: CurrentCurrent, bufStr, 10);
    len = strlen(bufStr);
    for (int i = 0 ; i < LCD_COL_COUNT; i++) lcdStr1[i] = ' ';
    if ( mode != 2  || blink == 1) {
        lcdStr1[0] = 'C';
        lcdStr1[1] = ':';

        switch (strlen(bufStr)) {
            case 4: lcdStr1[2] = bufStr[0]; lcdStr1[3] = bufStr[1]; lcdStr1[4] = ','; lcdStr1[5] = bufStr[2]; lcdStr1[6] = bufStr[3];  break;
            case 3: lcdStr1[2] = bufStr[0]; lcdStr1[3] = ','; lcdStr1[4] = bufStr[1]; lcdStr1[5] = bufStr[2];  break;
            case 1:
            case 2: lcdStr1[2] = '0'; lcdStr1[3] = ','; lcdStr1[4] = bufStr[0]; lcdStr1[5] = bufStr[1];  break;
        }
        lcdStr1[len==4?7:6] = ' ';
        lcdStr1[len==4?8:7] = 'A';
    }
    lcd_puts(lcdStr0);
    lcd_set_cursor(0,1);
    lcd_puts(lcdStr1);

    //debug info
    
     lcd_set_cursor(12,0);
     itoa(110, bufStr, 10);
     lcd_puts(bufStr);
    
    lcd_set_cursor(10,1);
    itoa(lastEncoder, bufStr, 10);
    lcd_puts(bufStr);
    
   
    
    

}

void initPorts(void){
    // current limit led out mode
    DDRB  = 0x01;
    // encoder pins as input
    DDRB &= ~(7<<5);
    PORTB|= (7<<5); //pull up
    //ADT
    //http://narodstream.ru/avr-urok-22-izuchaem-acp-chast-2/
    ADCSRA |= (1<<ADEN) // ���������� ������������� ���
    |(1<<ADPS2)|(1<<ADPS1);//�������� 64 = 128 ���
    ADMUX |= (1<<REFS1)|(1<<REFS0); //���������� �������� �� 2,56�, ���� ADC0

}

void initTimers(void){
    TIMSK =(1<<TOIE0);  // timer0 OF interrupt enable
    TCCR0 = (1<<CS02); // prescaler for timer 0  = 1/256
    // timer 1 pwm for 2 channel
  //  TCCR1A = 0b10100011; //COM1x1 + COM1x2 = 10b (non invert fast pwm) FOC1A = 0, FOC1B = 0, WGM11 = 1, WGM10 = 1
 //   TCCR1B = 0b00001001; //ICNC1 =0, ICES1 =0, -, WGM13 = 0, WGM12 = 1, SC12 = 0, SC11=0, SC10 = 1 // 10 bit fast PWM , no prescaling

}



int main(void)
{
    
    initPorts();
    initTimers();
    
    lcd_clear();
    lcd_init();
    
    lcd_on();
    // lcd_set_cursor(0,0);
    //lcd_enable_blinking();
    //  lcd_puts("Set:");
    // itoa(settedVoltage,str,10);
    //  lcd_return_home();
    //   lcd_set_cursor(1,1);
    //  lcd_puts(str);
    settedVoltage =  eeprom_read_word(&settedVoltageEeprom);
    settedCurrent = eeprom_read_word(&settedCurrentEeprom);
    sei();    
    
     displeyVal();
     
    while (1)
    {
       // test++;
      //  if (LcdNeed2refresh > 0) {
      //    displeyVal(0);  displeyVal(1); LcdNeed2refresh = 0;}
        displeyVal();
        //   	CUR_LIMIT_INV;
        /*    for (char i=0; i < 2; i++) {
        lcd_set_cursor(0,i);
        lcd_puts("     ");
        }
        _delay_ms(300);
        for (char i=0; i < 2; i++) {
        lcd_set_cursor(0,i);
        lcd_puts("50    ");
        }
        _delay_ms(300);*/
    }
}

