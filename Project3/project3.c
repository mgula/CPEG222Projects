/* Marcus Gula
 * Joel Johnson
 * Project 3
 * "Gated Parking Lot" - Microphone input = car entering: generate a random ticket, if
 * they enter it correctly, add it to a pool of valid tickets. Light sensor input = car
 * exiting: if they enter a valid ticket, invalidate the ticket. That's mostly it. Throw
 * some errors if they enter something weird or run out of time.
 */

#include <stdlib.h>
#include <stdio.h>
#include <p32xxxx.h>
#include <plib.h>
#include <sys/attribs.h>
#include <time.h>

// Set system frequency
#define SYS_FREQ 80000000L
// Configure bit settings
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1

#define REFRESH_RATE 250 // ~1/100 secs
#define CLEAR 16 // All segments of SSD off - or "cleared"

// Declare mic (jumper JK)
#define PMic PORTBbits.RB11
// Declare light sensor (jumper JF)
#define PLS PORTAbits.RA14
// Declare PLEDs (jumper JH)
#define PLed1 LATDbits.LATD13
#define PLed2 LATDbits.LATD8
#define PLed3 LATDbits.LATD0
#define PLed4 LATEbits.LATE8
#define PLed5 LATFbits.LATF13
#define PLed6 LATFbits.LATF4
#define PLed7 LATFbits.LATF5
#define PLed8 LATFbits.LATF12
// Declare keys (jumper JJ)
#define Col1 PORTBbits.RB3 // Columns are the CN pins, need to be inputs
#define Col2 PORTBbits.RB2
#define Col3 PORTBbits.RB1
#define Col4 PORTBbits.RB0
#define Row1 LATBbits.LATB9 // Will write to rows, need to be outputs
#define Row2 LATBbits.LATB8
#define Row3 LATBbits.LATB5
#define Row4 LATBbits.LATB4
// Declare segments of first SSD (jumpers JA and JB)
#define SegA LATEbits.LATE0
#define SegB LATEbits.LATE1
#define SegC LATEbits.LATE2
#define SegD LATEbits.LATE3
#define SegE LATGbits.LATG9
#define SegF LATGbits.LATG8
#define SegG LATGbits.LATG7
// Declare display selection of first SSD
#define DispSel LATGbits.LATG6
// Declare segments of second SSD (jumpers JC and JD)
#define SegA2 LATGbits.LATG12
#define SegB2 LATGbits.LATG13
#define SegC2 LATGbits.LATG14
#define SegD2 LATGbits.LATG15
#define SegE2 LATDbits.LATD7
#define SegF2 LATDbits.LATD1
#define SegG2 LATDbits.LATD9
// Declare display selection of second SSD
#define DispSel2 LATCbits.LATC1

void displayDigit(unsigned char, unsigned char); // Display one digit (right SSD)
void displayDigit2(unsigned char, unsigned char); // Display one digit (left SSD)
void make_ticket_value(); // Calculate the current ticket value
void make_display_value(); // Calculate the current value from ones, tens, hund, thou
void flicker(int one, int ten, int hundred, int thousand); // Maintain flicker
void enter_number(int value); // Enter numbers 0-9
void enter_cap(int value); // enter_number() for mode 1
void delete(); // Delete a digit from the display 
void delete_cap(); // Essentially delete() for mode 1
void clear_digits(); // Set ones, tens, hundreds, thousands to CLEAR
void generate_ticket(); // Generate a random ticket
int validate_ticket(); // Validate a ticket
int decrement_pleds(); // Turn off one PLED at a time
void display_error(); // Display the current error 
void enter_pass(); // Enter a valid ticket (mode 4)
void ticket_entry(); // Enter a valid ticket (mode 3)

int ticket[4]; // 4 digit ticket
int tickets[99]; // Array (as large as the max capacity) for storing valid tickets

unsigned char number[] = { // Look up table for displaying numbers
    0b0111111,   //0
    0b0000110,   //1
    0b1011011,   //2
    0b1001111,   //3
    0b1100110,   //4
    0b1101101,   //5
    0b1111101,   //6
    0b0000111,   //7
    0b1111111,   //8
    0b1101111,   //9
    0b1110111,   //A
    0b1111100,   //b
    0b0111001,   //C
    0b1011110,   //d
    0b1111001,   //E
    0b1110001,   //F
    0b0000000,   //clear
};

