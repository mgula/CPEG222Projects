/* Marcus Gula
 * Joel Johnson
 * Project 2
 * Calculator (add and subtract only, with hexadecimal conversion too)
 * Input: 16 button keypad
 * Output: 2 SSDs, 4 on-board LEDs */

#include <p32xxxx.h>
#include <plib.h>
#include <sys/attribs.h>

// Set system frequency
#define SYS_FREQ 80000000L
// Configure bit settings
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_2

#define REFRESH_RATE 250 // ~1/100 secs
#define CLEAR 16 // All segments of SSD off - or "cleared"

// Declare on-board LEDs
#define Led1 LATBbits.LATB10
#define Led2 LATBbits.LATB11
#define Led3 LATBbits.LATB12
#define Led4 LATBbits.LATB13
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
// Declare display selection of first SSd
#define DispSel LATGbits.LATG6
// Declare segments of second SSD (jumpers JC and JD)
#define SegA2 LATGbits.LATG12
#define SegB2 LATGbits.LATG13
#define SegC2 LATGbits.LATG14
#define SegD2 LATGbits.LATG15
#define SegE2 LATDbits.LATD7
#define SegF2 LATDbits.LATD1
#define SegG2 LATDbits.LATD9
// Display selection. 0 = right digit, 1 = left digit
#define DispSel2 LATCbits.LATC1

void displayDigit(unsigned char, unsigned char); // Display one digit (right SSD)
void displayDigit2(unsigned char, unsigned char); // Display one digit (left SSD)
void make_display_value(); // Calculate the correct value to display
void Display_value(int value, int HorD); // Display the value in hex or decimal
void flicker(int one, int ten, int hundred, int thousand); // Maintain flicker
void enter_number(int value); // Enter numbers 0-9
void delete(); // Delete a digit from the display 
void add(); // Add operator
void subtract(); // Minus operator
void equal(); // Equals operator
void clear_digits(); // Set ones, tens, hundreds, thousands to CLEAR

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
    0b1110110,   //H
    0b0111000    //L
};

int i; // For-loop variable
int mode = 1;
int lastOp = 0; // Stores the last operation made: 1 = add, 2 = subtract, 3 = equals
int sum = 0; // Needed to add/subtract
int negative = 1; // Keeps track of sign: 1 = positive, -1 = negative
int hexOrDec = 0; // Keeps track of hex or decimal: 0 = dec, 1 = hex
int ones = CLEAR; // Ones place
int tens = CLEAR; // Tens place
int hund = CLEAR; // Hundreds place
int thou = CLEAR; // Thousands place
int display = 0; // Number to display on SSDs
int keyLock = 0; // Lock mechanism for keypad
int volatile readB = 0; // Prevents a flicker-y bug

/* The ISR will be called every time a change notice pin detects a change.
 * Notice that, before allowing the button press to execute code, we check
 * to make sure we aren't in mode 3 or displaying hex. */
void __ISR(_CHANGE_NOTICE_VECTOR, ipl5) ChangeNoticeHandler(void) {
    readB = PORTB; // Read PORTB to clear CN4 mismatch condition
    INTDisableInterrupts(); // Don't want any interrupts during our interrupt
    IEC1CLR = 1; // Double up on the no-interrupt policy
    if (!keyLock) {
        keyLock = 1;
        Row1 = 0; 
        Row2 = Row3 = Row4 = 1;
        if (Col1 == 0) { // (Row1, Col1) = the 1 button
            if (hexOrDec != 1 && mode != 3) {
                enter_number(1);
            }
        } else if (Col2 == 0) { // (Row1, Col2) = the 2 button, etc
            if (hexOrDec != 1 && mode != 3) {
                enter_number(2);
            }
        } else if (Col3 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(3);
            }
        } else if (Col4 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                add();
                lastOp = 1; // Set last operation to add
            }
        }
        Row2 = 0; 
        Row1 = Row3 = Row4 = 1;
        if (Col1 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(4);
            }
        } else if (Col2 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(5);
            }
        } else if (Col3 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(6);
            }
        } else if (Col4 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                subtract();
                lastOp = 2;
            }
        }
        Row3 = 0; 
        Row1 = Row2 = Row4 = 1;
        if (Col1 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(7);
            }
        } else if (Col2 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(8);
            }
        } else if (Col3 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(9);
            }
        } else if (Col4 == 0) {
            if (hexOrDec != 1) {
                mode = 1;
            }
        }
        Row4 = 0; 
        Row1 = Row2 = Row3 = 1;
        if (Col1 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                enter_number(0);
            }
        } else if (Col2 == 0) {
            if (mode != 3) {
                if (hexOrDec == 0) { // Changing to hex display
                    hexOrDec = 1;
                } else if (hexOrDec == 1) {
                    hexOrDec = 0;
                }
            }
        } else if (Col3 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                equal();
                lastOp = 3;
            }
        } else if (Col4 == 0) {
            if (hexOrDec != 1 && mode != 3) {
                delete();
            }
        }
    }
    IFS1CLR = 1; // Clear the CN interrupt status
    IEC1SET = 1; // Important to turn these back on
    Row1 = Row2 = Row3 = Row4 = 0;
    INTEnableInterrupts(); // Turn on twice just to be sure
}

