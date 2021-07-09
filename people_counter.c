/**
 * Oscillator 0: T0
 * Oscillator 1: T5
 *
 * Make sure earth ground is connected to Arduino ground.
 * Commands:
 *  r       equalize values
 *  v       enable printing of values
 *  V       disable printing of values
 *  R       Set offset to 0
 *  s<num>; Set 7 segment to <num>
 *  t<num>; Set threshold to <num>
 * 
 *      Auto-reset of comparison
 * seconds  |   |   |   |   |   |   |   |   |   |
 * osc0     ‾|___|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 * osc1     
 * osc0_f   _|‾‾‾‾‾‾‾‾‾‾‾|_______________________
 * osc1_f   
 * 
 *      osc0, then osc1
 * seconds  |   |   |   |   |   |   |   |   |   |
 * osc0     ‾|___|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 * osc1     ___________|‾‾‾|_____________________
 * osc0_f   _|‾‾‾‾‾‾‾‾‾‾‾|_______________________
 * osc1_f   _____________________________________
 * entering ___________|‾‾‾|_____________________
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

#define WAITTX {flags|=0b00000010;while(flags&0b00000010);}
// #define PRINT_VALUES
// #define PRINT_OSC




volatile unsigned short int osc0_ovf = 0;
volatile unsigned short int osc1_ovf = 0;
volatile unsigned long int osc0_tcnt = 0;
volatile unsigned long int osc1_tcnt = 0;
volatile unsigned long int osc0;
volatile unsigned long int osc0_raw;
volatile unsigned long int osc1;
volatile long int osc0_offset __attribute__ ((section (".noinit")));

volatile unsigned long int osc0_threshold = 0;

volatile unsigned char flags = 0b00010000;  // {,print_offset,reading_seg,val_print_en,printing_threshold,reading_threshold,printing,print_results}
volatile unsigned char oscstat;             // {,,,,entering,exiting,osc1_first,osc0_first}
volatile unsigned char time_since_comp;
volatile unsigned char time_since_motion;
unsigned long tmp;
volatile unsigned long seg_disp = 0;

const unsigned char seg_lut[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};


#define TXBUF 32
volatile unsigned char string_ptr;
char print_string[TXBUF];

// Non-blocking version for use in ISRs
void print_num_nh(unsigned long n) {
    tmp = n;
    string_ptr = TXBUF-1;
    for (unsigned char x = 0; x<TXBUF; x++) {
        if (tmp % 10) string_ptr = TXBUF-1-x;
        print_string[TXBUF-1-x] = (tmp % 10) + '0';
        tmp = tmp / 10;
    }
    UDR0 = print_string[string_ptr];
    string_ptr++;
}

void print_num(unsigned long n) {
    print_num_nh(n);
    flags |= 0b00000010;
    while (flags & 0b00000010);
}

void print_char(char c) {
    string_ptr = TXBUF;
    UDR0 = c;
    flags |= 0b00000010;
    while (flags & 0b00000010);
}

ISR(TIMER0_OVF_vect) {
    osc0_ovf++;
}

ISR(TIMER5_OVF_vect) {
    osc1_ovf++;
}

ISR(TIMER3_COMPA_vect) {
    osc0_tcnt = TCNT0 & 255;
    osc1_tcnt = TCNT5 & 65535;
    osc0_raw = (((unsigned long)osc0_ovf << 8) | osc0_tcnt);                // Add an offset to make them closer together
    osc1 = ((unsigned long)osc1_ovf << 16) | osc1_tcnt;
    osc0 = osc0_raw + osc0_offset;
    if (osc0_tcnt == 0) osc0 += 256;
    if (oscstat & 0b00001100) time_since_motion++;                          // If entering or exiting, increment the counter
    if (time_since_motion > 10) {                                           // No motion for 1 sec, reset counter
        oscstat &= 0b1111110011;
        time_since_motion = 0;
    }
    if (oscstat & 0b00000011) time_since_comp++;                            // Increment the counter when a sensor detects something.
    if (time_since_comp > 20) {                                             // Reset after 2 secs
        time_since_comp = 0;
        oscstat &= 0b11111100;
    }
    TCNT0 = 0;
    TCNT5 = 0;
    osc0_ovf = 0;
    osc1_ovf = 0;
    flags |= 0b00000001;
    TIFR0 |= 0b00000001;
    TIFR5 |= 0b00000001;
}

ISR(USART0_TX_vect) {
    if (string_ptr < TXBUF) {
        UDR0 = print_string[string_ptr];
        string_ptr++;
    } else if (string_ptr == TXBUF) {
        flags &= 0b11111101;
        string_ptr++;
    }
}

ISR(USART0_RX_vect) {
    if (flags & 0b00000100) {
        if (('0' <= UDR0) && (UDR0 <= '9')) {
            osc0_threshold = osc0_threshold * 10 + UDR0 - '0';
        } else if (UDR0 == ';') {
            flags &= 0b11111011;
            flags |= 0b00001000;
            //print_num_nh(osc0_threshold);
        }
    } else if (flags & 0b00100000) {
        if (('0' <= UDR0) && (UDR0 <= '9')) {
            seg_disp = UDR0 - '0';
            PORTA = seg_lut[seg_disp];
        } else if (UDR0 == ';')
            flags &= 0b11011111;
    } else if (UDR0 == 'r') {
        osc0_offset = osc0_offset + osc1 - osc0;
        flags |= 0b01000000;
    } else if (UDR0 == 'R') {
        osc0_offset = 0;
        strcpy(print_string + TXBUF - 10, "eq. reset\n");
        string_ptr = TXBUF - 10 + 1;
        UDR0 = print_string[TXBUF - 10];
    } else if (UDR0 == 't') {
        osc0_threshold = 0;
        flags |= 0b00000100;
    } else if (UDR0 == 'v') {
        flags |= 0b00010000;
    } else if (UDR0 == 'V') {
        flags &= 0b11101111;
    } else if (UDR0 == 's') {
        flags |= 0b00100000;
    }
}



int main() {
    // put your setup code here, to run once:
    UBRR0 = 16;
    UCSR0A = 0b00000010;
    UCSR0B = 0b11011000;
    UCSR0C = 0b00000110;

    TCCR0A = 0b00000000;
    TCCR0B = 0b00000111;
    TIMSK0 = 0b00000001;

    // every 100 ms
    TCCR3A = 0b00000000;
    TCCR3B = 0b00001100;
    TIMSK3 = 0b00000010;
    OCR3A  = 6249;

    TCCR5A = 0b00000000;
    TCCR5B = 0b00000111;
    TIMSK5 = 0b00000001;
    
    DDRA = 0b01111111;
    PORTA = seg_lut[seg_disp];

    sei();
    
    while (1) {
        // put your main code here, to run repeatedly:
        if (flags & 0b00000001) {
            if (flags & 0b00010000) {
                print_num(osc0);
                print_char('\t');
                print_num(osc1);
                //print_num(osc0_tcnt);
                osc0_tcnt = 0;
                print_char('\n');
            }
            if (osc0 < osc0_threshold) {
#ifdef PRINT_OSC
                strcpy(print_string + TXBUF - 5, "osc0\n");
                string_ptr = TXBUF - 5 + 1;
                UDR0 = print_string[TXBUF - 5];
                WAITTX;
#endif
                // Other oscilltor is unset
                if (!(oscstat & 0b00000010)) {
                    oscstat |= 0b00000001;
                }
                // Other oscillator has been set, but is not currently set
                else if (osc1 >= osc0_threshold) {
                    time_since_motion = 0;
                    if (!(oscstat & 0b00000100)) {
                        PORTA = seg_lut[--seg_disp];
                        strcpy(print_string + TXBUF - 4, "out\n");
                        string_ptr = TXBUF - 4 + 1;
                        UDR0 = print_string[TXBUF - 4];
                        WAITTX;
                    }
                    oscstat |= 0b00000100;
                }
            }
            if (osc1 < osc0_threshold) {
#ifdef PRINT_OSC
                strcpy(print_string + TXBUF - 5, "osc1\n");
                string_ptr = TXBUF - 5 + 1;
                UDR0 = print_string[TXBUF - 5];
                WAITTX;
#endif
                // Other oscillator is unset
                if (!(oscstat & 0b00000001)) {
                    oscstat |= 0b00000010;
                }
                // Other oscillator has been set, but is not currently set
                else if (osc0 >= osc0_threshold) {
                    time_since_motion = 0;
                    if (!(oscstat & 0b00001000)) {
                        PORTA = seg_lut[++seg_disp];
                        strcpy(print_string + TXBUF - 3, "in\n");
                        string_ptr = TXBUF - 3 + 1;
                        UDR0 = print_string[TXBUF - 3];
                        WAITTX;
                    }
                    oscstat |= 0b00001000;
                }
            }
            flags &= 0b11111110;
        }
        if (flags & 0b00001000) {
            strcpy(print_string + TXBUF - 14, "threshold is: ");
            string_ptr = TXBUF - 14 + 1;
            UDR0 = print_string[TXBUF - 14];
            WAITTX;
            print_num(osc0_threshold);
            print_char('\n');
            flags &= 0b11110111;
        } else if (flags & 0b01000000) {
            strcpy(print_string + TXBUF - 12, "equalizing: ");
            string_ptr = TXBUF - 12 + 1;
            UDR0 = print_string[TXBUF - 12];
            WAITTX;
            print_char('\t');
            print_num(osc0_offset);
            print_char('\t');
            print_num(osc0);
            print_char('\t');
            print_num(osc1);
            
            print_char('\n');
            flags &= 0b10111111;
        }
    }
}
