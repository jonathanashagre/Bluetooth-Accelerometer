#define F_CPU 3333333
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUF_SIZE 128
#define BLE_RADIO_PROMPT "CMD> "

void twiInit() {
    // TODO Fill in with your code from Lab 6
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm;
    TWI0.MSTATUS =  TWI_WIF_bm |  TWI_RIF_bm | TWI_RXACK_bm;
    
    TWI0.MBAUD = 10;
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
    TWI0.MCTRLA = TWI_ENABLE_bm;
}

void readAccelerometerBytes(uint8_t *dest, uint8_t len) {
    // TODO Fill in with your code from Lab 6
    int count = 0;
    TWI0.MADDR = (0x19 << 1) | 0;
    while(!(TWI0.MSTATUS & TWI_WIF_bm));
    
    TWI0.MDATA = 0x02;
    while(!(TWI0.MSTATUS & TWI_WIF_bm));
    TWI0.MCTRLB = TWI_MCMD_REPSTART_gc; 
    
    TWI0.MADDR = (0x19 << 1) | 1;
    while(!(TWI0.MSTATUS & TWI_RIF_bm));
    
    while(count < len){
        
        while(!(TWI0.MSTATUS & TWI_RIF_bm));
        dest[count] = TWI0.MDATA;
        count++;
        if (count != len){
            TWI0.MCTRLB = TWI_ACKACT_ACK_gc | TWI_MCMD_RECVTRANS_gc;
        }
    }
    TWI0.MCTRLB = TWI_ACKACT_NACK_gc | TWI_MCMD_STOP_gc;
}

// Code in the getting started guide appears to do same thing, but they do not
// know how to code cleanly.
// This is all based on formula in Table 23-1 of the ATmega 3208 data sheet
// Need to shift value left 6 bits to follow format specified in data sheet:
// 16-bit number, 10 bits are whole number part, bottom 6 are fractional part
// The +0.5 is to force the result to be rounded *up* rather than down.
// SAMPLES_PER_BIT: 16, for normal asynchronous mode. Given in data sheet.
#define SAMPLES_PER_BIT 16
#define USART_BAUD_VALUE(BAUD_RATE) (uint16_t) ((F_CPU << 6) / (((float) SAMPLES_PER_BIT) * (BAUD_RATE)) + 0.5)
//indicate usart bits will be 8 bits in SIZE
void usartInit() {
    // TODO Fill in with USART peripheral initialization code
    PORTA.DIR |= PIN0_bm;
    PORTA.DIR &= ~PIN1_bm;
    USART0.BAUD = USART_BAUD_VALUE(9600);
    
    USART0.CTRLB |= USART_TXEN_bm;
    USART0.CTRLB |= USART_RXEN_bm;
    USART0.CTRLC |= USART_CHSIZE_8BIT_gc;
    USART0.CTRLC |= USART_CMODE_ASYNCHRONOUS_gc;
    
}

void usartWriteChar(char c) {
    // TODO fill this in
    //transmits character
    while (!(USART0.STATUS & USART_DREIF_bm));
    USART0.TXDATAL = c;
}

void usartWriteCommand(const char *cmd) {
    for (uint8_t i = 0; cmd[i] != '\0'; i++) {
        usartWriteChar(cmd[i]);
    }
}

char usartReadChar() {
    // TODO fill this in and return correct value
    while(!(USART0.STATUS & USART_RXCIF_bm));
    //receives character
    return USART0.RXDATAL;
}

void usartReadUntil(char *dest, const char *end_str) {
    // Zero out dest memory so we always have null terminator at end
    memset(dest, 0, BUF_SIZE);
    uint8_t end_len = strlen(end_str);
    uint8_t bytes_read = 0;
    while (bytes_read < end_len || strcmp(dest + bytes_read - end_len, end_str) != 0) {
        dest[bytes_read] = usartReadChar();
        bytes_read++;
    }
}
void bleInit(const char *name) {
    // Put BLE Radio in "Application Mode" by driving F3 high
    PORTF.DIRSET = PIN3_bm;
    PORTF.OUTSET = PIN3_bm;

    // Reset BLE Module - pull PD3 low, then back high after a delay
    PORTD.DIRSET = PIN3_bm | PIN2_bm;
    PORTD.OUTCLR = PIN3_bm;
    _delay_ms(10); // Leave reset signal pulled low
    PORTD.OUTSET = PIN3_bm;

    // The AVR-BLE hardware guide is wrong. Labels this as D3
    // Tell BLE module to expect data - set D2 low
    PORTD.OUTCLR = PIN2_bm;
    _delay_ms(200); // Give time for RN4870 to boot up

    char buf[BUF_SIZE];
    // Put RN4870 in Command Mode
    usartWriteCommand("$$$");
    usartReadUntil(buf, BLE_RADIO_PROMPT);

    // Change BLE device name to specified value
    // There can be some lag between updating name here and
    // seeing it in the LightBlue phone interface
    strcpy(buf, "S-,");
    strcat(buf, name);
    strcat(buf, "\r\n");
    usartWriteCommand(buf);
    usartReadUntil(buf, BLE_RADIO_PROMPT);
      
    // TODO 1: Send command to remove all previously declared BLE services
    usartWriteCommand("PZ\r\n");
    // TODO 2: Add a new service. Feel free to use any ID you want from the
    // BLE assigned numbers document. Avoid the "generic" services.
    
    usartWriteCommand("PS,FC94\r\n");
    // TODO 3: Add a new characteristic to the service for your accelerometer
    // data. Pick any ID you want from the BLE assigned numbers document.
    // Avoid the "generic" characteristics.
    usartWriteCommand("PC,2A80,0A,01\r\n");
    // TODO 4: Set the characteristic's initial value to hex "00".
    usartWriteCommand("SHW,0072,00\r\n");
}

int main() {
    twiInit();
    usartInit();
//    TCAinit();
    // TODO Change the argument to bleInit() to some unique name
    bleInit("Jonathan");

    int16_t accel[3];
    char buf[BUF_SIZE];

    while (1) {
        readAccelerometerBytes((uint8_t *) accel, 6);
        // Accelerometer readings only contain 12 bits of meaningful data
        accel[0] >>= 4;
        accel[1] >>= 4;
        accel[2] >>= 4;
        // TODO: Update your service's characteristic value according to the
        // accelerometer data.
        // 1g along x axis: Update to hex value "00"
        // 1g along y axis: Update to hex value "01"
        // 1g along z axis: Update to hex value "02"
        // Not 1g along any axis: Update to hex value "99"
        if(abs(accel[0]) >= 1023){
            usartWriteCommand("SHW,0072,00\r\n");
        }
        else if(abs(accel[1]) >= 1023){
            usartWriteCommand("SHW,0072,01\r\n");
        
        }
        else if(abs(accel[2]) >= 1023){
            usartWriteCommand("SHW,0072,02\r\n");
        }
        else{
            usartWriteCommand("SHW,0072,99\r\n");
        }
        // Wait for service characteristic update operation to complete
        usartReadUntil(buf, BLE_RADIO_PROMPT);
        _delay_ms(1000);
    
