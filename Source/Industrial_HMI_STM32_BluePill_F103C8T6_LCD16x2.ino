
/*
 * =============================================================================
 * Project: Industrial Control Interface (STM32)
 * =============================================================================
 * Author: rafamuratt
 * Date: June 2026
 * Platform: STM32F103C8T6 (BluePill)
 * MURAT-TECH CHANNEL: https://www.youtube.com/@Murat-TechChannel-EN
 * MURAT-TECH HUB:     https://murat-tech.eu/
 * 

 License
 This project is licensed under the Murat-Tech Source Available License v1.0. 
 Free for personal and educational use with attribution.
 Commercial use requires written authorization — contact info@murat-tech.eu
 https://github.com/rafamuratt/Industrial_HMI_STM32_BluePill_F103C8T6_LCD16x2?tab=License-1-ov-file
 
 If this project is helpfull for your application, please consider to support:
 https://www.paypal.com/donate/?hosted_button_id=8S8BJ9TT368VN

 * NOTES:
 #define VDC_LEVEL: fine adjustment to match the maximum power supply value (VDC) in order to make a correct proporcional read (analog input). 
 Here 3266 = 3.266V (Fine tunning for power supply, measured with a multimeter)

 #define MENU_SIZE: set the number of menu items (in this project, 5 items).

 * CORE FEATURES:
 * 1. HIGH-SPEED PWM (PA8): 20kHz switching frequency for silent motor control.
 * 2. PSEUDO-DAC (PA9): PWM output providing 0 to 3.3V analog level (may require Low Pass Filter to turn pulses into the smooth 0-3.3V):
     R = 4k7 to 10k
     C = 1 to 10uF (Electrolytic or Ceramic)
 * 3. DUAL-MODE UI (NAV_ADJ button):
 *    - Short Press: Step-by-step parameter increment/decrement.
 *    - 2s Long Press: Flips adjustment direction (+/-) and enters high-speed auto-scroll mode.
 * 4. SAVE THE SETTINGS IN THE EEPROM: Only after press the button SEL_ESC.
 * 5. ATOMIC EEPROM SAVING: Interrupt-safe Flash emulation. Before writing,
 *    all interrupts are suspended via noInterrupts() — pausing TIM2 — to
 *    prevent CPU desync or mid-write corruption. Interrupts are restored
 *    immediately after. This also prevents "dead" buttons during Flash erase
 *    cycles, which can stall the CPU for several milliseconds on STM32.
 * 6. SAFETY SYSTEM: 
 *    - Hardware Interrupt (PC14) for Emergency Stop (EMS).
 *    - Immediate PWM flush and timer pause on EMS trigger.
 *    - Visual Alert with Onboard LED (PC13) blink and <ACK> requirement.
 * 7. LCD INTERFACE: Custom 4-bit Hitachi 16x2 library on PortB with scannable menu tree.
 * =============================================================================
 */

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Libraries */
#include "MT_lcd_16x2_4bits_STM32.h"                                                          // Add an external LCD library
#include <EEPROM.h>                                                                           // Lib emulate EEPROM on Flash memory

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Virtual address for EEPROM (available from 0 to 4095)
const uint16_t addrParam1 = 0x10;
//const uint16_t addrParam2 = 0x20;

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Hardware Mapping & Constants */
#define        SEL_ESC        PA0                                                             // Push-button for confirm, enter, ACK, escape, save + escape
#define        ANALOG_IN      PA1
#define        NAV_ADJ        PA2                                                             // Push-button for scroll, select, adjust value, ON/OFF (PWM and Analog Output)
#define        PWM            PA8
#define        ANALOG_OUT     PA9                                                             // Analog output (pseudo)

/* The output for 16x2 LCD is define by the library as follows:
PB3 = RS (reset)
PB4 = EN (enable)
PB5 = D4 (data4 to 7)
PB6 = D5
PB7 = D6
PB8 = D7
*/

#define        LED_ONBOARD    PC13  
#define        EMS            PC14

#define        MENU_SIZE      5                                                               // Number of menu items
#define        VDC_LEVEL      3266                                                            // 3266 = 3.266V (Fine tunning for power supply)


