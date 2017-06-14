// CPEG222
// Marcus Gula and Joel Johnson
// Project 1
// General: implement a calendar that includes all valid dates of 2016
// Input: Two on-board buttons: Btn1, Btn2
// Output: Four on-board LEDs: Led1-Led4
//         PMod8LD: PLed1-PLed7
//         Two SSDs: Seg A-G x2, display select x2

#include <p32xxxx.h>

// Set system frequency
#define SYS_FREQ     80000000L
// Configure bit settings
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_2
// Set refresh rate of the segments
#define REFRESH_RATE 250 // ~1/100 secs
// Declare buttons
#define Btn1 PORTAbits.RA6
#define Btn2 PORTAbits.RA7
// Declare on-board LEDs
#define Led1 LATBbits.LATB10
#define Led2 LATBbits.LATB11
#define Led3 LATBbits.LATB12
#define Led4 LATBbits.LATB13
// Declare PLEDs (jumper JH)
#define PLed1 LATDbits.LATD13
#define PLed2 LATDbits.LATD8
#define PLed3 LATDbits.LATD0
#define PLed4 LATEbits.LATE8
#define PLed5 LATFbits.LATF13
#define PLed6 LATFbits.LATF4
#define PLed7 LATFbits.LATF5
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
// Set enum for SSD modes: left and right
enum Mode {Left,Right};
void displayDigit(unsigned char, unsigned char); // Display one digit
void flicker(); // Flicker between digits on both SSDs
int Compute_weekday(int month, int day); // Compute the weekday
void Increment_date(); // Increase day by 1
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
    0b0000000   //clear
};
unsigned int daysInMonth[] = { // The maximum number of days in each month
    31, // Jan
    29, // Feb
    31, // Mar
    30, // Apr
    31, // May
    30, // Jun
    31, // Jul
    31, // Aug
    30, // Sep
    31, // Oct
    30, // Nov
    31  // Dec
};
int i = 0; // Will use to run through loops
int weekday = 0; // Day of the week (used for PMOD display)
int slowdown = 0; // Used to slow down when incrementing date
int pause = 0; // Used to pause in mode 4
int ButtonlockA = 0; // Button Lock for Btn1
int ButtonlockB = 0; // Button Lock for Btn2
enum Mode mode=Left; // Initial mode is left
int modes = 1; // mode was taken ;(
int month = 1;
int day = 1;

