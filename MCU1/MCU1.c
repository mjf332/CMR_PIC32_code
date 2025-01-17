/*
 * File:        MCU1
 * Author:      Matthew Filipek
 * Target PIC:  PIC32MX250F128B
 */



////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
#include "pt_cornell_1_2.h"
#include <plib.h>

////////////////////////////////////
// graphics libraries
#include "tft_gfx.h"
#include "tft_master.h"
#include "SD_Card.h"
#include "PWM_logic.h"
#include "I2C_Reg_defs.h"


char buffer[60]; // string buffer
//static int speedTarget; // target fan speed


//#define I2CBAUD 100000 //clock operating at 10kHz
//#define BRG_VAL  ((PBCLK/2/I2CBAUD)-2)
//#define I2CAddress 0x1C



static char I2CDataIn;
static int byteReceved;
static int config1 = 0x0E;
static int Status1 = 0xFF;
static int LF_PWM;
static int LF_P = 23;
static int LF_I = 12;
static int LF_D = 10;
static int LM_PWM;
static int LM_P = 23;
static int LM_I = 12;
static int LM_D = 10;
static int LB_PWM;
static int LB_P = 23;
static int LB_I = 12;
static int LB_D = 10;
static unsigned char halted;

static int LF_Speed;
static int LM_Speed;
static int LB_Speed;
static unsigned char Bat_lvl;
static unsigned char Hum_sense;


//---I2C Register Addresses---


////Config
//#define ADDR_CLR_I2C_STATE 0xFF
//#define ADDR_Config1  0x00 
///* BIT7(MSB) = PID_EN
// * 
// * PID_EN - If enabled, PWM addresses represent target speeds, if
// * disabled, they will be interpreted as duty cycles. Note: in all cases, motor
// * speed/effort is set with a signed 8-bit number
// * 
// * BIT6 = LowPowerShutoff_EN
// * 
// * LowPowerShutoff_EN - MCU will halt all motors if low battery signal recieved
// * 
// * BIT5 = MOTOR_HALT
// * 
// * MOTOR_HALT - MCU will halt all motors
// * 
// * BIT4 = RAMP_DOWN_EN
// * 
// * RAMP_DOWN_EN - MCU will set a maximum speed delta; any speed changes
// * that exceed this delta will be done gradually rather than
// * instantaneously
// * 
// * BIT3 = MCU_TIMEOUT_EN
// * 
// * MCU_TIMEOUT - MCU will set all motor efforts to 0 if no command has been received 
// * for 2 seconds
// * 
// * BIT2 = 6_WHEEL_EN
// * 
// * 6_WHEEL_EN - when bit is set, all 6 wheels are used, otherwise only 4 are in use
// * 
// * Default state of config1 = 0x8E
// */
//#define SIX_WHEEL_EN    0x02
//
////Read-only
//#define ADDR_Status1  0x01
///*
// * BIT7(MSB) = LF_ENC_STAT
// * BIT6 =LM_ENC_STAT
// * BIT5 = LB_ENC_STAT
// * 
// * XX_ENC_STAT - Bit indicates if the encoder for the given motor is
// * connected, 1 for connected, 0 for disconnected
// * 
// * BIT4 = LF_STALL
// * BIT3 = LM_STALL
// * BIT2 = LB_STALL
// * 
// * XX_STALL - Bit indicates if the given motor has stalled, i.e. it is
// * being given a sufficiently high PWM duty cycle, but is still not
// * moving. This may also indicate an improper connection to the motor. 0
// * if stalled, 1 if nominal
// * 
// * BIT1 = BAT_CUTOFF
// * 
// * BAT_CUTOFF - Bit indicates the rover's battery is running low, 1 if
// * nominal, 0 if low
//
//Default state of register = 0xFF
// */
//#define LF_ENC_STAT 0x80
//#define LM_ENC_STAT 0x40
//#define LB_ENC_STAT 0x20
//
//
////Motor addresses
//#define ADDR_LF_PWM 0x10 //NOTE, motor speeds are signed, 2s compliment numbers
//#define ADDR_LF_P 0x11
//#define ADDR_LF_I 0x12
//#define ADDR_LF_D 0x13
//#define ADDR_LM_PWM 0x14
//#define ADDR_LM_P 0x15
//#define ADDR_LM_I 0x16
//#define ADDR_LM_D 0x17
//#define ADDR_LB_PWM 0x18
//#define ADDR_LB_P 0x19
//#define ADDR_LB_I 0x1A
//#define ADDR_LB_D 0x1B
//
////Motor Bank Addresses
//#define ADDR_ALL_PWM 0x20 //NOTE, motor speeds are signed, 2s compliment numbers
//#define ADDR_ALL_P  0x21
//#define ADDR_ALL_I  0x22
//#define ADDR_ALL_D  0x23
//#define ADDR_HALT  0x2F
//
//
////----Read-only addresses----
//
//#define ADDR_LF_SPEED 0x30 //NOTE, motor speeds are signed, 2s compliment numbers
//#define ADDR_LM_SPEED 0x31
//#define ADDR_LB_SPEED 0x32
//#define ADDR_BAT_LVL 0x33
//#define ADDR_HUM_SENSE 0x34