/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Objects */
HardwareTimer *pwmTimer = new HardwareTimer(TIM1);                                            // Create a pointer for the timer 1 object (PWM)
HardwareTimer *isrTimer = new HardwareTimer(TIM2);                                            // Create a pointer for the timer 2 object (interruption)

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Functions Prototypes */
void sysStop();
void checkBt();
void menuPage();
void subMenuPage();
void analogIN();
void params();
void saveSettings();
void loadSettings();
void pwm_An_Update();

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Global variables - volatile forces constant RAM check */
unsigned short menuNumber = 1, subMenu = 0;
bool updateLCD = 1, systemRun = 1, inMenu = 0;
int analogValue = 0;

volatile bool emsFlag  = 0; 
static bool changeDir = 0, enterFlag = 0;                                        
static volatile int data1 = 0,                                                                // Simulate some object detection
                    param1 = 10;                                                              // despite int, parameter is set from 0 - 255                                              

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Interruptions */
void myIsr(){                                                                                 // TIM2 interruption each 10ms
     static int count1 = 0, count2 = 0;                                                       // NOte: no ISR flag clearing needed (Auto-handled in the STM core).

     if(digitalRead(EMS) == LOW){                                                             // Emergency is active
          emsFlag = 1;
          systemRun = 0;
          sysStop();

          count2 += 1;
          if(count2 == 50){                                                                   // 500ms base time (10ms x 50)
               count2 = 0; 
               digitalWrite(LED_ONBOARD, !digitalRead(LED_ONBOARD));                          // Toggle the onboard LED every 500ms                                                              
          }
     }     
     else{
          digitalWrite(LED_ONBOARD, HIGH);                                                    // Ensure LED stays OFF when EMS is safe
          count2 = 0; 
     } 
     count1 += 1;
     if(count1 == 25){                                                                        // 250ms base time (10ms x 25)
          count1 = 0;                                            
          if(systemRun){
               data1++;                                                                       // Just increment data present at home screen
               if(data1 > 1024) data1 = 0;                                                    
          }
     }                                       
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Main */
void setup(){
     
     lcd_begin();
     lcd_send(0x0C);
     
     pinMode(SEL_ESC, INPUT_PULLUP);
     pinMode(NAV_ADJ, INPUT_PULLUP);
     pinMode(EMS, INPUT_PULLUP);                                                              // Emergency switch
     pinMode(ANALOG_IN, INPUT_ANALOG);                                                        // PA1

     pinMode(LED_ONBOARD, OUTPUT);
     digitalWrite(LED_ONBOARD, HIGH);                                                         // Reverse logic (HIGH = LED onboard OFF)
   
     pwmTimer->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PWM);                                    // Timer 1, Channel 1, set the PWM output on PA8                                                      
     pwmTimer->setMode(2, TIMER_OUTPUT_COMPARE_PWM1, ANALOG_OUT);                             // Timer 1, Channel 2, set the Analog output on PA9  
     pwmTimer->setOverflow(20000, HERTZ_FORMAT);                                              // 20kHz, not audible 
     pwmTimer->setCaptureCompare(1, 0, TICK_COMPARE_FORMAT);                                  // Set duty to 0 (PA8 = OFF) 
     pwmTimer->resume();           

     isrTimer->setOverflow(10000, MICROSEC_FORMAT);                                           // Timer 2 overflow each 10ms (Note: The STM core handles the clock (RCC) and PSC/ARR automatically)
     isrTimer->attachInterrupt(myIsr);
     isrTimer->resume();                                                                      // Start the interruption 

     loadSettings();                                                                          // Recover the parameter values on restart/ boot
     pwm_An_Update();                                                                         // Apply the saved parameter (PWM/ pseudo Analog out) during startup

     lcd_char(1,1,'M');                                                                       // Splash startup screen
     lcd_chrCp('T');
     lcd_str(2,1,"MURAT-TECH");
     DELAY_MS(1500);                                                       
}