int i = 0;
int j = 0;
int k = 0;
int num_tickets = 0;
int ticket_value = 0;
int cap = 0; // Capacity
int open = 0; // Open spots
int disp = 0; // Used in mode 5 to flash PLEDs
int found = 0; // Find a ticket
int mic = 0; // Microphone input
int mode = 1; // Start in mode 1
int ones = CLEAR; // Ones place
int tens = CLEAR; // Tens place
int hund = CLEAR; // Hundreds place
int thou = CLEAR; // Thousands place
int display = 0;
int keyLock = 0; // Lock mechanism for keypad
int modeLock = 0; // Lock mechanism for modes
int volatile readB = 0; // Prevents a flicker-y bug
int error = 0;
int error_count = 0;
int valid = 0;
/* 8 second timer for mode 3 and 4, 5 second timer for mode 5. */
void __ISR(_TIMER_5_VECTOR, ipl1) Timer1Handler(void) {
    if (mode == 3 || mode == 4) {
        k = decrement_pleds();
        if (k == 0) {
            modeLock = 0;
            error = 1;
            mode = 5;
            T4CONCLR = 0x8000; // stop timer
            TMR4 = 0; // reset timer contents
        }
    } else if (mode == 5) {
        error_count++; // Count to 5 at a speed of 1 Hz
        disp = !disp; // Flash PLEDs
        if (error_count == 5) { 
            modeLock = 0;
            mode = 2;
            T4CONCLR = 0x8000;
            TMR4 = 0;
        }
    }
    IFS0CLR = 0x00100000; // Clear the Timer5 interrupt status flag
}
/* The ISR will be called every time a change notice pin detects a change. */
void __ISR(_CHANGE_NOTICE_VECTOR, ipl5) ChangeNoticeHandler(void) {
    readB = PORTB; // Read PORTB to clear CN mismatch condition
    IEC1CLR = 1; // Disable CN interrupts
    if (!keyLock) {
        keyLock = 1;
        Row1 = 0; 
        Row2 = Row3 = Row4 = 1;
        if (Col1 == 0) { // (Row1, Col1) = the 1 button
            if (mode == 1) {
                enter_cap(1);
            } else {
                enter_number(1);
            }
        } else if (Col2 == 0) { // (Row1, Col2) = the 2 button, etc
            if (mode == 1) {
                enter_cap(2);
            } else {
                enter_number(2);
            }
        } else if (Col3 == 0) {
            if (mode == 1) {
                enter_cap(3);
            } else {
                enter_number(3);
            }
        } else if (Col4 == 0) {
            // A button - functionality not defined
        }
        Row2 = 0; 
        Row1 = Row3 = Row4 = 1;
        if (Col1 == 0) {
            if (mode == 1) {
                enter_cap(4);
            } else {
                enter_number(4);
            }
        } else if (Col2 == 0) {
            if (mode == 1) {
                enter_cap(5);
            } else {
                enter_number(5);
            }
        } else if (Col3 == 0) {
            if (mode == 1) {
                enter_cap(6);
            } else {
                enter_number(6);
            }
        } else if (Col4 == 0) {
            // B button - functionality not defined
        }
        Row3 = 0; 
        Row1 = Row2 = Row4 = 1;
        if (Col1 == 0) {
            if (mode == 1) {
                enter_cap(7);
            } else {
                enter_number(7);
            }
        } else if (Col2 == 0) {
            if (mode == 1) {
                enter_cap(8);
            } else {
                enter_number(8);
            }
        } else if (Col3 == 0) {
            if (mode == 1) {
                enter_cap(9);
            } else {
                enter_number(9);
            }
        } else if (Col4 == 0) {
            clear_digits();
            display = 0;
            if (mode == 1) {
                cap = 0;
            }
        }
        Row4 = 0; 
        Row1 = Row2 = Row3 = 1;
        if (Col1 == 0) {
            if (mode == 1) {
                enter_cap(0);
            } else {
                enter_number(0);
            }
        } else if (Col2 == 0) {
            // F button - functionality not defined
        } else if (Col3 == 0) {
            if (mode == 1) {
                open = cap;
                mode = 2;
            } else if (mode == 3) {
                ticket_entry();
            } else if (mode == 4) {
                enter_pass();
            }
        } else if (Col4 == 0) {
            if (mode == 1) {
                delete_cap();
            } else {
                delete();
            }
        }
    }
    IFS1CLR = 1; // Clear the CN interrupt status
    IEC1SET = 1; // Important to turn these back on
    Row1 = Row2 = Row3 = Row4 = 0;
}