main() {
    AD1PCFG = 0xFFFF; // Initialize pins as digital
    TRISB = 0x000F; // Columns are the only inputs (rows are output)
    TRISC = 0; // Everything else output
    TRISD = 0;
    TRISE = 0;
    TRISG = 0;
    readB = PORTB; // Read PORTB again to prevent mismatch
    PORTB = 0x0; // Make sure on-board LEDs are off initially
    INTEnableSystemMultiVectoredInt();
    INTDisableInterrupts();
    CNCON = 0x8000; // Enable CN module
    CNEN = 0x3C;  // Enable CN pins 2, 3, 4, 5
    CNPUE = 0x3C; // Enable pull-up for 2, 3, 4, 5
    IPC6SET = 0x00140000; // Priority levels - don't really care for now
    IPC6SET = 0x00030000;
    IFS1CLR  = 0x0001;
    IEC1SET  = 0x0001;
    INTEnableInterrupts();
    while(1) {
        /**Current*/
        if (mode == 1) { // Mode 1 - basically just reset everything to initial 
            Led1 = Led2 = Led3 = Led4 = 0;
            clear_digits();
            negative = 1;
            sum = 0;
            display = 0;
            lastOp = 0;
            hexOrDec = 0;
            Display_value(display, 0);
            mode = 2;
        } else if (mode == 2) {
            if (negative == 1) { // Check if negative and adjust accordingly
                Led4 = 0;
            } else if (negative == -1) {
                Led4 = 1;
            }
            if (hexOrDec == 1) { // Check if hex and adjust accordingly
                Led1 = Led2 = 1;
                Display_value(display, hexOrDec);
            } else {
                if (display > 9999) { // Dealing with magnitude only
                    mode = 3;
                } else if (display == 0 && !(lastOp == 1 || lastOp == 2)) {
                    negative = 1;
                }
                Led1 = Led2 = 0;
                Display_value(display, hexOrDec);
            }
        } else if (mode == 3) {
            if (negative == 1) { // Flash H
                for (i = 0;i < 20000; i++) {
                    displayDigit(0, number[17]);
                    displayDigit2(0, number[17]);
                    displayDigit(1, number[17]);
                    displayDigit2(1, number[17]);
                }
                flicker(CLEAR, CLEAR, CLEAR, CLEAR);
                for(i=0; i < 1000000; i++);
            } else if (negative == -1) { // Flash L
                for (i = 0;i < 20000; i++) {
                    displayDigit(0, number[18]);
                    displayDigit2(0, number[18]);
                    displayDigit(1, number[18]);
                    displayDigit2(1, number[18]);
                }
                flicker(CLEAR, CLEAR, CLEAR, CLEAR);
                for(i=0; i < 1000000; i++);
            }
        }
        if (keyLock && Col1 && Col2 && Col3 && Col4) {
            keyLock = 0;
        }
    }
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
/* Display digit (left SSD) */
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
/* Used by display_value to flicker the correct digits */
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
/* Display the current value either in hex or decimal */
void Display_value(int value, int HorD) {
    if (HorD == 0) { // Decimal
        if (value < 10 && value >= 0) {
            flicker(value%10, CLEAR, CLEAR, CLEAR);
        } else if (value < 100 && value >= 10) {
            flicker(value%10, (value%100)/10, CLEAR, CLEAR);
        } else if (value < 1000 && value >= 100) {
            flicker(value%10, (value%100)/10, (value%1000)/100, CLEAR);
        } else if (value < 10000 && value >= 1000) {
            flicker(value%10, (value%100)/10, (value%1000)/100, (value%10000)/1000);
        }
    } else if (HorD == 1) { // Hex
        int ones = value / 16;
        int remainder1 = value % 16;
        if (ones != 0) {
            int tens = ones / 16;
            int remainder2 = ones % 16;
            if (tens != 0) {
                int hundreds = tens / 16;
                int remainder3 = tens % 16;
                if (hundreds != 0) {
                    int remainder4 = hundreds % 16;
                    flicker(remainder1, remainder2, remainder3, remainder4);
                } else {
                    flicker(remainder1, remainder2, remainder3, CLEAR);
                }
            } else {
                flicker(remainder1, remainder2, CLEAR, CLEAR);
            }
        } else {
            flicker(remainder1, CLEAR, CLEAR, CLEAR);
        }
    }
}
/* Calculate the value to display. Essentially, we look at each digit, 
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
    make_display_value();
}
/* Sort of the opposite of enter_number. */ 
//bug!
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
    } else if (ones == CLEAR && tens == CLEAR && hund == CLEAR && thou == CLEAR && display != 0) { // May or may not need this case?
        ones = display%10; // In this case, we have pressed D immediately after E, which
        tens = (display%100)/10; // resets the digits. Need to set them to match display.
        hund = (display%1000)/100;
        thou = (display%10000)/1000;
        delete();
    }
    make_display_value();
}
/* Clear all digits. Notice that this doesn't touch display. */
void clear_digits() {
    ones = tens = hund = thou = CLEAR;
}
/* This method sets up sum for the equals operator. Also knows how to add 
 * if the last operator was add/subtract. NOTE: instead of dealing with 
 * display possibly being negative, we make it always positive and 
 * keep track of the sign with the negative variable. subtract and equal
 * use the same logic. */
