/* Marcus Gula
 * Joel Johnson
 * Project 4
 * "Line Following Robot" - follow a line using 2 servos and 4 light sensors.
 */

#include <p32xxxx.h>
#include <sys/attribs.h>

// Set system frequency
#define SYS_FREQ 80000000L
// Configure bit settings
#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_1, FWDTEN = OFF
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1
// Set variables
#define REFRESH_RATE 250 // ~1/100 secs
#define CLEAR 10 // All segments of SSD off - or "cleared"
#define PWM_FREQ 0x186A
// Declare mic (jumper JJ)
#define PMic PORTBbits.RB1
// Declare on-board LEDs
#define Led1 LATBbits.LATB10
#define Led2 LATBbits.LATB11
#define Led3 LATBbits.LATB12
#define Led4 LATBbits.LATB13
// Declare light sensors - if robot is facing you (you are closest to the sensors):
// Pls1 = far left, Pls2 = center left, Pls3 = center right, Pls4 = far right (jumper JF)
#define Pls1 PORTDbits.RD14
#define Pls2 PORTFbits.RF8
#define Pls3 PORTFbits.RF2
#define Pls4 PORTDbits.RD15
// Declare on-board button (Btn2)
#define Btn PORTAbits.RA7
#define Btn2 PORTAbits.RA6
// Declare segments of SSD (jumpers JA and JB)
#define SegA LATEbits.LATE0
#define SegB LATEbits.LATE1
#define SegC LATEbits.LATE2
#define SegD LATEbits.LATE3
#define SegE LATGbits.LATG9
#define SegF LATGbits.LATG8
#define SegG LATGbits.LATG7
// Declare display selection of first SSD (jumper JB)
#define DispSel LATGbits.LATG6

void displayDigit(unsigned char, unsigned char); // Display one digit
void flicker(int one, int ten); // Maintain flicker
void updateLedsAndSensor(); 
void straight();
void hardLeft();
void hardRight();
void slightLeft();
void slightRight();
void reverse();
void modeFour();

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
    0b0000000,   //clear
    0b0111001,   //C
    0b0111000,   //L
};

int i = 0; // used in for-loops
int mode = 1; // mode control
int mic = 0; // current mic value
int sensor = -1; // initialize to something other than 0, since 0000 is valid
int newSensor = -2; // same as above, except also initialize to something other than sensor
int buttonLock = 0; // lock variable for button
int micLock = 0; // lock variable for mic (don't want to pick up one noise twice)
int sensorLock = 0; // lock for sensors
int display = 0; // value to display on SSDs
int flag = 0; // used in mode 3
int offTrack = 0; // used with flag
int timer = 0; // current timer value
int x = 0;

void __ISR(_TIMER_5_VECTOR, ipl1) Timer5Handler(void) {
    timer++; // increment timer
    TMR4 = 0;
    IFS0CLR = 0x00100000; // Clear the Timer5 interrupt status flag
}