main() {
    // Microphone
    AD1PCFG = 0xF7FF; // all pins digital, except for the mic pin
    AD1CON1 = 0x00E0;
    AD1CHS=0x000B0000;
    AD1CSSL = 0;
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;
    AD1CON1bits.ADON = 1;
    // Ports
    TRISA = 0x4000; // Light sensor is input
    TRISB = 0x000F; // Columns are inputs (rows are output)
    TRISC = 0; // Everything else output
    TRISD = 0;
    TRISE = 0;
    TRISG = 0;
    TRISF = 0;
    readB = PORTB; // Read PORTB again to prevent mismatch
    PORTB = 0;
    INTEnableSystemMultiVectoredInt();
    INTDisableInterrupts();
    // Change Notice interrupts
    CNCON = 0x8000; // Enable CN module
    CNEN = 0x3C;  // Enable CN pins 2, 3, 4, 5
    CNPUE = 0x3C; // Enable pull-up for 2, 3, 4, 5
    IPC6SET = 0x00140000;
    IPC6SET = 0x00030000;
    IFS1CLR = 1;
    IEC1SET = 1;
    // Timer interrupts
    T4CON = 0x0; // Stop 16-bit Timer4 and clear control register
    T5CON = 0x0; // Stop 16-bit Timer5 and clear control register
    T4CONSET = 0x0038; // Enable 32-bit mode, prescaler at 1:8
    TMR4 = 0x0; // Clear contents of the TMR4 and TMR5
    PR4 = 0x00989680; // Load PR4 and PR5 registers with 32-bit value - 1 Hz currently
    IPC5SET = 0x00000004; // Set priority level=1 and
    IPC5SET = 0x00000001; // Set sub-priority level=1
    IFS0CLR = 0x00100000; // Clear the Timer5 interrupt status flag
    IEC0SET = 0x00100000; // Enable Timer5 interrupts
    INTEnableInterrupts();
    srand(time(NULL)); // rand() seed
    while(1) {
        if (mode == 1) {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
            if (cap < 10) {
                flicker(cap, CLEAR, CLEAR, 12);
            } else {
                flicker(cap%10, (cap%100)/10, CLEAR, 12);
            }
        } else if (mode == 2) {
            disp = 0;
            error_count = 0;
            modeLock = 0;
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
            mic = readADC(0); // always listening for horns
            if (mic > 31) {
                if (open == 0) { // throw error 3 if lot is full
                    error = 3;
                    mode = 5;
                    T4CONCLR = 0x8000;
                    TMR4 = 0;
                } else {
                    mode = 3;
                }
            }
            if (PLS == 1) { // always watching for headlights
                if (open == cap) { //throw error 4 if lot is empty
                    error = 4;
                    mode = 5;
                    T4CONCLR = 0x8000;
                    TMR4 = 0;
                } else {
                    mode = 4;
                }
            }
            if (open < 10) {
                flicker(open%10, CLEAR, CLEAR, 15);
            } else {
                flicker(open%10, (open%100)/10, CLEAR, 15);
            }
        } else if (mode == 3) {
            if (!modeLock) {  // only want to do these things once per entry into
                modeLock = 1; //this mode
                PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
                T4CONSET = 0x8000; //start timer
                j = 0;
                while (!j) {
                    generate_ticket();
                    j = validate_ticket();
                }
            }
            flicker(ticket[3], ticket[2], ticket[1], ticket[0]);
        } else if (mode == 4) {
            if (!modeLock) {
                PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
                modeLock = 1;
                clear_digits();
                display = 0;
                T4CONSET = 0x8000;
            }
            if (ones == CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
                flicker(0, CLEAR, CLEAR, CLEAR);
            } else {
                flicker(ones, tens, hund, thou);
            }
        } else if (mode == 5) {
            if (!modeLock) {
                modeLock = 1;
                T4CONSET = 0x8000;
            }
            display_error();
        }
        if (keyLock && Col1 && Col2 && Col3 && Col4) {
            keyLock = 0;
        }
    }
}
/* Compare the user's input to every valid ticket. If they enter a valid
 * ticket, invalidate it. Throw error 2 otherwise. */