//// command array
static char cmd[30];
static int value;
static int count;


//#define LF_ENC_MASK 0x08
//#define LM_ENC_MASK 0x10
//#define LB_ENC_MASK 0x10

static int LF_error;
static int LF_integral;
static int LF_effort;
static int LF_lastSpeed;
static int LM_error;
static int LM_integral;
static int LM_effort;
static int LM_lastSpeed;
static int LB_error;
static int LB_integral;
static int LB_effort;
static int LB_lastSpeed;
static int count_LF;
static int count_LM;
static int count_LB;
static int IntThresh = 0;
signed short LB_speed_out;
//// receive function prototype (see below for code)
//// int GetDataBuffer(char *buffer, int max_size);
//
//// === thread structures ============================================
//// thread control structs
static struct pt pt_uart, pt_pid, pt_anim, pt_input, pt_output, pt_DMA_output, pt_ENC;

static PT_THREAD(protothread_Anim(struct pt *pt)){
    PT_BEGIN(pt);
    while(1){
        tft_fillRect(250,0,70,200, BLACK);
        tft_setRotation(1);
        tft_setCursor(0,0);
        tft_setTextSize(3);
        tft_setTextColor(WHITE);
        sprintf(buffer, "Speed target: %d", LB_PWM );
        tft_writeString(buffer);
        tft_setCursor(0,50);
        sprintf(buffer, "Motor effort: %d", LB_effort );        
        tft_writeString(buffer);
        tft_setCursor(0,100);
        sprintf(buffer, "Motor speed:  %d", LB_Speed );        
        tft_writeString(buffer);
        tft_setCursor(0,150);
        sprintf(buffer, "Error:        %d", LB_error );        
        tft_writeString(buffer);
        
        PT_YIELD_TIME_msec(100);
    }
    
    PT_END(pt);
}

