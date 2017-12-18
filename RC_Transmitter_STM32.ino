// Micro RC Project. A tiny little 2.4GHz and LEGO "Power Functions" / "MECCANO" IR RC transmitter!
// 3.3V, 72MHz STM32F103C8T6 ARM board
// Atmel AVR version see: https://github.com/TheDIYGuy999/RC_Transmitter
// 2.4GHz NRF24L01 radio module
// SSD 1306 128 x 63 0.96" OLED
// Custom PCB will follow
// Menu for the following adjustments:
// -Channel reversing
// -Channel travel limitation adjustable in steps of 5%
// -Value changes are stored in EEPROM, individually per vehicle
// Radio transmitter tester included (press "Select" button during power up)
// NRF24L01+PA+LNA SMA radio modules with power amplifier are also supported
// ATARI PONG game :-) Press the "Back" button during power on to start it

const float codeVersion = 2.01; // Software revision

//
// =======================================================================================================
// BUILD OPTIONS (comment out unneeded options)
// =======================================================================================================
//

//#define DEBUG // if not commented out, Serial.print() is active! For debugging only!!
//#define OLED_DEBUG // if not commented out, an additional diagnostics screen is shown during startup

//
// =======================================================================================================
// INCLUDE LIRBARIES
// =======================================================================================================
//

// Libraries
#include <Wire.h>
#include <SPI.h>
#include <RF24_STM32.h> // https://github.com/TheDIYGuy999/RF24_STM32
#include <EEPROM.h> // Part of https://github.com/rogerclarkmelbourne/Arduino_STM32
#include <LegoIr.h> // https://github.com/TheDIYGuy999/LegoIr
#include <statusLED.h> // https://github.com/TheDIYGuy999/statusLED
#include <U8g2lib.h> // https://github.com/olikraus/u8g2

// Tabs (.h files in the sketch directory) see further down

//
// =======================================================================================================
// PIN ASSIGNMENTS & GLOBAL VARIABLES
// =======================================================================================================
//

// Is the radio or IR transmission mode active?
byte transmissionMode = 1; // Radio mode is active by default

// Select operation trannsmitter operation mode
byte operationMode = 0; // Start in transmitter mode (0 = transmitter mode, 1 = tester mode, 2 = game mode)

// Vehicle address
int vehicleNumber = 1; // Vehicle number one is active by default

// Radio channels (126 channels are supported)
byte chPointer = 0; // Channel 1 (the first entry of the array) is active by default
const byte NRFchannel[] {
  1, 2
};

// the ID number of the used "radio pipe" must match with the selected ID on the transmitter!
// 10 ID's are available @ the moment
const uint64_t pipeOut[] = {
  0xE9E8F0F0B1LL, 0xE9E8F0F0B2LL, 0xE9E8F0F0B3LL, 0xE9E8F0F0B4LL, 0xE9E8F0F0B5LL,
  0xE9E8F0F0B6LL, 0xE9E8F0F0B7LL, 0xE9E8F0F0B8LL, 0xE9E8F0F0B9LL, 0xE9E8F0F0B0LL
};
const int maxVehicleNumber = (sizeof(pipeOut) / (sizeof(uint64_t)));

// Hardware configuration: Set up nRF24L01 radio on hardware SPI bus & pins PA8 (CE) & PA1 (CSN)
RF24 radio(PA8, PA1);

// The size of this struct should not exceed 32 bytes
struct RcData {
  byte axis1; // Aileron (Steering for car)
  byte axis2; // Elevator
  byte axis3; // Throttle
  byte axis4; // Rudder
  boolean mode1 = false; // Mode1 (toggle speed limitation)
  boolean mode2 = false; // Mode2 (toggle acc. / dec. limitation)
  boolean momentary1 = false; // Momentary push button
  byte pot1; // Potentiometer
} __attribute__((packed, aligned(2))); // no padding, aligned to an address that is divisible by 2 (keeps it compatible with AVR)
RcData data;

// This struct defines data, which are embedded inside the ACK payload
struct ackPayload {
  float vcc; // vehicle vcc voltage
  float batteryVoltage; // vehicle battery voltage
  boolean batteryOk; // the vehicle battery voltage is OK!
  byte channel = 1; // the channel number
} __attribute__((packed, aligned(2))); // no padding, aligned to an address that is divisible by 2 (keeps it compatible with AVR)
ackPayload payload;

// Did the receiver acknowledge the sent data?
boolean transmissionState;

// LEGO powerfunctions IR
LegoIr pf;
int pfChannel;
const int pfMaxAddress = 3;

// Configuration arrays
const int servos = 4;
const byte entries = 44; // Entries per array

//Joystick reversing
uint16_t joystickReversed[maxVehicleNumber + 1][servos] = { // 1 + 10 Vehicle Addresses, 4 Servos
  {false, false, false, false}, // Address 0 used for EEPROM initialisation

  {false, false, false, false}, // Address 1
  {false, false, false, false}, // Address 2
  {false, false, false, false}, // Address 3
  {false, false, false, false}, // Address 4
  {false, false, false, false}, // Address 5
  {false, false, false, false}, // Address 6
  {false, false, false, false}, // Address 7
  {false, false, false, false}, // Address 8
  {false, false, false, false}, // Address 9
  {false, false, false, false}, // Address 10
};