void enter_pass() {
    display = ones + (10*tens) + (100*hund) + (1000*thou);
    i = 0;
    valid = 0;
    while (i < cap) {
        if (display == tickets[i]) {
            found = i;
            valid = 1;
        }
        i++;
    }
    if (valid) {
        num_tickets--;
        tickets[found] = -1; // invalidate the ticket
        open++;
        mode = 2;
        T4CONCLR = 0x8000;
        TMR4 = 0;
    } else {
        error = 2;
        modeLock = 0;
        mode = 5;
        T4CONCLR = 0x8000;
        TMR4 = 0;
    }
}
/* Display any of the 4 possible errors. */
void display_error() {
    if (error == 1) {
        flicker(1, 1, 1, 14);
        if (disp) {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
        } else {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
        }
    } else if (error == 2) {
        flicker(2, 2, 2, 14);
        if (disp) {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
        } else {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
        }
    } else if (error == 3) {
        flicker(3, 3, 3, 14);
        if (disp) {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
        } else {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
        }
    }  else if (error == 4) {
        flicker(4, 4, 4, 14);
        if (disp) {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 0;
        } else {
            PLed1 = PLed2 = PLed3 = PLed4 = PLed5 = PLed6 = PLed7 = PLed8 = 1;
        }
    }
}
/* Create a ticket by generating 4 random numbers between 0 and 9. */
void generate_ticket() {
    for (i=0; i < 4; i++) {
        ticket[i] = rand() % 10;
    }
}
/* Compare the current ticket with all other existing tickets. Return 0 if
 * the ticket matches any other ticket. */