static int ENC_TEST_THRESHOLD = 1000;
static PT_THREAD(protothread_ENC(struct pt *pt)){
    PT_BEGIN(pt);
    while(1){
        if (!(Status1 & F_ENC_STAT)) {//if encoder already unplugged
            if ((Status1 & B_ENC_STAT) && ((config1 & SIX_WHEEL_EN) && (Status1 & M_ENC_STAT))) {//if 
                LF_effort = (LM_effort + LB_effort) >> 1; //set speed to average of remaining motors
            } else if ((config1 & SIX_WHEEL_EN) && (Status1 & M_ENC_STAT)) {
                LF_effort = LM_effort;
            } else if ((Status1 & B_ENC_STAT)) {
                LF_effort = LB_effort;
            } else {
                LF_effort = 2000;
            }

            CNPUA |= LF_ENC_MASK; //pull-up enable
            CNPDA &= !LF_ENC_MASK;
            PT_YIELD_TIME_msec(1);
            if (!(PORTA & LF_ENC_MASK)) {//if not pulled high
                Status1 |= F_ENC_STAT;
                CNPUA &= !LF_ENC_MASK;
            } else {
                CNPDA |= LF_ENC_MASK; //pull-down
                CNPUA &= !LF_ENC_MASK;
                PT_YIELD_TIME_msec(1);
                if (PORTA & LF_ENC_MASK) {
                    Status1 |= F_ENC_STAT;
                    CNPDA &= !LF_ENC_MASK;
                }

            }
        }
        if (!(Status1 & M_ENC_STAT)) {//if encoder already unplugged
            if ((Status1 & F_ENC_STAT) && ((Status1 & B_ENC_STAT))) {//if 
                LM_effort = (LM_effort + LB_effort) >> 1; //set speed to average of remaining motors
            } else if ((Status1 & B_ENC_STAT)) {
                LM_effort = LB_effort;
            } else if ((Status1 & F_ENC_STAT)) {
                LM_effort = LF_effort;

            } else {
                LM_effort = 2000;
            }

            CNPUB |= LM_ENC_MASK; //pull-up enable
            CNPDB &= !LM_ENC_MASK;
            PT_YIELD_TIME_msec(1);
            if (!(PORTB & LM_ENC_MASK)) {//if not pulled high
                Status1 |= M_ENC_STAT;
                CNPUB &= !LM_ENC_MASK;
            } else {
                CNPDB |= LM_ENC_MASK; //pull-down
                CNPUB &= !LM_ENC_MASK;
                PT_YIELD_TIME_msec(1);
                if (PORTB & LM_ENC_MASK) {
                    Status1 |= M_ENC_STAT;
                    CNPDB &= !LM_ENC_MASK;

                }

            }
        }
        if (!(Status1 & B_ENC_STAT)) {//if encoder already unplugged
            if ((Status1 & F_ENC_STAT) && ((Status1 & M_ENC_STAT) && (config1 & SIX_WHEEL_EN))) {//if 
                LB_effort = (LM_effort + LF_effort) >> 1; //set speed to average of remaining motors
            } else if ((Status1 & 0x40)&&(config1 & 0x02)) {
                LB_effort = LM_effort;
            } else if ((Status1 & 0x80)) {
                LB_effort = LF_effort;

            } else {
                LB_effort = 2000;
            }

            CNPUA |= LB_ENC_MASK; //pull-up enable
            CNPDA &= !LB_ENC_MASK;
            PT_YIELD_TIME_msec(1);
            if (!(PORTA & LB_ENC_MASK)) {//if not pulled high
                Status1 |= B_ENC_STAT;
                CNPUA &= !LB_ENC_MASK;
            } else {
                CNPDA |= LB_ENC_MASK; //pull-down
                CNPUA &= !LB_ENC_MASK;
                PT_YIELD_TIME_msec(1);
                if (PORTA & LB_ENC_MASK) {
                    Status1 |= B_ENC_STAT;
                    CNPDA &= !LB_ENC_MASK;

                }

            }
        }

        if ((LF_Speed == 0) && ((LF_effort > (2000 + ENC_TEST_THRESHOLD)) || (LF_effort < (2000 - ENC_TEST_THRESHOLD)))) {
            CNPUA |= LF_ENC_MASK; //pull-up enable
            CNPDA &= !LF_ENC_MASK;
            PT_YIELD_TIME_msec(1);
            if (PORTA & LF_ENC_MASK) {//if pin pulled high
                CNPUA &= !LF_ENC_MASK; //pull-up disable
                CNPDA |= LF_ENC_MASK; //pull-down enable
                PT_YIELD_TIME_msec(1); //leave time for value to settle
                if (!(PORTA & LF_ENC_MASK)) {//if pin pulled low
                    Status1 &= !F_ENC_STAT;
                    if ((Status1 & B_ENC_STAT) && ((config1 & SIX_WHEEL_EN) && (Status1 & M_ENC_STAT))) {//if 
                        LF_effort = (LM_effort + LB_effort) >> 1; //set speed to average of remaining motors
                    } else if ((config1 & SIX_WHEEL_EN) && (Status1 & M_ENC_STAT)) {
                        LF_effort = LM_effort;
                    } else if ((Status1 & B_ENC_STAT)) {
                        LF_effort = LB_effort;
                    } else {
                        LF_effort = 2000;
                    }
                }
            }

            CNPUA &= !LF_ENC_MASK; //
            CNPDA &= !LF_ENC_MASK; //            
        }
        if ((LM_Speed == 0) && ((LM_effort > (2000 + ENC_TEST_THRESHOLD)) || (LM_effort < (2000 - ENC_TEST_THRESHOLD)) && (config1 & SIX_WHEEL_EN))) {
            CNPUB |= LM_ENC_MASK; //pull-up enable
            CNPDB &= !LM_ENC_MASK; //pull-up enable
            PT_YIELD_TIME_msec(1); //let pin value settle
            if (PORTB & LM_ENC_MASK) {//if pin pulled high
                CNPUB &= !LM_ENC_MASK; //pull-up disable
                CNPDB |= LM_ENC_MASK; //pull-down enable
                PT_YIELD_TIME_msec(1); //leave time for value to settle
                if (!(PORTB & LM_ENC_MASK)) {//if pin pulled low
                    Status1 &= !M_ENC_STAT;
                    if ((Status1 & F_ENC_STAT) && ((Status1 & B_ENC_STAT))) {//if 
                        LM_effort = (LM_effort + LB_effort) >> 1; //set speed to average of remaining motors
                    } else if ((Status1 & B_ENC_STAT)) {
                        LM_effort = LB_effort;
                    } else if ((Status1 & F_ENC_STAT)) {
                        LM_effort = LF_effort;

                    } else {
                        LM_effort = 2000;
                    }
                }
            }

            CNPUB &= !M_ENC_STAT; //
            CNPDB &= !M_ENC_STAT; //            
        }

        if ((LB_Speed == 0) && ((LB_effort > (2000 + ENC_TEST_THRESHOLD)) || (LB_effort < (2000 - ENC_TEST_THRESHOLD)))) {
            CNPUA |= LB_ENC_MASK; //pull-up enable
            CNPDA &= !LB_ENC_MASK;
            PT_YIELD_TIME_msec(1); //let pin value settle
            if (PORTA & LB_ENC_MASK) {//if pin pulled high
                CNPUA &= !LB_ENC_MASK; //pull-up disable
                CNPDA |= LB_ENC_MASK; //pull-down enable
                PT_YIELD_TIME_msec(1); //leave time for value to settle
                if (!(PORTA & LB_ENC_MASK)) {//if pin pulled low
                    Status1 &= !B_ENC_STAT;
                    if ((Status1 & F_ENC_STAT) && ((Status1 & M_ENC_STAT) && (config1 & SIX_WHEEL_EN))) {//if 
                        LB_effort = (LM_effort + LF_effort) >> 1; //set speed to average of remaining motors
                    } else if ((Status1 & M_ENC_STAT)&&(config1 & SIX_WHEEL_EN)) {
                        LB_effort = LM_effort;
                    } else if ((Status1 & F_ENC_STAT)) {
                        LB_effort = LF_effort;

                    } else {
                        LB_effort = 2000;
                    }
                }
            }

            CNPUA &= !LB_ENC_MASK; //
            CNPDA &= !LB_ENC_MASK; //  
        }





    SetDCOC4PWM(LF_effort);
    SetDCOC1PWM(LM_effort);
    SetDCOC5PWM(LB_effort);

        PT_YIELD_TIME_msec(20);

    }
    PT_END(pt);
}