//Joystick percent negative
uint16_t joystickPercentNegative[maxVehicleNumber + 1][servos] = { // 1 + 10 Vehicle Addresses, 4 Servos
  {100, 100, 100, 100}, // Address 0 not used

  {100, 100, 100, 100}, // Address 1
  {100, 100, 100, 100}, // Address 2
  {100, 100, 100, 100}, // Address 3
  {100, 100, 100, 100}, // Address 4
  {100, 100, 100, 100}, // Address 5
  {100, 100, 100, 100}, // Address 6
  {100, 100, 100, 100}, // Address 7
  {100, 100, 100, 100}, // Address 8
  {100, 100, 100, 100}, // Address 9
  {100, 100, 100, 100}, // Address 10
};

//Joystick percent positive
uint16_t joystickPercentPositive[maxVehicleNumber + 1][servos] = { // 1 + 10 Vehicle Addresses, 4 Servos
  {100, 100, 100, 100}, // Address 0 not used

  {100, 100, 100, 100}, // Address 1
  {100, 100, 100, 100}, // Address 2
  {100, 100, 100, 100}, // Address 3
  {100, 100, 100, 100}, // Address 4
  {100, 100, 100, 100}, // Address 5
  {100, 100, 100, 100}, // Address 6
  {100, 100, 100, 100}, // Address 7
  {100, 100, 100, 100}, // Address 8
  {100, 100, 100, 100}, // Address 9
  {100, 100, 100, 100}, // Address 10
};

// TX voltages
boolean batteryOkTx = false;
#define BATTERY_DETECT_PIN PB1 // The 20k & 10k battery detection voltage divider is connected to pin PB1
float txVcc;
float txBatt;

// Joysticks
#define JOYSTICK_1 PA4
#define JOYSTICK_2 PA0
#define JOYSTICK_3 PA3
#define JOYSTICK_4 PA2

// Potentiometer
#define POT_1 PB0

// Joystick push buttons
#define JOYSTICK_BUTTON_LEFT PB4
#define JOYSTICK_BUTTON_RIGHT PB3

byte leftJoystickButtonState;
byte rightJoystickButtonState;

// Buttons
#define BUTTON_LEFT PB12 // - or channel select
#define BUTTON_RIGHT PB14 // + or transmission mode select
#define BUTTON_SEL PB13 // select button for menu
#define BUTTON_BACK PB15 // back button for menu

byte leftButtonState = 7; // init states with 7 (see macro below)!
byte rightButtonState = 7;
byte selButtonState = 7;
byte backButtonState = 7;

// macro for detection of rising edge and debouncing
/*the state argument (which must be a variable) records the current and the last 3 reads
  by shifting one bit to the left at each read and bitwise anding with 15 (=0b1111).
  If the value is 7(=0b0111) we have one raising edge followed by 3 consecutive 1's.
  That would qualify as a debounced raising edge*/
#define DRE(signal, state) (state=(state<<1)|(signal&1)&15)==7

// Status LED objects (false = not inverted)
statusLED greenLED(false); // green: ON = ransmitter ON, flashing = Communication with vehicle OK
statusLED redLED(false); // red: ON = battery empty

// OLED display SSD1306
// SDA = PB7, SCL = PB6 (Hardware I2C)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

int activeScreen = 0; // the currently displayed screen number (0 = splash screen)
boolean displayLocked = true;
byte menuRow = 0; // Menu active cursor line

//
// =======================================================================================================
// INCLUDE TABS (header files in sketch directory)
// =======================================================================================================
//

#include "readVCC.h"
#include "eepromHandler.h"
#include "transmitterConfig.h"
#include "MeccanoIr.h" // https://github.com/TheDIYGuy999/MeccanoIr
#include "pong.h" // A little pong game :-)

//
// =======================================================================================================
// RADIO SETUP
// =======================================================================================================
//

void setupRadio() {
  radio.begin();
  radio.setChannel(NRFchannel[chPointer]);

  radio.powerUp();

  // Set Power Amplifier (PA) level to one of four levels: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH and RF24_PA_MAX
  if (boardVersion < 1.1 ) radio.setPALevel(RF24_PA_MIN); // No independent NRF24L01 3.3 PSU, so only "MIN" transmission level allowed
  else radio.setPALevel(RF24_PA_MAX); // Independent NRF24L01 3.3 PSU, so "FULL" transmission level allowed

  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(pipeOut[vehicleNumber - 1], true); // Ensure autoACK is enabled
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.setRetries(5, 5);                  // 5x250us delay (blocking!!), max. 5 retries
  //radio.setCRCLength(RF24_CRC_8);          // Use 8-bit CRC for performance*/

#ifdef DEBUG
  radio.printDetails();
  delay(1800);
#endif

  // All axes to neutral position
  data.axis1 = 50;
  data.axis2 = 50;
  data.axis3 = 50;
  data.axis4 = 50;

  // Transmitter
  if (operationMode == 0) {
    radio.openWritingPipe(pipeOut[vehicleNumber - 1]); // Vehicle Number 1 = Array number 0, so -1!
    radio.write(&data, sizeof(RcData));
  }

  // Receiver (radio tester mode)
  if (operationMode == 1) {
    radio.openReadingPipe(1, pipeOut[vehicleNumber - 1]);
    radio.startListening();
  }
}

//
// =======================================================================================================
// LEGO POWERFUNCTIONS SETUP
// =======================================================================================================
//