int validate_ticket() {
    make_ticket_value();
    i = 0;
    while (i < num_tickets) {
        if (ticket_value == tickets[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}
/* Compare the user's input to the current ticket. Add it to tickets[]
 * if they enter it correctly. Throw error 2 otherwise. */
void ticket_entry() {
    make_display_value();
    make_ticket_value();
    if (display == ticket_value) {
        mode = 2;
        num_tickets++;
        open--;
        tickets[num_tickets - 1] = ticket_value;
        T4CONCLR = 0x8000;
        TMR4 = 0;
    } else {
        error = 2;
        modeLock = 0;
        mode = 5;
        T4CONCLR = 0x8000;
        TMR4 = 0;
    }
}
/* Turn off the PLEDs one at a time. Return 0 if they're all off. */
int decrement_pleds() {
    if (PLed1 && PLed2 && PLed3 && PLed4 && PLed5 && PLed6 && PLed7 && PLed8) {
        PLed1 = 0;
        return 1;
    } else if (!PLed1 && PLed2 && PLed3 && PLed4 && PLed5 && PLed6 && PLed7 && PLed8) {
        PLed2 = 0;
        return 1;
    } else if (!PLed1 && !PLed2 && PLed3 && PLed4 && PLed5 && PLed6 && PLed7 && PLed8) {
        PLed3 = 0;
        return 1;
    } else if (!PLed1 && !PLed2 && !PLed3 && PLed4 && PLed5 && PLed6 && PLed7 && PLed8) {
        PLed4 = 0;
        return 1;
    } else if (!PLed1 && !PLed2 && !PLed3 && !PLed4 && PLed5 && PLed6 && PLed7 && PLed8) {
        PLed5 = 0;
        return 1;
    } else if (!PLed1 && !PLed2 && !PLed3 &&!PLed4 && !PLed5 && PLed6 && PLed7 && PLed8) {
        PLed6 = 0;
        return 1;
    } else if (!PLed1 && !PLed2 && !PLed3 && !PLed4 && !PLed5 && !PLed6 && PLed7 && PLed8) {
        PLed7 = 0;
        return 1;
    }  else if (!PLed1 && !PLed2 && !PLed3 && !PLed4 && !PLed5 && !PLed6 && !PLed7 && PLed8) {
        PLed8 = 0;
        return 0;
    }
}
/* Read and return the input into the microphone. */
int readADC(int ch) {
    AD1CON1bits.SAMP = 1;
    while (!AD1CON1bits.DONE);
    return ADC1BUF0;
}
/* Display digit (right SSD) */
void displayDigit (unsigned char display_digit, unsigned char value){
    DispSel = display_digit;
    SegA    = value & 1;
    SegB    = (value >> 1) & 1;
    SegC    = (value >> 2) & 1;
    SegD    = (value >> 3) & 1;
    SegE    = (value >> 4) & 1;
    SegF    = (value >> 5) & 1;
    SegG    = (value >> 6) & 1;
}
/* Display digit (left SSD). */
void displayDigit2 (unsigned char display_digit, unsigned char value){
    DispSel2 = display_digit;
    SegA2    = value & 1;
    SegB2    = (value >> 1) & 1;
    SegC2    = (value >> 2) & 1;
    SegD2    = (value >> 3) & 1;
    SegE2    = (value >> 4) & 1;
    SegF2    = (value >> 5) & 1;
    SegG2    = (value >> 6) & 1;
}
/* Maintain the flickering of the SSDs. */
void flicker(int one, int ten, int hundred, int thousand){
    for (i = 0;i < REFRESH_RATE; i++) {
        displayDigit(0, number[one]);
        displayDigit2(0, number[hundred]);
    }
    for (i = 0;i < REFRESH_RATE; i++) {
        displayDigit(1, number[ten]);
        displayDigit2(1, number[thousand]);
    }
}
/* Set the display variable. Essentially, we look at each digit, 
 * ignore it if it's CLEAR, and add it otherwise. */
void make_display_value() {
    if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou != CLEAR) {
        display = ones + (10*tens) + (100*hund) + (1000*thou);
    } else if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou == CLEAR) {
        display = ones + (10*tens) + (100*hund);
    } else if (ones != CLEAR && tens != CLEAR && hund == CLEAR && thou == CLEAR) {
        display = ones + (10*tens);
    } else if (ones != CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        display = ones;
    } else if (ones == CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        display = 0;
    }
}
/* Make the ticket value according to the current digits in ticket[]. */
void make_ticket_value() {
    ticket_value = 0;
    for (i=0; i < 4; i++) {
        if (i == 3) {
            ticket_value += ticket[i];
        } else if (i == 2) {
            ticket_value += 10*ticket[i];
        }  else if (i == 1) {
            ticket_value += 100*ticket[i];
        }  else if (i == 0) {
            ticket_value += 1000*ticket[i];
        }
    }
}
/* Set our ones, tens, hundreds and thousands places according to which are
 * CLEAR. */
void enter_number(int value) {
    if (ones == CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        ones = value;
    } else if (ones != CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        tens = ones;
        ones = value;
    } else if (ones != CLEAR && tens != CLEAR && hund == CLEAR && thou == CLEAR) {
        hund = tens;
        tens = ones;
        ones = value;
    } else if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou == CLEAR) {
        thou = hund;
        hund = tens;
        tens = ones;
        ones = value;
    } else if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou != CLEAR) {
        thou = hund;
        hund = tens;
        tens = ones;
        ones = value;
    }
}
/* Same as above, except we don't care about the hundreds or thousands place now. */
void enter_cap(int value) {
    if (ones == CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        ones = value;
    } else if (ones != CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        tens = ones;
        ones = value;
    }  else if (ones != CLEAR && tens != CLEAR && hund == CLEAR && thou == CLEAR) {
        tens = ones;
        ones = value;
    } 
    make_display_value();
    cap = display;
}
/* Set the most significant (active) digit to CLEAR. */ 
void delete() {
    if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou != CLEAR) {
        ones = tens;
        tens = hund;
        hund = thou;
        thou = CLEAR;
    } else if (ones != CLEAR && tens != CLEAR && hund != CLEAR && thou == CLEAR) {
        ones = tens;
        tens = hund;
        hund = CLEAR;
    } else if (ones != CLEAR && tens != CLEAR && hund == CLEAR && thou == CLEAR) {
        ones = tens;
        tens = CLEAR;
    } else if (ones != CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR) {
        ones = CLEAR;
    }
}
/* Same as above, except we don't care about the hundreds or thousands place now. */
void delete_cap() {
    if (ones != CLEAR && tens != CLEAR) {
        ones = tens;
        tens = CLEAR;
    } else if (ones != CLEAR && tens == CLEAR) {
        ones = CLEAR;
    } 
    make_display_value();
    cap = display;
}
/* Set all digits to clear. */
void clear_digits() {
    ones = tens = hund = thou = CLEAR;
}