static int LF_dir;
static int LM_dir;
static int LB_dir;
static int LF_reverse;
static int LM_reverse;
static int LB_reverse;
static int last_last_LB_effort;
static int last_last_LM_effort;
static int last_last_LF_effort;
static int last_LF_effort;
static int last_LM_effort;
static int last_LB_effort;
#define MIN_STOP_SPEED 2100
#define MAX_STOP_SPEED 2500


static PT_THREAD(protothread_pid(struct pt *pt)) {
    PT_BEGIN(pt);
    while (1) {

        if (Status1 & F_ENC_STAT) {
            
            if(LF_effort > MIN_STOP_SPEED && LF_effort < MAX_STOP_SPEED){
                if(last_LF_effort != LF_effort){//check for divide by zero condition
                    LF_dir = (LF_Speed- LF_lastSpeed)/(LF_effort - last_LF_effort) ;//+((RB_Speed- RB_lastSpeed)/(RB_effort - last_RB_effort)- (RB_lastSpeed- RB_last_last_speed)/(last_RB_effort - last_last_RB_effort))/(RB_effort-last_last_RB_effort);// ds/de gives direction information    
                }
            }
            else if (LF_effort <= MIN_STOP_SPEED){//effort very small, speed is certainly negative
                LF_dir = -1;
            }
            else if (LF_effort >= MAX_STOP_SPEED){//effort very large, positive speed
                LF_dir = 1;
            }
            else{
                LF_dir = LF_reverse;
            }
            
            if(LF_dir < 0) {
                if(LF_Speed > 0){
                    LF_Speed *= -1;
                }
                LF_reverse = -1;//save direction
            }
            else if(LF_dir == 0 && (LF_reverse==-1)){
                if(LF_Speed>0){
                    LF_Speed *= -1;
                }//in special case when speed did not change, latch last direction
            }
            else{
                if(LF_Speed < 0){
                    LF_Speed *= -1; 
                }
                LF_reverse = 1; //save direction
            }            
            
            
            
            LF_error = LF_PWM - LF_Speed; // get error from difference

            if (abs(LF_error) > IntThresh) {
                LF_integral += LF_error;
            } else {
                LF_integral *= 0.9;
            }
            if (LF_integral > 400) {
                LF_integral = 400;
            } else if (LF_integral < -400) {
                LF_integral = -400;
            }
            LF_effort += ((LF_error * LF_P)/ 16.0) +((LF_integral * LF_I)/64.0) + (((LF_lastSpeed - LF_Speed) * LF_D)/32.0);
            
            if(LF_effort > 3999){
                LF_effort = 3999;
            }
            else if(LF_effort < 1){
                LF_effort = 1;
            }
            
        } 


        if (Status1 & M_ENC_STAT && config1 & SIX_WHEEL_EN) {
            if(LM_effort > MIN_STOP_SPEED && LM_effort < MAX_STOP_SPEED){
                if(last_LM_effort != LM_effort ){//check for divide by zero condition
                    LM_dir = (LM_Speed- LM_lastSpeed)/(LM_effort - last_LM_effort) ;// ds/de gives direction information    
                }
            }
            else if (LM_effort <= MIN_STOP_SPEED){//effort very small, speed is certainly negative
                LM_dir = -1;
            }
            else if (LM_effort >= MAX_STOP_SPEED){//effort very large, positive speed
                LM_dir = 1;
            }
            else{
                LM_dir = LM_reverse;
            }
            
            if(LM_dir < 0) {
                if(LM_Speed > 0){
                    LM_Speed *= -1;
                }
                LM_reverse = -1;//save direction
            }
            else if(LM_dir == 0 && (LM_reverse==-1)){
                if(LM_Speed>0){
                    LM_Speed *= -1;
                }//in special case when speed did not change, latch last direction
            }
            else{
                if(LM_Speed < 0){
                    LM_Speed *= -1; 
                }
                LM_reverse = 1; //save direction
            }            
            
            
            
            LM_error = LM_PWM - LM_Speed; // get error from difference

            if (abs(LM_error) > IntThresh) {
                LM_integral += LM_error;
            } else {
                LM_integral *= 0.9;
            }
            if (LM_integral > 400) {
                LM_integral = 400;
            } else if (LM_integral < -400) {
                LM_integral = -400;
            }
            LM_effort += ((LM_error * LM_P)/16.0) +((LM_integral * LM_I)/64.0) + (((LM_lastSpeed - LM_Speed) * LM_D)/32.0);
            
            if(LM_effort > 3999){
                LM_effort = 3999;
            }
            else if(LM_effort < 1){
                LM_effort = 1;
            }
            
        } 

        if (Status1 & B_ENC_STAT) {
            
            if(LB_effort > MIN_STOP_SPEED && LB_effort < MAX_STOP_SPEED){
                if(last_LB_effort != LB_effort){//check for divide by zero condition
                    LB_dir = (LB_Speed- LB_lastSpeed)/(LB_effort - last_LB_effort) ;//+((RB_Speed- RB_lastSpeed)/(RB_effort - last_RB_effort)- (RB_lastSpeed- RB_last_last_speed)/(last_RB_effort - last_last_RB_effort))/(RB_effort-last_last_RB_effort);// ds/de gives direction information    
                }
            }
            else if (LB_effort <= MIN_STOP_SPEED){//effort very small, speed is certainly negative
                LB_dir = -1;
            }
            else if (LB_effort >= MAX_STOP_SPEED){//effort very large, positive speed
                LB_dir = 1;
            }
            else{
                LB_dir = LB_reverse;
            }
            
            if(LB_dir < 0) {
                if(LB_Speed > 0){
                    LB_Speed *= -1;
                }
                LB_reverse = -1;//save direction
            }
            else if(LB_dir == 0 && (LB_reverse==-1)){
                if(LB_Speed>0){
                    LB_Speed *= -1;
                }//in special case when speed did not change, latch last direction
            }
            else{
                if(LB_Speed < 0){
                    LB_Speed *= -1; 
                }
                LB_reverse = 1; //save direction
            }            
            LB_speed_out = LB_Speed; 
            
            
            LB_error = LB_PWM - LB_Speed; // get error from difference

            if (abs(LB_error) > IntThresh) {
                LB_integral += LB_error;
            } else {
                LB_integral *= 0.9;
            }
            if (LB_integral > 400) {
                LB_integral = 400;
            } else if (LB_integral < -400) {
                LB_integral = -400;
            }
            LB_effort += ((LB_error * LB_P)/16.0) +((LB_integral * LB_I)/64.0) + (((LB_lastSpeed - LB_Speed) * LB_D)/32.0);
            
            if(LB_effort > 3999){
                LB_effort = 3999;
            }
            else if(LB_effort < 1){
                LB_effort = 1;
            }
            
        } 
        
//        tft_fillRect(0,0,240,30, ILI9340_BLACK);
//        tft_setTextColor(ILI9340_WHITE);
//        tft_setTextSize(2);
//        tft_setCursor(0,0);
//        sprintf(buffer, "%d %d %d", LM_Speed, LM_effort, LM_PWM);
//        tft_writeString(buffer);
        
        LM_lastSpeed = LM_Speed;
        LB_lastSpeed = LB_Speed;
        LF_lastSpeed = LF_Speed;
        last_LB_effort = LB_effort;
        last_LM_effort = LM_effort;
        last_LF_effort = LF_effort;
        SetDCOC4PWM(LF_effort);
        SetDCOC1PWM(LM_effort);
        SetDCOC5PWM(LB_effort);
        PT_YIELD_TIME_msec(20);
    }
    PT_END(pt);
}