int main() {
    // Mic declarations
    AD1PCFG = 0xFFFD; // All digital except for mic pin
    AD1CON1 = 0x00E0;
    AD1CHS=0x00010000;
    AD1CSSL = 0;
    AD1CON2 = 0;
    AD1CON3 = 0x1F3F;
    AD1CON1bits.ADON = 1;
    // Servo declarations (right)
    OC2CON = 0x0000;
    OC2R = 0x0060;
    OC2RS = PWM_FREQ*0.075; // Start at the "off" duty cycle
    T1CONSET = 0x0070;
    OC2CON = 0x0006;
    PR1 = PWM_FREQ;
    // Servo declarations (left)
    OC3CON = 0x0000;
    OC3R = 0x0060;
    OC3RS = PWM_FREQ*0.075;
    T2CONSET = 0x0070;
    OC3CON = 0x0006;
    PR2 = PWM_FREQ;
    // Tristate 
    TRISA = 0x00C0; //b3
    TRISB = 0x0002;
    TRISE = 0;
    TRISG = 0;
    TRISD = 0xC000;
    TRISF = 0x0104;
    // Interrupts
    INTEnableSystemMultiVectoredInt();
    INTDisableInterrupts();
    T4CON = 0; // Stop 16-bit Timer4 and clear control register
    T5CON = 0; // Stop 16-bit Timer5 and clear control register
    T4CONSET = 0x0038; // Enable 32-bit mode, prescaler at 1:8
    TMR4 = 0; // Clear contents of the TMR4 and TMR5
    PR4 = 0x00989680; // Load PR4 and PR5 registers with 32-bit value - 1 Hz currently
    IPC5SET = 0x00000004; // Set priority level=1 and
    IPC5SET = 0x00000001; // Set sub-priority level=1
    IFS0CLR = 0x00100000; // Clear the Timer5 interrupt status flag
    IEC0SET = 0x00100000; // Enable Timer5 interrupts
    INTEnableInterrupts();
    if (Btn2) {
        mode = 4;
    }
    while(1) {
        if (mode == 1) {
            updateLedsAndSensor();
            flag = 0;
            if (Btn && !buttonLock) { // Start all timers
                buttonLock = 1;
                mode = 2;
                T1CONSET = 0x8000; 
                OC2CONSET = 0x8000;
                T2CONSET = 0x8000; 
                OC3CONSET = 0x8000;
                T4CONSET = 0x8000;
            }
            mic = readADC(0);
            if (mic > 550 && !micLock) {
                micLock = 1;
                mode = 2;
                T1CONSET = 0x8000; 
                OC2CONSET = 0x8000;
                T2CONSET = 0x8000; 
                OC3CONSET = 0x8000;
                T4CONSET = 0x8000;
            }
            flicker (display%10, display/10);
        } else if (mode == 2) {
            flag = 0;
            flicker (timer%10, timer/10);
            if (Btn && !buttonLock) { // Failsafe to exit mode 2
                buttonLock = 1;
                display = timer;
                timer = 0;
                mode = 1;
                T1CONCLR = 0x8000; 
                OC2CONCLR = 0x8000;
                T2CONCLR = 0x8000; 
                OC3CONCLR = 0x8000;
                T4CONCLR = 0x8000;
            }
            if (!sensorLock) {
                sensorLock = 1;
                if (Pls1 && !Pls2 && !Pls3 && !Pls4) {
                    newSensor = 1000;
                    hardLeft();
                } else if (!Pls1 && Pls2 && !Pls3 && !Pls4) {
                    newSensor = 0100;
                    // Continue last movement
                } else if (!Pls1 && !Pls2 && Pls3 && !Pls4) {
                    newSensor = 0010;
                    // Continue last movement
                } else if (!Pls1 && !Pls2 && !Pls3 && Pls4) {
                    newSensor = 0001;
                    hardRight();
                } else if (Pls1 && Pls2 && !Pls3 && !Pls4) {
                    newSensor = 1100;
                    slightLeft();
                } else if (Pls1 && !Pls2 && Pls3 && !Pls4) {
                    newSensor = 1010;
                    // Continue last movement
                } else if (Pls1 && !Pls2 && !Pls3 && Pls4) {
                    newSensor = 1001;
                    straight();
                } else if (!Pls1 && Pls2 && Pls3 && !Pls4) {
                    newSensor = 0110;
                    straight();
                } else if (!Pls1 && Pls2 && !Pls3 && Pls4) {
                    newSensor = 0101;
                    // Continue last movement
                } else if (!Pls1 && !Pls2 && Pls3 && Pls4) {
                    newSensor = 0011;
                    slightRight();
                } else if (Pls1 && Pls2 && Pls3 && !Pls4) {
                    newSensor = 1110;
                    hardLeft();
                } else if (Pls1 && Pls2 && !Pls3 && Pls4) {
                    newSensor = 1101;
                    straight();
                } else if (Pls1 && !Pls2 && Pls3 && Pls4) {
                    newSensor = 1011;
                    straight();
                } else if (!Pls1 && Pls2 && Pls3 && Pls4) {
                    newSensor = 0111;
                    hardRight();
                } else if (Pls1 && Pls2 && Pls3 && Pls4) {
                    newSensor = 1111;
                    mode = 3; // If all are on, we have somehow left the track
                } else if (!Pls1 && !Pls2 && !Pls3 && !Pls4) {
                    newSensor = 0000;
                    if (timer > 58) { // The "Hail Mary" - stop approximately near
                        mode = 1;     // the end of the track ... assumes a lot
                        display = timer;
                        timer = 0;
                        T1CONCLR = 0x8000;
                        OC2CONCLR = 0x8000;
                        T2CONCLR = 0x8000;
                        OC3CONCLR = 0x8000;
                        T4CONCLR = 0x8000;
                    } else {
                        straight();
                    }
                }
            }
            updateLedsAndSensor();
        } else if (mode == 3) { // "off track" mode
            if (Btn && !buttonLock) {
                buttonLock = 1;
                display = timer;
                timer = 0;
                mode = 1;
                T1CONCLR = 0x8000; 
                OC2CONCLR = 0x8000;
                T2CONCLR = 0x8000; 
                OC3CONCLR = 0x8000;
                T4CONCLR = 0x8000;
            }
            if (!flag) {
                flag = 1;
                offTrack = timer;
            }
            updateLedsAndSensor();
            flicker (timer%10, timer/10);
            if (offTrack == timer) {
                reverse();
            } else {
                if (sensor != 1111) {
                    mode = 2;
                } else {
                    reverse();
                }
            }
        } else if (mode == 4) {
            if (!flag) {
                flag = 1;
                OC2RS = PWM_FREQ*0.1;
                OC3RS = PWM_FREQ*0.1;
                T1CONSET = 0x8000; 
                OC2CONSET = 0x8000;
                T2CONSET = 0x8000; 
                OC3CONSET = 0x8000;
                T4CONSET = 0x8000;
            }
            if (Btn) {
                buttonLock = 1;
                mode = 1;
                timer = 0;
                OC2RS = PWM_FREQ*0.075;
                OC3RS = PWM_FREQ*0.075;
                T1CONCLR = 0x8000; 
                OC2CONCLR = 0x8000;
                T2CONCLR = 0x8000; 
                OC3CONCLR = 0x8000;
                T4CONCLR = 0x8000;
                TMR1 = 0;
                TMR2 = 0;
                TMR4 = 0;
            }
            modeFour();
        }
        if (!Btn && buttonLock) {
            buttonLock = 0;
        }
        if (micLock) {
            for(i = 0; i < 100; i++);
            micLock = 0;
        }
        if (sensor != newSensor) { // Only unlock if sensor reading is new
            sensorLock = 0;
        }
    }
}
/* Read mic input and convert from analog. */
int readADC(int ch) {
    AD1CON1bits.SAMP = 1;
    while (!AD1CON1bits.DONE);
    return ADC1BUF0;
}
/* Display a digit. */
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
/* Maintain the flickering of the SSD. */
void flicker(int one, int ten){
    displayDigit(0, number[one]);
    for (i = 0;i < REFRESH_RATE; i++);
    displayDigit(1, number[ten]);    
    for (i = 0;i < REFRESH_RATE; i++);
}
/* Update the current sensor value, and update the on-board LEDs. */
void updateLedsAndSensor() {
   if (Pls1 && !Pls2 && !Pls3 && !Pls4) {
       sensor = 1000;
       Led1 = 1;
       Led2 = Led3 = Led4 = 0;
   } else if (!Pls1 && Pls2 && !Pls3 && !Pls4) {
       sensor = 0100;
       Led2 = 1;
       Led1 = Led3 = Led4 = 0;
   } else if (!Pls1 && !Pls2 && Pls3 && !Pls4) {
       sensor = 0010;
       Led3 = 1;
       Led1 = Led2 = Led4 = 0;
   } else if (!Pls1 && !Pls2 && !Pls3 && Pls4) {
       sensor = 0001;
       Led4 = 1;
       Led1 = Led2 = Led3 = 0;
   } else if (Pls1 && Pls2 && !Pls3 && !Pls4) {
       sensor = 1100;
       Led1 = Led2 = 1;
       Led3 = Led4 = 0;
   } else if (Pls1 && !Pls2 && Pls3 && !Pls4) {
       sensor = 1010;
       Led1 = Led3 = 1;
       Led2 = Led4 = 0;
   } else if (Pls1 && !Pls2 && !Pls3 && Pls4) {
       sensor = 1001;
       Led1 = Led4 = 1;
       Led2 = Led3 = 0;
   } else if (!Pls1 && Pls2 && Pls3 && !Pls4) {
       sensor = 0110;
       Led2 = Led3 = 1;
       Led1 = Led4 = 0;
   } else if (!Pls1 && Pls2 && !Pls3 && Pls4) {
       sensor = 0101;
       Led2 = Led4 = 1;
       Led1 = Led3 = 0;
   } else if (!Pls1 && !Pls2 && Pls3 && Pls4) {
       sensor = 0011;
       Led3 = Led4 = 1;
       Led1 = Led2 = 0;
   } else if (Pls1 && Pls2 && Pls3 && !Pls4) {
       sensor = 1110;
       Led1 = Led2 = Led3 = 1;
       Led4 = 0;
   } else if (Pls1 && Pls2 && !Pls3 && Pls4) {
       sensor = 1101;
       Led1 = Led2 = Led4 = 1;
       Led3 = 0;
   } else if (Pls1 && !Pls2 && Pls3 && Pls4) {
       sensor = 1011;
       Led1 = Led3 = Led4 = 1;
       Led2 = 0;
   } else if (!Pls1 && Pls2 && Pls3 && Pls4) {
       sensor = 0111;
       Led2 = Led3 = Led4 = 1;
       Led1 = 0;
   } else if (Pls1 && Pls2 && Pls3 && Pls4) {
       sensor = 1111;
       Led1 = Led2 = Led3 = Led4 = 1;
   } else if (!Pls1 && !Pls2 && !Pls3 && !Pls4) {
       sensor = 0000;
       Led1 = Led2 = Led3 = Led4 = 0;
   } 
}
/* A duty cycle configuration that is *mostly* straight. */
void straight() {
    OC2RS = PWM_FREQ*0.0585;
    OC3RS = PWM_FREQ*0.1;
}
/* A duty cycle configuration that turns left, hard. */
void hardLeft() {
    OC2RS = PWM_FREQ*0.0585;
    OC3RS = PWM_FREQ*0.065;
}
/* A duty cycle configuration that turns right, hard. */
void hardRight() {
    OC2RS = PWM_FREQ*0.085;
    OC3RS = PWM_FREQ*0.1;
}
/* A duty cycle configuration that eases left. */
void slightLeft() {
    OC2RS = PWM_FREQ*0.0585;
    OC3RS = PWM_FREQ*0.08;
}
/* A duty cycle configuration that eases right. */
void slightRight() {
    OC2RS = PWM_FREQ*0.0685;
    OC3RS = PWM_FREQ*0.1;
}
/* A duty cycle configuration that reverses. */
void reverse() {
    OC2RS = PWM_FREQ*0.1;
    OC3RS = PWM_FREQ*0.0555;
}
void modeFour() {
    if (timer % 4 == 0) {
        flicker(11, CLEAR);
    } else if (timer % 4 == 1) {
        flicker(CLEAR, 0);
    } else if (timer % 4 == 2) {
        flicker(0, CLEAR);
    } else if (timer % 4 == 3) {
        flicker(CLEAR, 12);
    }
}