void setupPowerfunctions() {
  pfChannel = vehicleNumber - 1;  // channel 0 - 3 is labelled as 1 - 4 on the LEGO devices!

  if (pfChannel > pfMaxAddress) pfChannel = pfMaxAddress;

  pf.begin(PB5, pfChannel);  // Pin PB5, channel 0 - 3
}

//
// =======================================================================================================
// MAIN ARDUINO SETUP (1x during startup)
// =======================================================================================================
//

void setup() {

#ifdef DEBUG
  Serial.begin(115200);
  printf_begin();
  delay(3000);
#endif

  // Disable STM32 JTAG debug pins! Allows to use pin PB3 and PB4
  // See: http://www.stm32duino.com/viewtopic.php?f=3&t=2084&p=27945&hilit=pb3#p27916
  disableDebugPorts();

  // ADC setup for VCC readout
  setup_vcc_sensor();

  // LED setup
  greenLED.begin(PB9); // Green LED on pin PB9
  redLED.begin(PB8); // Red LED on pin PB8

  // Pinmodes (all other pinmodes are handled inside libraries)
  pinMode(JOYSTICK_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(JOYSTICK_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_SEL, INPUT_PULLUP);
  pinMode(BUTTON_BACK, INPUT_PULLUP);

  pinMode(JOYSTICK_1, INPUT_ANALOG);
  pinMode(JOYSTICK_2, INPUT_ANALOG);
  pinMode(JOYSTICK_3, INPUT_ANALOG);
  pinMode(JOYSTICK_4, INPUT_ANALOG);

  pinMode(POT_1, INPUT_ANALOG);

  // EEPROM setup
  setupEeprom();

  if (joystickReversed[0][0] || (!digitalRead(BUTTON_BACK) && !digitalRead(BUTTON_SEL))) { // 65535 is EEPROM default after a program download,
    // this indicates that we have to initialise the EEPROM with our default values! Or manually triggered with
    // both "SEL" & "BACK" buttons pressed during switching on!
    writeEepromDefaults(); // then write defaults to EEPROM
    readEeprom(); // and fill the arrays with the new EEPROM values!

  }

  // Switch to radio tester mode, if "Select" button is pressed
  if (digitalRead(BUTTON_BACK) && !digitalRead(BUTTON_SEL)) {
    operationMode = 1;
  }

  // Switch to game mode, if "Back" button is pressed
  if (!digitalRead(BUTTON_BACK) && digitalRead(BUTTON_SEL)) {
    operationMode = 2;
  }

  // Joystick setup
  JoystickOffset(); // Compute all joystick center points
  readJoysticks(); // Then do the first jocstick read

  // Radio setup
  setupRadio();

  // LEGO Powerfunctions setup
  setupPowerfunctions();

  // Display setup
  u8g2.begin();
  u8g2.setFontRefHeightExtendedText();
  u8g2.setFont(u8g_font_6x10);

  // Splash screen
  checkBattery();
  activeScreen = 0; // 0 = splash screen active
  drawDisplay();
#ifdef OLED_DEBUG
  activeScreen = 100; // switch to the diagnostics screen
  delay(1500);
  drawDisplay();
#endif
  activeScreen = 1; // switch to the main screen
  delay(1500);
}

//
// =======================================================================================================
// BUTTONS
// =======================================================================================================
//

// Sub function for channel travel adjustment and limitation --------------------------------------
void travelAdjust(boolean upDn) {
  int16_t inc = 5;
  if (upDn) inc = 5; // Direction +
  else inc = -5; // -

  if ( (menuRow & 0x01) == 0) {// even (2nd column)
    joystickPercentPositive[vehicleNumber][(menuRow - 6) / 2 ] += inc; // row 6 - 12 = 0 - 3
  }
  else { // odd (1st column)
    joystickPercentNegative[vehicleNumber][(menuRow - 5) / 2 ] += inc;  // row 5 - 11 = 0 - 3
  }
  joystickPercentPositive[vehicleNumber][(menuRow - 6) / 2 ] = constrain(joystickPercentPositive[vehicleNumber][(menuRow - 6) / 2 ], 20, 100);
  joystickPercentNegative[vehicleNumber][(menuRow - 5) / 2 ] = constrain(joystickPercentNegative[vehicleNumber][(menuRow - 5) / 2 ], 20, 100);
}

// Main buttons function --------------------------------------------------------------------------
void readButtons() {

  // Every 10 ms
  static unsigned long lastTrigger;
  if (millis() - lastTrigger >= 10) {
    lastTrigger = millis();

    // Left joystick button (Mode 1)
    if (DRE(digitalRead(JOYSTICK_BUTTON_LEFT), leftJoystickButtonState) && (transmissionMode == 1)) {
      data.mode1 = !data.mode1;
      drawDisplay();
    }

    // Right joystick button (Mode 2)
    if (DRE(digitalRead(JOYSTICK_BUTTON_RIGHT), rightJoystickButtonState) && (transmissionMode == 1)) {
      data.mode2 = !data.mode2;
      drawDisplay();
    }

    if (activeScreen <= 10) { // if menu is not displayed ----------

      // Left button: Channel selection
      if (DRE(digitalRead(BUTTON_LEFT), leftButtonState) && (transmissionMode < 3)) {
        vehicleNumber ++;
        if (vehicleNumber > maxVehicleNumber) vehicleNumber = 1;
        setupRadio(); // Re-initialize the radio with the new pipe address
        setupPowerfunctions(); // Re-initialize the LEGO IR transmitter with the new channel address
        drawDisplay();
      }

      // Right button: Change transmission mode. Radio <> IR
      if (infrared) { // only, if transmitter has IR option
        if (DRE(digitalRead(BUTTON_RIGHT), rightButtonState)) {
          if (transmissionMode < 3) transmissionMode ++;
          else {
            transmissionMode = 1;
            setupRadio(); // Re-initialize radio, if we switch back to radio mode!
          }
          drawDisplay();
        }
      }
    }
    else { // if menu is displayed -----------
      // Left button: Value -
      if (DRE(digitalRead(BUTTON_LEFT), leftButtonState)) {
        if (activeScreen == 11) {
          joystickReversed[vehicleNumber][menuRow - 1] = false;
        }
        if (activeScreen == 12) {
          travelAdjust(false); // -
        }
        drawDisplay();
      }

      // Right button: Value +
      if (DRE(digitalRead(BUTTON_RIGHT), rightButtonState)) {
        if (activeScreen == 11) {
          joystickReversed[vehicleNumber][menuRow - 1] = true;
        }
        if (activeScreen == 12) {
          travelAdjust(true); // +
        }
        drawDisplay();
      }
    }

    // Menu buttons:

    // Select button: opens the menu and scrolls through menu entries
    if (DRE(digitalRead(BUTTON_SEL), selButtonState) && (transmissionMode == 1)) {
      activeScreen = 11; // 11 = Menu screen 1
      menuRow ++;
      if (menuRow > 4) activeScreen = 12; // 12 = Menu screen 2
      if (menuRow > 12) {
        activeScreen = 11; // Back to menu 1, entry 1
        menuRow = 1;
      }
      drawDisplay();
    }

    // Back / Momentary button:
    if (activeScreen <= 10) { // Momentary button, if menu is NOT displayed
      if (!digitalRead(BUTTON_BACK)) data.momentary1 = true;
      else data.momentary1 = false;
    }
    else { // Goes back to the main screen & saves the changed entries in the EEPROM
      if (DRE(digitalRead(BUTTON_BACK), backButtonState)) {
        activeScreen = 1; // 1 = Main screen
        menuRow = 0;
        drawDisplay();
        writeEeprom(); // update changed values in EEPROM
      }
    }
  }
}

//
// =======================================================================================================
// JOYSTICKS
// =======================================================================================================
//

int offset[4]; // the auto calibration offset of each joystick

// Auto-zero subfunction (called during setup) ----
void JoystickOffset() {
  offset[0] = 2047 - analogRead(JOYSTICK_1);
  offset[1] = 2047 - analogRead(JOYSTICK_2);
  offset[2] = 2047 - analogRead(JOYSTICK_3);
  offset[3] = 2047 - analogRead(JOYSTICK_4);
}

// Mapping and reversing subfunction ----
byte mapJoystick(byte input, byte arrayNo) {
  int reading[4];
  reading[arrayNo] = analogRead(input) + offset[arrayNo]; // read joysticks and add the offset
  reading[arrayNo] = constrain(reading[arrayNo], (4095 - range), range); // then limit the result before we do more calculations below

#ifdef CONFIG_2_CH // In most "car style" transmitters, less than a half of the throttle potentiometer range is used for the reverse. So we have to enhance this range!
  if (reading[2] < (range / 2) ) {
    reading[2] = constrain(reading[2], (range / 3), (range / 2)); // limit reverse range, which will be multiplied later
    reading[2] = map(reading[2], (range / 3), (range / 2), 0, (range / 2)); // reverse range multiplied by 4
  }
#endif

  if (transmissionMode == 1 && operationMode != 2 ) { // Radio mode and not game mode
    if (joystickReversed[vehicleNumber][arrayNo]) { // reversed
      return map(reading[arrayNo], (4095 - range), range, (joystickPercentPositive[vehicleNumber][arrayNo] / 2 + 50), (50 - joystickPercentNegative[vehicleNumber][arrayNo] / 2));
    }
    else { // not reversed
      return map(reading[arrayNo], (4095 - range), range, (50 - joystickPercentNegative[vehicleNumber][arrayNo] / 2), (joystickPercentPositive[vehicleNumber][arrayNo] / 2 + 50));
    }
  }
  else { // IR mode
    return map(reading[arrayNo], (4095 - range), range, 0, 100);
  }
}

// Main Joystick function ----
void readJoysticks() {

  // save previous joystick positions
  byte previousAxis1 = data.axis1;
  byte previousAxis2 = data.axis2;
  byte previousAxis3 = data.axis3;
  byte previousAxis4 = data.axis4;

  // Read current joystick positions, then scale and reverse output signals, if necessary (only for the channels we have)
#ifdef CH1
  data.axis1 = mapJoystick(JOYSTICK_1, 0); // Aileron (Steering for car)
#endif

#ifdef CH2
  data.axis2 = mapJoystick(JOYSTICK_2, 1); // Elevator
#endif

#ifdef CH3
  data.axis3 = mapJoystick(JOYSTICK_3, 2); // Throttle
#endif

#ifdef CH4
  data.axis4 = mapJoystick(JOYSTICK_4, 3); // Rudder
#endif

  // Only allow display refresh, if no value has changed!
  if (previousAxis1 != data.axis1 ||
      previousAxis2 != data.axis2 ||
      previousAxis3 != data.axis3 ||
      previousAxis4 != data.axis4) {
    displayLocked = true;
  }
  else {
    displayLocked = false;
  }
}

//
// =======================================================================================================
// POTENTIOMETER
// =======================================================================================================
//

void readPotentiometer() {
  data.pot1 = map(analogRead(POT_1), 0, 4095, 0, 100);
  data.pot1 = constrain(data.pot1, 0, 100);
}

//
// =======================================================================================================
// TRANSMIT LEGO POWERFUNCTIONS IR SIGNAL
// =======================================================================================================
//

void transmitLegoIr() {
  static byte speedOld[2];
  static byte speed[2];
  static byte pwm[2];
  static unsigned long previousMillis;

  unsigned long currentMillis = millis();

  // Flash green LED
  greenLED.flash(30, 2000, 0, 0);

  // store joystick positions into an array-----
  speed[0] = data.axis3;
  speed[1] = data.axis2;

  // compute pwm value for "red" and "blue" motor, if speed has changed more than +/- 3, or every 0.6s
  // NOTE: one IR pulse at least every 1.2 s is required in order to prevent the vehivle from stopping
  // due to a signal timeout!
  for (int i = 0; i <= 1; i++) {
    if ((speedOld[i] - 3) > speed[i] || (speedOld[i] + 3) < speed[i] || currentMillis - previousMillis >= 600) {
      speedOld[i] = speed[i];
      previousMillis = currentMillis;
      if (speed[i] >= 0 && speed[i] < 6) pwm[i] = PWM_REV7;
      else if (speed[i] >= 6 && speed[i] < 12) pwm[i] = PWM_REV6;
      else if (speed[i] >= 12 && speed[i] < 18) pwm[i] = PWM_REV5;
      else if (speed[i] >= 18 && speed[i] < 24) pwm[i] = PWM_REV4;
      else if (speed[i] >= 24 && speed[i] < 30) pwm[i] = PWM_REV3;
      else if (speed[i] >= 30 && speed[i] < 36) pwm[i] = PWM_REV2;
      else if (speed[i] >= 36 && speed[i] < 42) pwm[i] = PWM_REV1;
      else if (speed[i] >= 42 && speed[i] < 58) pwm[i] = PWM_BRK;
      else if (speed[i] >= 58 && speed[i] < 64) pwm[i] = PWM_FWD1;
      else if (speed[i] >= 64 && speed[i] < 70) pwm[i] = PWM_FWD2;
      else if (speed[i] >= 70 && speed[i] < 76) pwm[i] = PWM_FWD3;
      else if (speed[i] >= 76 && speed[i] < 82) pwm[i] = PWM_FWD4;
      else if (speed[i] >= 82 && speed[i] < 88) pwm[i] = PWM_FWD5;
      else if (speed[i] >= 88 && speed[i] < 94) pwm[i] = PWM_FWD6;
      else if (speed[i] >= 94) pwm[i] = PWM_FWD7;

      // then transmit IR data
      pf.combo_pwm(pwm[1], pwm[0]); // red and blue in one IR package
    }
  }
}

//
// =======================================================================================================
// TRANSMIT MECCANO / ERECTOR IR SIGNAL
// =======================================================================================================
//

void transmitMeccanoIr() {

  static boolean A;
  static boolean B;
  static boolean C;
  static boolean D;

  // Flash green LED
  greenLED.flash(30, 1000, 0, 0);

  // Channel A ----
  if (data.axis1 > 90) { // A +
    buildIrSignal(1);
    A = true;
  }
  if (data.axis1 < 10) { // A -
    buildIrSignal(2), A = true;
    A = true;
  }
  if (data.axis1 < 90 && data.axis1 > 10 && A) { // A OFF
    buildIrSignal(3);
    A = false;
  }

  // Channel B ----
  if (data.axis2 > 90) { // B +
    buildIrSignal(4);
    B = true;
  }
  if (data.axis2 < 10) { // B -
    buildIrSignal(5), A = true;
    B = true;
  }
  if (data.axis2 < 90 && data.axis2 > 10 && B) { // B OFF
    buildIrSignal(6);
    B = false;
  }

  // Channel C ----
  if (data.axis3 > 90) { // C +
    buildIrSignal(7);
    C = true;
  }
  if (data.axis3 < 10) { // C -
    buildIrSignal(8), A = true;
    C = true;
  }
  if (data.axis3 < 90 && data.axis3 > 10 && C) { // C OFF
    buildIrSignal(9);
    C = false;
  }

  // Channel D ----
  if (data.axis4 > 90) { // D +
    buildIrSignal(10);
    D = true;
  }
  if (data.axis4 < 10) { // D -
    buildIrSignal(11), A = true;
    D = true;
  }
  if (data.axis4 < 90 && data.axis4 > 10 && D) { // D OFF
    buildIrSignal(12);
    D = false;
  }
}

//
// =======================================================================================================
// TRANSMIT RADIO DATA
// =======================================================================================================
//

void transmitRadio() {

  static boolean previousTransmissionState;
  static float previousRxVcc;
  static float previousRxVbatt;
  static boolean previousBattState;
  static unsigned long previousSuccessfulTransmission;

  if (transmissionMode == 1) { // If radio mode is active: ----

    // Send radio data and check if transmission was successful
    if (radio.write(&data, sizeof(struct RcData)) ) {
      if (radio.isAckPayloadAvailable()) {
        radio.read(&payload, sizeof(struct ackPayload)); // read the payload, if available
        //radio.read(&payload, 10); // read the payload, if available //STM32 padding bodge...
        previousSuccessfulTransmission = millis();
      }
    }

    // Switch channel for next transmission
    chPointer ++;
    if (chPointer >= sizeof((*NRFchannel) / sizeof(byte))) chPointer = 0;
    radio.setChannel(NRFchannel[chPointer]);

    // if the transmission was not confirmed (from the receiver) after > 1s...
    if (millis() - previousSuccessfulTransmission > 1000) {
      greenLED.on();
      transmissionState = false;
      memset(&payload, 0, sizeof(payload)); // clear the payload array, if transmission error
#ifdef DEBUG
      Serial.println("Data transmission error, check receiver!");
#endif
    }
    else {
      greenLED.flash(30, 100, 0, 0); //30, 100
      transmissionState = true;
#ifdef DEBUG
      Serial.println("Data successfully transmitted");
#endif
    }

    if (!displayLocked) { // Only allow display refresh, if not locked ----
      // refresh transmission state on the display, if changed
      if (transmissionState != previousTransmissionState) {
        previousTransmissionState = transmissionState;
        drawDisplay();
      }

      // refresh Rx Vcc on the display, if changed more than +/- 0.05V
      if (payload.vcc - 0.05 >= previousRxVcc || payload.vcc + 0.05 <= previousRxVcc) {
        previousRxVcc = payload.vcc;
        drawDisplay();
      }

      // refresh Rx V Batt on the display, if changed more than +/- 0.3V
      if (payload.batteryVoltage - 0.3 >= previousRxVbatt || payload.batteryVoltage + 0.3 <= previousRxVbatt) {
        previousRxVbatt = payload.batteryVoltage;
        drawDisplay();
      }

      // refresh battery state on the display, if changed
      if (payload.batteryOk != previousBattState) {
        previousBattState = payload.batteryOk;
        drawDisplay();
      }
    }

#ifdef DEBUG
    Serial.print(data.axis1);
    Serial.print("\t");
    Serial.print(data.axis2);
    Serial.print("\t");
    Serial.print(data.axis3);
    Serial.print("\t");
    Serial.print(data.axis4);
    Serial.print("\t");
    Serial.println(F_CPU / 1000000, DEC);
#endif
  }
  else { // else infrared mode is active: ----
    radio.powerDown();
  }
}

//
// =======================================================================================================
// READ RADIO DATA (for radio tester)
// =======================================================================================================
//

void readRadio() {

  static unsigned long lastRecvTime = 0;
  byte pipeNo;

  payload.batteryVoltage = txBatt; // store the battery voltage for sending
  payload.vcc = txVcc; // store the vcc voltage for sending
  payload.batteryOk = batteryOkTx; // store the battery state for sending

  if (radio.available(&pipeNo)) {
    radio.writeAckPayload(pipeNo, &payload, sizeof(struct ackPayload) );  // prepare the ACK payload
    radio.read(&data, sizeof(struct RcData)); // read the radia data and send out the ACK payload
    lastRecvTime = millis();
#ifdef DEBUG
    Serial.print(data.axis1);
    Serial.print("\t");
    Serial.print(data.axis2);
    Serial.print("\t");
    Serial.print(data.axis3);
    Serial.print("\t");
    Serial.print(data.axis4);
    Serial.println("\t");
#endif
  }

  // Switch channel
  if (millis() - lastRecvTime > 500) {
    chPointer ++;
    if (chPointer >= sizeof((*NRFchannel) / sizeof(byte))) chPointer = 0;
    radio.setChannel(NRFchannel[chPointer]);
    payload.channel = NRFchannel[chPointer];
  }

  if (millis() - lastRecvTime > 1000) { // set all analog values to their middle position, if no RC signal is received during 1s!
    data.axis1 = 50; // Aileron (Steering for car)
    data.axis2 = 50; // Elevator
    data.axis3 = 50; // Throttle
    data.axis4 = 50; // Rudder
    payload.batteryOk = true; // Clear low battery alert (allows to re-enable the vehicle, if you switch off the transmitter)
#ifdef DEBUG
    Serial.println("No Radio Available - Check Transmitter!");
#endif
  }

  if (millis() - lastRecvTime > 2000) {
    setupRadio(); // re-initialize radio
    lastRecvTime = millis();
  }
}

//
// =======================================================================================================
// LED
// =======================================================================================================
//

void led() {

  // Red LED (ON = battery empty, number of pulses are indicating the vehicle number)
  if (batteryOkTx && (payload.batteryOk || transmissionMode > 1 || !transmissionState) ) {
    if (transmissionMode == 1) redLED.flash(140, 150, 500, vehicleNumber); // ON, OFF, PAUSE, PULSES
    if (transmissionMode == 2) redLED.flash(140, 150, 500, pfChannel + 1); // ON, OFF, PAUSE, PULSES
    if (transmissionMode == 3) redLED.off();

  } else {
    redLED.on(); // Always ON = battery low voltage (Rx or Tx)
  }
}

//
// =======================================================================================================
// CHECK TX BATTERY VOLTAGE
// =======================================================================================================
//

void checkBattery() {

  // Every 500 ms
  static unsigned long lastTrigger;
  if (millis() - lastTrigger >= 500) {
    lastTrigger = millis();

    txBatt = (analogRead(BATTERY_DETECT_PIN) / 413.64) + diodeDrop; // 4095steps / 9.9V = 413.64 + diode drop!

    txVcc = readVcc() / 1000.0 ;

    if (txBatt >= cutoffVoltage) {
      batteryOkTx = true;
#ifdef DEBUG
      Serial.print(txBatt);
      Serial.println(" Tx battery OK");
#endif
    } else {
      batteryOkTx = false;
#ifdef DEBUG
      Serial.print(txBatt);
      Serial.println(" Tx battery empty!");
#endif
    }
  }
}

//
// =======================================================================================================
// DRAW DISPLAY
// =======================================================================================================
//

void drawDisplay() {

  // clear screen ----
  u8g2.clearBuffer();

  switch (activeScreen) {
    case 0: // Screen # 0 splash screen-----------------------------------

      if (operationMode == 0) u8g2.drawStr(3, 10, "Micro RC Transmitter");
      if (operationMode == 1) u8g2.drawStr(3, 10, "Micro RC Tester");
      if (operationMode == 2) u8g2.drawStr(3, 10, "Micro PONG");

      // Dividing Line
      u8g2.drawLine(0, 13, 128, 13);

      // Software version
      u8g2.setCursor(3, 30);
      u8g2.print("SW: ");
      u8g2.print(codeVersion);

      // Hardware version
      u8g2.print(" HW: ");
      u8g2.print(boardVersion);

      u8g2.setCursor(3, 43);
      u8g2.print("created by:");
      u8g2.setCursor(3, 55);
      u8g2.print("TheDIYGuy999");

      break;

    case 100: // Screen # 100 diagnosis screen-----------------------------------

      u8g2.drawStr(3, 10, "Joystick readings:");

      // Joysticks:
      u8g2.setCursor(3, 30);
      u8g2.print("Axis 1: ");
      u8g2.print(data.axis1);
      u8g2.setCursor(3, 40);
      u8g2.print("Axis 2: ");
      u8g2.print(data.axis2);
      u8g2.setCursor(3, 50);
      u8g2.print("Axis 3: ");
      u8g2.print(data.axis3);
      u8g2.setCursor(3, 60);
      u8g2.print("Axis 4: ");
      u8g2.print(data.axis4);

      break;

    case 1: // Screen # 1 main screen-------------------------------------

      // Tester mode ==================
      if (operationMode == 1) {
        // screen dividing lines ----
        u8g2.drawLine(0, 12, 128, 12);

        // Tx: data ----
        u8g2.setCursor(0, 10);
        u8g2.print("CH: ");
        u8g2.print(vehicleNumber);
        u8g2.setCursor(50, 10);
        u8g2.print("Bat: ");
        u8g2.print(txBatt);
        u8g2.print("V");

        drawTarget(0, 14, 50, 50, data.axis4, data.axis3); // left joystick
        drawTarget(74, 14, 50, 50, data.axis1, data.axis2); // right joystick
        drawTarget(55, 14, 14, 50, 14, data.pot1); // potentiometer
      }

      // Transmitter mode ================
      if (operationMode == 0) {
        // screen dividing lines ----
        u8g2.drawLine(0, 13, 128, 13);
        u8g2.drawLine(64, 0, 64, 64);

        // Tx: data ----
        u8g2.setCursor(0, 10);
        if (transmissionMode > 1) {
          u8g2.print("Tx: IR   ");
          if (transmissionMode < 3) u8g2.print(pfChannel + 1);

          u8g2.setCursor(68, 10);
          if (transmissionMode == 2) u8g2.print("LEGO");
          if (transmissionMode == 3) u8g2.print("MECCANO");
        }
        else {
          u8g2.print("Tx: 2.4G");
          u8g2.setCursor(52, 10);
          u8g2.print(vehicleNumber);
        }

        u8g2.setCursor(3, 25);
        u8g2.print("Vcc: ");
        u8g2.print(txVcc);

        u8g2.setCursor(3, 35);
        u8g2.print("Bat: ");
        u8g2.print(txBatt);

        // Rx: data. Only display the following content, if in radio mode ----
        if (transmissionMode == 1) {
          u8g2.setCursor(68, 10);
          if (transmissionState) {
            u8g2.print("Rx: OK");
          }
          else {
            u8g2.print("Rx: ??");

            /*// Display debug values on the right hand side
            u8g2.setCursor(68, 35);
            u8g2.print("R: ");
            u8g2.print(sizeof(joystickReversed));
            u8g2.setCursor(68, 45);
            u8g2.print("N: ");
            u8g2.print(sizeof(joystickPercentNegative));
            u8g2.setCursor(68, 55);
            u8g2.print("P: ");
            u8g2.print(sizeof(joystickPercentPositive));*/
          }

          u8g2.setCursor(3, 45);
          u8g2.print("Mode 1: ");
          u8g2.print(data.mode1);

          u8g2.setCursor(3, 55);
          u8g2.print("Mode 2: ");
          u8g2.print(data.mode2);

          if (transmissionState) {
            u8g2.setCursor(68, 25);
            u8g2.print("Vcc: ");
            u8g2.print(payload.vcc);

            u8g2.setCursor(68, 35);
            u8g2.print("Bat: ");
            u8g2.print(payload.batteryVoltage);

            u8g2.setCursor(68, 45);
            if (payload.batteryOk) {
              u8g2.print("Bat. OK ");
            }
            else {
              u8g2.print("Low Bat. ");
            }
            u8g2.setCursor(68, 55);
            u8g2.print("CH: ");
            u8g2.print(payload.channel);
          }
        }
      }

      // Game mode ================
      // called directly inside the loop() function to increase speed!

      break;

    case 11: // Screen # 11 Menu 1 (channel reversing)-----------------------------------

      u8g2.setCursor(0, 10);
      u8g2.print("Channel Reverse (");
      u8g2.print(vehicleNumber);
      u8g2.print(")");

      // Dividing Line
      u8g2.drawLine(0, 13, 128, 13);

      // Cursor
      if (menuRow == 1) u8g2.setCursor(0, 25);
      if (menuRow == 2) u8g2.setCursor(0, 35);
      if (menuRow == 3) u8g2.setCursor(0, 45);
      if (menuRow == 4) u8g2.setCursor(0, 55);
      u8g2.print(">");

      // Servos
      u8g2.setCursor(10, 25);
      u8g2.print("CH. 1 (R -): ");
      u8g2.print(joystickReversed[vehicleNumber][0]); // 0 = Channel 1 etc.

      u8g2.setCursor(10, 35);
      u8g2.print("CH. 2 (R |): ");
      u8g2.print(joystickReversed[vehicleNumber][1]);

      u8g2.setCursor(10, 45);
      u8g2.print("CH. 3 (L |): ");
      u8g2.print(joystickReversed[vehicleNumber][2]);

      u8g2.setCursor(10, 55);
      u8g2.print("CH. 4 (L -): ");
      u8g2.print(joystickReversed[vehicleNumber][3]);

      break;

    case 12: // Screen # 12 Menu 2 (channel travel limitation)-----------------------------------

      u8g2.setCursor(0, 10);
      u8g2.print("Channel % - & + (");
      u8g2.print(vehicleNumber);
      u8g2.print(")");

      // Dividing Line
      u8g2.drawLine(0, 13, 128, 13);

      // Cursor
      if (menuRow == 5) u8g2.setCursor(45, 25);
      if (menuRow == 6) u8g2.setCursor(90, 25);
      if (menuRow == 7) u8g2.setCursor(45, 35);
      if (menuRow == 8) u8g2.setCursor(90, 35);
      if (menuRow == 9) u8g2.setCursor(45, 45);
      if (menuRow == 10) u8g2.setCursor(90, 45);
      if (menuRow == 11) u8g2.setCursor(45, 55);
      if (menuRow == 12) u8g2.setCursor(90, 55);
      u8g2.print(">");

      // Servo travel percentage
      u8g2.setCursor(0, 25);
      u8g2.print("CH. 1:   ");
      u8g2.print(joystickPercentNegative[vehicleNumber][0]); // 0 = Channel 1 etc.
      u8g2.setCursor(100, 25);
      u8g2.print(joystickPercentPositive[vehicleNumber][0]);

      u8g2.setCursor(0, 35);
      u8g2.print("CH. 2:   ");
      u8g2.print(joystickPercentNegative[vehicleNumber][1]);
      u8g2.setCursor(100, 35);
      u8g2.print(joystickPercentPositive[vehicleNumber][1]);

      u8g2.setCursor(0, 45);
      u8g2.print("CH. 3:   ");
      u8g2.print(joystickPercentNegative[vehicleNumber][2]);
      u8g2.setCursor(100, 45);
      u8g2.print(joystickPercentPositive[vehicleNumber][2]);

      u8g2.setCursor(0, 55);
      u8g2.print("CH. 4:   ");
      u8g2.print(joystickPercentNegative[vehicleNumber][3]);
      u8g2.setCursor(100, 55);
      u8g2.print(joystickPercentPositive[vehicleNumber][3]);

      break;
  }
  // show display queue ----
  u8g2.sendBuffer();
}

// Draw target subfunction for radio tester mode ----
void drawTarget(int x, int y, int w, int h, int posX, int posY) {
  u8g2.drawFrame(x, y, w, h);
  u8g2.drawDisc((x + w / 2) - (w / 2) + (posX / 2), (y + h / 2) + (h / 2) - (posY / 2), 5, 5);
}

//
// =======================================================================================================
// MAIN LOOP
// =======================================================================================================
//

void loop() {

  // only read analog inputs in transmitter (0) or game mode (2)
  if (operationMode == 0 || operationMode == 2) {

    // Read joysticks
    readJoysticks();

    // Read Potentiometer
    readPotentiometer();
  }

  // Transmit data via infrared or 2.4GHz radio
  if (operationMode == 1) readRadio(); // 2.4 GHz radio tester
  if (operationMode == 0) {
    transmitRadio(); // 2.4 GHz radio
    if (transmissionMode == 2) transmitLegoIr(); // LEGO Infrared
    if (transmissionMode == 3) transmitMeccanoIr(); // MECCANO Infrared
  }

  // Refresh display every 50 ms in tester mode 1 (in radio mode 0 only, if value has changed)
  static unsigned long lastDisplay;
  if (operationMode == 1 && millis() - lastDisplay >= 50) {
    lastDisplay = millis();
    drawDisplay();
  }

  // Atari Pong game :-)
  if (operationMode == 2) pong();

  // If not in game mode:
  else {
    led(); // LED control
    checkBattery(); // Check battery
    readButtons();
  }
}