// external interrupt

void __ISR(_EXTERNAL_1_VECTOR, ipl1) INT1Interrupt(void) {
    //tft_fillScreen(ILI9340_BLACK); //240x320 vertical display
    count_LF++;

    // clear interrupt flag
    mINT1ClearIntFlag();
}
void __ISR(_EXTERNAL_2_VECTOR, ipl2) INT2Interrupt(void) {
    //tft_fillScreen(ILI9340_BLACK); //240x320 vertical display
    count_LB++;

    // clear interrupt flag
    mINT2ClearIntFlag();
}
void __ISR(_EXTERNAL_4_VECTOR, ipl4) INT4Interrupt(void) {
    //tft_fillScreen(ILI9340_BLACK); //240x320 vertical display
    count_LM++;

    // clear interrupt flag
    mINT4ClearIntFlag();
}
void __ISR(_TIMER_3_VECTOR, ipl2) Timer3Handler(void) { 
    mT3ClearIntFlag();
}


void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void) { 
    LM_Speed = count_LM;//record number of encoder pulses
    LF_Speed = count_LF;
    LB_Speed = count_LB;

    count_LM = 0;//zero counter variables
    count_LF = 0;
    count_LB = 0;
    mT2ClearIntFlag(); //clear interrupt flag, if you forget to do this, the microcontroller will interrupt continuously
}