void loop(){
     // EMERGENCY DISPLAY LOGIC
     if (emsFlag) {
          // While in emergency, keep the screen fixed
          lcd_str(1,1," EMERGENCY STOP ");
          lcd_str(2,1,"          <ACK> ");
     } 
    
     // NORMAL NAVIGATION LOGIC
     else {
          if(updateLCD){
               menuPage();
               updateLCD = false;
          } 
          if(!emsFlag && subMenu && (menuNumber == 3 || menuNumber == 4)) subMenuPage();      // Constant lcd refresh (some sub-menus only)                                                                 
          if(!emsFlag && !inMenu && !subMenu) lcd_num(2,5,data1,VAL_MIL);                     // Home screen live data update    
     }
     checkBt();
     delay(40);                                                                               // Small delay to prevent LCD flooding
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Read Buttons */
void checkBt() {
    static uint32_t pressStartTime = 0, firstRepeatTime = 0, lastRepeatTime = 0;                              
    static bool longPressTriggered = 0, menuFlag = 0, updateEnable = 0;

    // --- SEL_ESC BUTTON LOGIC ---
     if((digitalRead(SEL_ESC) == LOW)) enterFlag = 1;

     if((digitalRead(SEL_ESC)) && enterFlag && emsFlag){
          if(!inMenu) enterFlag = 0;                                                          // Make sure to return to the main (or menu pages) after emergency ACK
          
          // Only allow ACK if the physical Emergency button is NOT pressed (High)
          if(digitalRead(EMS)){
               emsFlag = 0;
               inMenu = 0;
               lcd_clear();
               updateLCD = 1;
          }
          else updateLCD = 1;     // Force redraw of "EMERGENCY STOP <ACK>"  
          return;
     }

     if((digitalRead(SEL_ESC)) && enterFlag){
          enterFlag = 0;
     
          if(!inMenu) inMenu = true;                                                          // From Home Screen -> Go to Menu List
                
          else if(inMenu && !subMenu) {
               // Check if its in the "RETURN" option
               if(menuNumber == 2) { 
                    inMenu = false;                                                           // Go back to Home Screen
                    menuNumber = 1;                                                           // Reset cursor to top for next time
               } 
               
               else subMenu = true;                                                           // From Menu List -> Go into a Sub-Menu (Settings)   
          }
          else if(subMenu) {
               if(menuNumber == 3) saveSettings();                                            // Save the parameters data into Flash (pseudo EEPROM)
                    
               // From inside a Sub-Menu -> Go back to Menu List (Escape) and RESET to "unstick" the buttons
               subMenu = false;
               menuFlag = 0; 
               enterFlag = 0;
               longPressTriggered = 0;
          }
        
          updateLCD = true;
          delay(50);                                                                          // Small "debounce" to let the physical button settle after the save
     }

    // --- NAV_ADJ BUTTON LOGIC ---
    if(digitalRead(NAV_ADJ) == LOW){ 
          if(menuFlag == 0) {
               menuFlag = 1;
               pressStartTime = millis();
               longPressTriggered = 0;
               updateEnable = 0;                                                              // Reset on every new press
          }         

          if(subMenu && menuNumber == 3){
               if (!longPressTriggered && (millis() - pressStartTime > 2000)){                // Detect the 2s mark to flip direction (increase/ decrease)
                    longPressTriggered = 1;
                    changeDir = !changeDir; 
                    firstRepeatTime = millis();
               }

               if(longPressTriggered){                                                        // Constant increase/decrease while holding (after the flip)
                    if(millis() - firstRepeatTime > 500) updateEnable = 1;                    // Small delay after flipping direction before numbers start flying

                    if(updateEnable && (millis() - lastRepeatTime > 100)){ 
                         lastRepeatTime = millis();
                         if(changeDir) {                    
                              param1 -= 2; 
                              if(param1 < 0) param1 = 0;
                         }
                         else {
                              param1 += 2;
                              if(param1 > 255) param1 = 255;
                         }
                         updateLCD = true; 
                    }
               }
          }
    } 
    else {                                                                                    // NAV_ADJ button is RELEASED
          if(menuFlag == 1){
               
               // CASE A: short tap (Step Feature) 
               if(!longPressTriggered){                                             
                    if(inMenu && !subMenu) {     
                         menuNumber++;
                         if(menuNumber > MENU_SIZE) menuNumber = 1;
                    }
                    else if(subMenu){
                         if(menuNumber == 1){
                              systemRun = !systemRun;
                              if(systemRun){
                                   pwm_An_Update();                                           // Restore the duty cycle from params
                                   pwmTimer->resume();
                              } else sysStop();
                         }
                         else if(menuNumber == 3){                                            // STEP FEATURE: Uses current changeDir
                              if (changeDir) param1 -= 3;                                     // (incr/ dec of 3 due data conv mapping 0-255 = 0-100%)
                              else           param1 += 3;  
                              
                              if (param1 < 0) param1 = 0;
                              if (param1 > 255) param1 = 255;
                         }
                    }
               }
               
               // CASE B: General Reset after any release
               menuFlag = 0;
               longPressTriggered = 0;
               updateEnable = 0;
               updateLCD = true;
          }
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* System Stop */

void sysStop(){
     pwmTimer->setCaptureCompare(1, 0, TICK_COMPARE_FORMAT);                                  // Set duty to 0 (PA8 = OFF) 
     pwmTimer->setCaptureCompare(2, 0, TICK_COMPARE_FORMAT);                                  // Set duty to 0 (PA9 = OFF)
     pwmTimer->getHandle()->Instance->EGR = TIM_EGR_UG;                                       // Force and ensure that both PA8 and PA9 are "flushed" to 0V simultaneously before the clock stops
     pwmTimer->pause();
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Home and Menu pages */

void menuPage(){
     // 1. HOME PAGE
     if(!inMenu && !subMenu) {
          lcd_clear();
          lcd_str(1,2,"STM32: ");
          if(systemRun) lcd_str(1,9,"Running");
          else lcd_str(1,9,"Stopped");

          lcd_str(2,10,"Pcs");
          
          return;
     }

     // 2. CATEGORY LIST (">" menu)
     if(inMenu && !subMenu){
          char text[][16] = { "RUN/STOP       ",                                              // Open text quantity (lines []) limited to 15 columns + NULL [16]
                              "RETURN         ",                                              // Not used the whole 16 coluns + NULL [17] because '>' occupies the column 1 and already
                              "MOTOR SPEED    ",                                              // start counting from column 2
                              "VOLTAGE LEVEL  ",                                      
                              "ABOUT          "};
          
          char i,j;
          
          if(!subMenu){                                                                       // Menu level
               lcd_char(1,1,'>');                                                             // Print '>'
               lcd_char(2,1,' ');                                                             // Print nothing at second line, first column: only to "clean" trash data
          }
          switch(menuNumber){
               case 1:
                    for(i=0, j=2; i<16 && j<17; i++, j++){                                    // "for" is used for faster customization (using data in text[][17]) instead direct print
                         lcd_char(1,j,text[0][i]);                                            // Print the menu 1 in the line 1
                         lcd_char(2,j,text[1][i]);                                            // Print the menu 2 in the line 2
                    }
                    break;

               case 2:
                    for(i=0, j=2; i<16 && j<17; i++, j++){                       
                         lcd_char(1,j,text[1][i]);                                            // Print the menu 2 in the line 1
                         lcd_char(2,j,text[2][i]);                                            // Print the menu 3 in the line 2
                    }
                    break;

               case 3:
                    for(i=0, j=2; i<16 && j<17; i++, j++){                       
                         lcd_char(1,j,text[2][i]);                               
                         lcd_char(2,j,text[3][i]);                               
                    }
                    break;

               case 4:
                    for(i=0, j=2; i<16 && j<17; i++, j++){                       
                         lcd_char(1,j,text[3][i]);                               
                         lcd_char(2,j,text[4][i]);                               
                    }
                    break;

               case 5:
                    for(i=0, j=2; i<16 && j<17; i++, j++){                       
                         lcd_char(1,j,text[4][i]);                               
                         lcd_char(2,j,text[0][i]);                               
                    }
                    break;
          }
     }

     // 3. SUB-MENU (Setting values)
     else if(subMenu) {
          lcd_char(1,1,' ');
          subMenuPage();                                                            // Sub-menu level
     }
}
     
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Sub-menus */

void subMenuPage(){
     switch (menuNumber){
          case 1:
               lcd_str(1,1,"System: ");
               if(systemRun){
                    lcd_str(1,9,"Running ");
                    lcd_str(2,1,"<STOP>          ");
               } 
               else{
                    lcd_str(1,9,"Stopped ");
                    lcd_str(2,1,"<RUN>           ");
               }          
               break;

          case 2:                                                                             // Nothing is here (Return option)

               break;

          case 3:
               lcd_str(1,1," Motor 1: ");
               lcd_str(2,1,"     Speed      ");
               params();
               break;       

          case 4:                                                               
               lcd_str(1,1,"PA1-ADC Channel1");
               lcd_str(2,1,"  Level: ");
               analogIN();
               break;

          case 5:
               lcd_str(1,1,"Check our blog!");
               lcd_str(2,1," murat-tech.eu ");
               break;
     }
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Check the voltage level on analog input (10 bits resolution) */

void analogIN(){
     analogValue = analogRead(ANALOG_IN);                                                     // Read analog input PA1

     int voltage = map(analogValue, 0, 1023, 0, VDC_LEVEL);                                   // Convert 0 to 3.3V in 10 bits data

     // Data handling for LCD print (fake floating point)
     if(voltage >= 3000){
          voltage = voltage - 3000;
          lcd_str(2,10,"3.");
     }

     else if(voltage >= 2000 && voltage <= 2999){
          voltage = voltage - 2000;
          lcd_str(2,10,"2.");
     }

     else if(voltage >= 1000 && voltage <= 1999){
          voltage = voltage - 1000;
          lcd_str(2,10,"1.");
     }

     else if(voltage >= 0 && voltage <= 999){
          lcd_str(2,10,"0.");
     }
     voltage = voltage/ 10;
     lcd_numZero(2,12,voltage,VAL_DEZ);
     lcd_str(2,14,"V   ");
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Paramaters handling      */

void params(){
     if (changeDir){                                                                          // Provide visual feedback of direction
          lcd_str(2,2,"(-) ");                                                      
          lcd_str(2,12,"    ");                                                               // erase old (+)
     }
     else{
          lcd_str(2,12,"(+) ");
          lcd_str(2,2,"    ");                                                                // erase old (-)
     }           

     uint32_t percData = map(param1, 0, 255, 0, 100);
     if(systemRun) pwm_An_Update();                                                           // Only release PWM if in running condition 

     lcd_num(1,11,percData,VAL_CEN);
     lcd_char(1,14,'%');
     
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Save settings in the virtual EEPROM (section of Flash memory) */

void saveSettings() {
     noInterrupts();                                                                          // Pause TIM2 and other interrupts
     EEPROM.update(addrParam1, param1);
     interrupts();                                                                            // Resume TIM2
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* Read data back. If Flash is empty, it returns 0xFFFF (65535) */

void loadSettings() {
    uint16_t stored1 = EEPROM.read(addrParam1);
    if (stored1 != 0xFFFF) param1 = stored1;                                                  // Check if the data is valid before applying
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* @20kHz: Update the PWM and pseudo analog output (average voltage level) */

void pwm_An_Update() {
     uint32_t maxValue = pwmTimer->getOverflow();
     
     uint32_t duty = map(param1, 0, 255, 0, maxValue);                                        // PWM out, set duty cycle based on param1: 0 to 255 = 0 to 100%
     uint32_t pseudoAN = map(param1, 0, 255, 0, maxValue);                                    // Pseudo analog out, average level based on param1: 0 to 255 = 0 to 100%

     pwmTimer->setCaptureCompare(1, duty, TICK_COMPARE_FORMAT);                               // Use the 'percent' logic but scaled to 0-255 range
     pwmTimer->setCaptureCompare(2, pseudoAN, TICK_COMPARE_FORMAT);                           // The same for the pseudo Analog out
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