void add() {
    clear_digits();
    if (lastOp == 0 || lastOp == 3) {
        sum = display;
        display = 0;
    } else if (lastOp == 1) {
        if (negative == 1) {
            display += sum;
            if (display > 9999) {
                mode = 3;
            }
            sum = display;
            display = 0;
        } else if (negative == -1) {
            display = sum - display;
            if (display < 0) {
                negative = 1;
                display *= -1; // Make it positive!
                sum = display;
                display = 0;
            } else {
                sum = display;
                display = 0;
            }
        }
    } else if (lastOp == 2) { // Subtract here
        if (negative == 1) {
            display = sum - display;
            if (display < 0) {
                negative = -1;
                display *= -1; // Make it positive!
                sum = display;
                display = 0;
            } else {
                sum = display;
                display = 0;
            }
        } else if (negative == -1) {
            display += sum;
            if (display > 9999) {
                mode = 3;
            }
            sum = display;
            display = 0;
        }
    }
}
/* This method sets up sum for the equals operator. Also knows how to subtract 
 * if the last operator was add/subtract. */
void subtract() {
    clear_digits();
    if (lastOp == 0 || lastOp == 3) {
        sum = display;
        display = 0;
    } else if (lastOp == 2) {
        if (negative == 1) {
            display = sum - display;
            if (display < 0) {
                negative = -1;
                display *= -1; // Make it positive!
                sum = display;
                display = 0;
            } else {
                sum = display;
                display = 0;
            }
        } else if (negative == -1) {
            display += sum;
            if (display > 9999) {
                mode = 3;
            }
            sum = display;
            display = 0;
        }
    } else if (lastOp == 1) { // Add here
        if (negative == 1) {
            display += sum;
            if (display > 9999) {
                mode = 3;
            }
            sum = display;
            display = 0;
        } else if (negative == -1) {
            display = sum - display;
            if (display < 0) {
                negative = 1;
                display *= -1; // Make it positive!
                sum = display;
                display = 0;
            } else {
                sum = display;
                display = 0;
            }
        }
    }
}
/* Adds or subtracts according to the last operator. Note that it does nothing
 * if the last operator was 0 or 3. */
void equal() {
    if (lastOp == 1) {
        clear_digits();
        if (negative == 1) {
            display += sum;
            sum = 0;
        } else if (negative == -1) {
            display = sum - display;
            if (display < 0) {
                negative = 1;
                display *= -1; // Make it positive!
                sum = 0;
            } else {
                sum = 0;
            }
        }
    } else if (lastOp == 2) {
        clear_digits();
        if (negative == 1) {
            display = sum - display;
            if (display < 0) {
                negative = -1;
                display *= -1; // Make it positive!
                sum = 0;
            } else {
                sum = 0;
            }
        } else if (negative == -1) {
            display += sum;
            sum = 0;
        }
    }
}