static int I2Cstate = 0;
static unsigned char I2C_request;

///////////////////////////////////////////////////////////////////
//
// Slave I2C interrupt handler
// This handler is called when a qualifying I2C events occurs
// this means that as well as Slave events
// Master and Bus Collision events will also trigger this handler.
//
///////////////////////////////////////////////////////////////////
static unsigned char WDTCount = 0;
void __ISR(_I2C_1_VECTOR, ipl3) _SlaveI2CHandler(void) {
    unsigned char temp;
    static unsigned int dIndex;

    // check for MASTER and Bus events and respond accordingly
    if (IFS1bits.I2C1MIF) {
        mI2C1MClearIntFlag();
        return;
    }
    if (IFS1bits.I2C1BIF) {
        I2Cstate = 0;
        mI2C1BClearIntFlag();
        return;
    }

    // handle the incoming message
    if ((I2C1STATbits.R_W == 0) && (I2C1STATbits.D_A == 0)) {
        // reset any state variables needed by a message sequence
        // perform a dummy read
        temp = SlaveReadI2C1();
        I2C1CONbits.SCLREL = 1; // release the clock
        I2Cstate = 0;
    } else if ((I2C1STATbits.R_W == 0) && (I2C1STATbits.D_A == 1)) {//data received, input to slave
        WDTCount = 0;//reset watchdog
        WriteTimer1(0);//feed the watchdog
        
        // writing data to our module
        I2CDataIn = SlaveReadI2C1();
        byteReceved = 1;
        I2C1CONbits.SCLREL = 1; // release clock stretch bit
        
        if( I2Cstate == 0 && I2CDataIn != ADDR_CLR_I2C_STATE){
            I2Cstate = 1;
            I2C_request = I2CDataIn;
        }
        else if(I2Cstate == 1){
            switch(I2C_request){
                case ADDR_ALL_PWM:
                    if (config1 & PID_EN){
                        LF_PWM = getPWM(I2CDataIn);
                        LM_PWM = LF_PWM;
                        LB_PWM = LF_PWM;
                    }
                    else{
                        LF_effort = getEffort(I2CDataIn);
                        LM_effort = LF_effort;
                        LB_effort = LF_effort;
                    }
                    break;
                case ADDR_ALL_P:
                    LF_P = *(unsigned char*)(&I2CDataIn);
                    LM_P = *(unsigned char*)(&I2CDataIn);
                    LB_P = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_ALL_I:
                    LF_I = *(unsigned char*)(&I2CDataIn);
                    LM_I = *(unsigned char*)(&I2CDataIn);
                    LB_I = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_ALL_D:
                    LF_D = *(unsigned char*)(&I2CDataIn);
                    LM_D = *(unsigned char*)(&I2CDataIn);
                    LB_D = *(unsigned char*)(&I2CDataIn);
                    break;                     
                case ADDR_F_PWM:
                    if (config1 & PID_EN){
                        LF_PWM = getPWM(I2CDataIn);
                    }
                    else{
                        LF_effort = getEffort(I2CDataIn);
                    }
                    break;
                case ADDR_F_P:
                    LF_P = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_F_I:
                    LF_I = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_F_D:
                    LF_D = *(unsigned char*)(&I2CDataIn); 
                    break;
                    
                case ADDR_M_PWM:
                    if (config1 & PID_EN){
                        LM_PWM = getPWM(I2CDataIn);
                    }
                    else{
                        LM_effort = getEffort(I2CDataIn);
                    }
                    break;
                case ADDR_M_P:
                    LM_P = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_M_I:
                    LM_I = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_M_D:
                    LM_D = *(unsigned char*)(&I2CDataIn); 
                    break;
                case ADDR_B_PWM:
                    if (config1 & PID_EN){
                        LB_PWM = getPWM(I2CDataIn);
                    }
                    else{
                        LB_effort = getEffort(I2CDataIn);
                    }
                    break;
                case ADDR_B_P:
                    LB_P = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_B_I:
                    LB_I = *(unsigned char*)(&I2CDataIn);
                    break;
                case ADDR_B_D:
                    LB_D = *(unsigned char*)(&I2CDataIn); 
                    break;
                case ADDR_Config1:
                    config1 = I2CDataIn;
                    break;                   
                default:
                    break;
            
            } 
            I2Cstate = 0;
        }
        
        
    } else if ((I2C1STATbits.R_W == 1) && (I2C1STATbits.D_A == 0)) {
        // read of the slave device, read the address
        temp = SlaveReadI2C1();
        if (I2Cstate == 1) {
            I2Cstate = 0;
            switch (I2C_request) {
                case ADDR_B_SPEED:
                    SlaveWriteI2C1(LB_speed_out >> 8);
                    break;
                default:
                    SlaveWriteI2C1(0x7F);
                    I2C_request = 0;
                    break;
            }
        }
    } else if ((I2C1STATbits.R_W == 1) && (I2C1STATbits.D_A == 1)) {
        // output the data until the MASTER terminates the
        // transfer with a NACK, continuing reads return 0
        switch (I2C_request) {
            case ADDR_B_SPEED:
                SlaveWriteI2C1(LB_speed_out & 255);
                break;
            default:
                SlaveWriteI2C1(0x7F);
                I2C_request = 0;
                break;
        }

    }

    mI2C1SClearIntFlag();
}