main(){
    // Set PORTA as input - two buttons
    TRISA = 0xc0; 
    // Set all of port B as digital
    AD1PCFG = 0xFFFF;
    // Set all other ports as output (PMODs, SSDs)
    TRISB = 0;
    TRISC = 0;
    TRISD = 0;
    TRISE = 0;
    TRISF = 0;
    TRISG = 0;
    // Initialize ports
    PORTB = 0x0400; // Start with Led1 on
    PORTD = 0; // Start with all others off
    PORTE = 0;
    PORTF = 0;
    PORTG = 0;
    
    while(1){
        if (modes == 1) {
            pause = 0;
            slowdown = 0;
            month = 1; // Month and day should always be 1 in this mode
            day = 1;
            PLed1 = 0;
            PLed2 = 0;
            PLed3 = 0;
            PLed4 = 0;
            PLed5 = 0;
            PLed6 = 0;
            PLed7 = 0;
            Led2 = 0;
            Led3 = 0;
            Led4 = 0;
            Led1 = 1; // Only Led1 on
            flicker(); // Maintain flickering
        } 
        else if (modes == 2) {
            pause = 0;
            PLed1 = 0;
            PLed2 = 0;
            PLed3 = 0;
            PLed4 = 0;
            PLed5 = 0;
            PLed6 = 0;
            PLed7 = 0;
            Led1 = 0;
            Led4 = 0;
            Led2 = 1; // Only Led2 on
            flicker(); // Maintain flickering
        } 
        else if (modes == 3) {
            pause = 0;
            Led2 = 0;
            Led3 = 1; // Only Led3 on
            if (day > daysInMonth[month - 1]) {
                day = 1; // This prevents bad dates, like Feb. 31st
            }
            flicker(); // Maintain flickering
        } 
        else if (modes == 4) {
            if (pause) {
                flicker(); // If paused, only flicker
            } else {
                if (PLed1 == 0 && PLed2 == 0 && PLed3 == 0 && PLed4 == 0 && PLed5 == 0 && PLed6 == 0 && PLed7 == 0) {
                    weekday = Compute_weekday(month, day); // If all PLeds are off, quickly turn the correct one on
                    switch (weekday) {
                        case 1:
                            PLed2 = 0;
                            PLed3 = 0;
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            break;
                        case 2:
                            PLed3 = 0;
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            break;
                        case 3:
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            break;
                        case 4:
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            break;
                        case 5:
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            break;
                        case 6:
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            PLed6 = 1;
                            break;
                        case 7:
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            PLed6 = 1;
                            PLed7 = 1;
                            break;
                    }
                }
                slowdown++;
                Led1 = 0;
                Led3 = 0;
                Led4 = 1; // Only Led4 on
                flicker(); // Maintain flickering
                if (slowdown%125 == 0) {
                    slowdown = 0;
                    if (month == 12 && day == 31) {
                        PLed6 = 0; // Jumping from a Sat to a Fri here - turn Led6 off
                    }
                    Increment_date();
                    weekday = Compute_weekday(month, day);
                    switch (weekday) {
                        case 1:
                            PLed2 = 0;
                            PLed3 = 0;
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            break;
                        case 2:
                            PLed3 = 0;
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            break;
                        case 3:
                            PLed4 = 0;
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            break;
                        case 4:
                            PLed5 = 0;
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            break;
                        case 5:
                            PLed6 = 0;
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            break;
                        case 6:
                            PLed7 = 0;
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            PLed6 = 1;
                            break;
                        case 7:
                            PLed1 = 1;
                            PLed2 = 1;
                            PLed3 = 1;
                            PLed4 = 1;
                            PLed5 = 1;
                            PLed6 = 1;
                            PLed7 = 1;
                            break;
                    }
                }
            }
        }
        // Next state logic
        if (Btn1 && Btn2) { // Force reset - can happen in any mode
            ButtonlockA = 1;
            ButtonlockB = 1;
            flicker(); // Use flicker to debounce and maintain SSDs
            modes = 1; // Jump back to mode 1
        } 
        else if (Btn2 && !ButtonlockB && modes == 1) { // Button 2 while in mode 1
            ButtonlockB = 1;
            flicker(); // Use flicker to debounce and maintain SSDs
            modes = 4; // Jump to mode 4
        } 
        else if (Btn1 && !ButtonlockA) { // Button 1 in any mode
            slowdown = 0;
            ButtonlockA = 1;  
            flicker(); // Use flicker to debounce and maintain SSDs
            if (modes == 1) {
                modes = 2;
            } else if (modes == 2) {
                modes = 3;
            } else if (modes == 3) {
                modes = 4;
            } else if (modes == 4) {
                modes = 2;
            }
        } 
        else if (Btn2 && !ButtonlockB && modes == 2) { // Button 2 while in mode 2
            ButtonlockB = 1;
            if (month == 12) { // Increase month
                month = 1;
            } else {
                month++;
            }
        } 
        else if (Btn2 && !ButtonlockB && modes == 3) { // Button 2 while in mode 3
            ButtonlockB = 1;
            if (day == daysInMonth[month - 1]) { // Increase day
                day = 1;
            } else {
                day++;
            }
        } 
        else if (Btn2 && !ButtonlockB && modes == 4) { // Button 2 while in mode 4
	     ButtonlockB = 1;
            if (pause == 0) { // Initiates pause
                pause = 1;
            } else {
                pause = 0;
            }
        }
        else if (!Btn1 && ButtonlockA) { // Reset button lock A
            ButtonlockA = 0;
        } 
        else if (!Btn2 && ButtonlockB) { // Reset button lock B
            ButtonlockB = 0;
        }
    }
}
// Display digit - SSD 1 (right SSD)
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
// Display digit - SSD 2 (left SSD)
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
// This method is used both to flicker between digits on each SSD and debounce buttons
void flicker(){
    for (i = 0;i < REFRESH_RATE; i++) {
        displayDigit(mode == Right, number[day%10]);
        displayDigit2(mode == Right, number[month%10]);
    }
    for (i = 0;i < REFRESH_RATE; i++) {
        displayDigit(mode == Left, number[day/10]);
        displayDigit2(mode == Left, number[month/10]);
    }
}
/* 1 = Sunday, 2 = Monday, 3 = Tuesday, 4 = Wednesday, 5 = Thursday, 6 = Friday, 7 = Saturday
 * this method assumes you are passing a valid day of the month - day 
 * and month checks are also made on button press, however*/
int Compute_weekday(int month, int day) {
    int total = 5; // Jan 1st is a Friday, 5th day of the week.
    total--; // subtract 1 to be consistent with above scheme
    switch (month) {
        case 1:
            total += day;
            break;
        case 2:
            total += 31 + day;
            break;
        case 3:
            total += 60 + day;
            break;
        case 4:
            total += 91 + day;
            break;
        case 5:
            total += 121 + day;
            break;
        case 6:
            total += 152 + day;
            break;
        case 7:
            total += 182 + day;
            break;
        case 8:
            total += 213 + day;
            break;
        case 9:
            total += 244 + day;
            break;
        case 10:
            total += 274 + day;
            break;
        case 11:
            total += 305 + day;
            break;
        case 12:
            total += 335 + day;
            break;
        default:
            total = -1; // A month other than 1-12 was passed. make that an error case
    }
    total %= 7;
    if (total == 0) {
    	total = 7; // 7 % 7 = 0. Make this a 7 for simplicity's sake
    }
    return total;
}
/* Add one to the date. If it's the end of the month, advance to the next month.
 * If it's December, go back to January.*/
void Increment_date() {
    if (day == daysInMonth[month - 1]) {
        day = 1;
        if (month == 12) {
            month = 1;
        } else {
            month++;
        }
    } else {
        day++;
    }
}