void InitI2C(void) {

    CloseI2C1();
    
    // Enable the I2C module with clock stretching enabled
    // define a Master BRG of 400kHz although this is not used by the slave
    OpenI2C1(I2C_SLW_DIS | I2C_ON | I2C_7BIT_ADD | I2C_STR_DIS|I2C_SM_EN, BRG_VAL); //48);
//    I2C1BRG = BRG_VAL;
//    I2C1CON = 0xD040;
//    I2C1CON &= !I2C_SM_EN;
//    I2C1CON |= 0x40;
    // set the address of the slave module
    I2C1ADD = I2CAddress;
    I2C1MSK = 0;
    // I2C1CON |= 0x8000;
    // configure the interrupt priority for the I2C peripheral
    mI2C1SetIntPriority(I2C_INT_PRI_3 | I2C_INT_SLAVE);

    // clear pending interrupts and enable I2C interrupts
    mI2C1SClearIntFlag();
    EnableIntSI2C1;
}


inline void setWDT(void){
    OpenTimer1(T1_ON | T1_SOURCE_INT | T1_PS_1_256, 60000);
    ConfigIntTimer1(T1_INT_ON | T1_INT_PRIOR_1);
    
    
}

void __ISR(_TIMER_1_VECTOR, ipl1) WatchdogInt(void)
{
    WDTCount ++;
    if(WDTCount > 3 && (config1 & MCU_TIMEOUT)){//~1sec
        WDTCount = 3; //prevent overflow
        if(config1 & PID_EN){
            LM_PWM = 0;
            LB_PWM = 0;
            LF_PWM = 0;
        }
        else{

            LM_effort = 2250;
            LB_effort = 2250;
            LF_effort = 2250;

        }
    }
    mT1ClearIntFlag();  
}


// === Main  ======================================================


void main(void) {


    SYSTEMConfigPerformance(sys_clock);
    // === I2C Init ================
    InitI2C();

    
//    ANSELA = 0; //make sure analog is cleared
//    ANSELB = 0;
//    AD1CON2 = 0;
    mJTAGPortEnable(0);
//    
//    
    PT_setup();
//    RPA0R = 0;
//    RPA1R = 0;
//    CM2CON = 0;
//    CVRCON = 0x0000;
//    ANSELA = 0; //make sure analog is cleared
//    ANSELB = 0;
//    count = 0;
//    
//    mPORTASetPinsDigitalOut(BIT_2);
    mPORTBSetPinsDigitalOut(BIT_2 | BIT_3);    
//    
    RPB2R = 0x05;  // OC 4
    RPB3R = 0x05;  // OC 1
    RPA2R = 0x06; // OC 5

    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_256, 4000);
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    
    OpenTimer3(T3_ON|T3_SOURCE_INT| T3_PS_1_8, 4000);
    ConfigIntTimer3(T3_INT_OFF | T3_INT_PRIOR_2);

    // set up OC for PWM
    ConfigIntOC1(OC_INT_PRIOR_5 | EXT_INT_SUB_PRI_2);
    OpenOC1(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 2250, 2250);//LM pin 6
    ConfigIntOC4(OC_INT_PRIOR_5 | EXT_INT_SUB_PRI_2);
    OpenOC4(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 2250, 2250);//LF pin 7
    ConfigIntOC5(OC_INT_PRIOR_5 | EXT_INT_SUB_PRI_2);
    OpenOC5(OC_ON | OC_TIMER3_SRC | OC_PWM_FAULT_PIN_DISABLE, 2250, 2250);//LB pin 9
//
//    mPORTBSetPinsDigitalIn(BIT_4);
////    mPORTASetPinsDigitalIn(BIT_3 | BIT_4);
//    
    INT1R = 0; //pin 10, LF_ENC
    INT4R = 0x02; //pin 11, LM_ENC
    INT2R = 0x02; //pin 12, LB_ENC
    
    ConfigINT1(EXT_INT_ENABLE | RISING_EDGE_INT |EXT_INT_PRI_1);
    ConfigINT2(EXT_INT_ENABLE | RISING_EDGE_INT |EXT_INT_PRI_2);
    ConfigINT4(EXT_INT_ENABLE | RISING_EDGE_INT |EXT_INT_PRI_4);

    
    // init the threads

    PT_INIT(&pt_pid);
    PT_INIT(&pt_ENC);
    PT_INIT(&pt_anim);
    LF_PWM = 0;
    LM_PWM = 0;
    LB_PWM = 0;
//    
    LF_effort = 2250;
    LM_effort = 2250;
    LB_effort = 2250;
    SetDCOC4PWM(LF_effort);
    SetDCOC1PWM(LM_effort);
    SetDCOC5PWM(LB_effort);
    tft_init_hw();
    tft_begin();
    tft_fillScreen(BLACK); //240x320 vertical display
    tft_setRotation(1);
    
//    SPISTAT = 0;
//    if (!FSInit()) {
//        tft_fillRect(0, 0, 20, 20, RED);//print this if init fails
//        while (1);
//    }
//    spicon_fs = SPI1CON;
//    spicon2_fs = SPI1CON2;// reset configuration bits so that TFT writes can occur
//    sprintf(buffer, "%s", CMR);//"CMR_logo.bmp");
//    readBMP(buffer, 0, 0);
    
    setWDT();
    INTEnableSystemMultiVectoredInt();
    //round robin thread schedule
    while (1) {
        if(config1 & PID_EN){
//            PT_SCHEDULE(protothread_ENC(&pt_ENC));
            PT_SCHEDULE(protothread_pid(&pt_pid));
        }
        else{
            SetDCOC4PWM(LF_effort);
            SetDCOC1PWM(LM_effort);
            SetDCOC5PWM(LB_effort);
        }
        PT_SCHEDULE(protothread_Anim(&pt_anim));
        I2C1CONbits.SCLREL = 1; // release the clock

    }
} // main


// === end  ======================================================
