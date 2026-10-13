/**************************************************************************/
/*! 
    @file     readMifare.pde
    @author   Adafruit Industries
	@license  BSD (see license.txt)

    Modified by Edvards Rutkovskis.
    
    Original description:

    This example will wait for any ISO14443A card or tag, and
    depending on the size of the UID will attempt to read from it.
   
    If the card has a 4-byte UID it is probably a Mifare
    Classic card, and the following steps are taken:
   
    - Authenticate block 4 (the first block of Sector 1) using
      the default KEYA of 0XFF 0XFF 0XFF 0XFF 0XFF 0XFF
    - If authentication succeeds, we can then read any of the
      4 blocks in that sector (though only block 4 is read here)
	 
    If the card has a 7-byte UID it is probably a Mifare
    Ultralight card, and the 4 byte pages can be read directly.
    Page 4 is read by default since this is the first 'general-
    purpose' page on the tags.

    Modifications by Ed:
    The code repurposed for operation with NT3H2211 memory for
    16-channel sVNS implant. 
    NT3H is Mifare Ultralight-supported tag, hence 7-byte UID.
    4-byte UID-related code commented and uncompiled to save the memory.
    All tag memory from page 4 to 15 is read and then user is prompted
    to modify the memory in the Serial Monitor. 
    For the correct operation, select Serial baud rate of 115200 and
    follow prompts precicely.




This is an example sketch for the Adafruit PN532 NFC/RFID breakout boards
This library works with the Adafruit NFC breakout 
  ----> https://www.adafruit.com/products/364
 
Check out the links above for our tutorials and wiring diagrams 
These chips use SPI or I2C to communicate.

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

*/
/**************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

// Uncomment just _one_ line below depending on how your breakout or shield
// is connected to the Arduino:

// Use this line for a breakout with a software SPI connection (recommended):
//Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Use this line for a breakout with a hardware SPI connection.  Note that
// the PN532 SCK, MOSI, and MISO pins need to be connected to the Arduino's
// hardware SPI SCK, MOSI, and MISO pins.  On an Arduino Uno these are
// SCK = 13, MOSI = 11, MISO = 12.  The SS line can be any digital IO pin.
//Adafruit_PN532 nfc(PN532_SS);

// Or use this line for a breakout or shield with an I2C connection:
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// variables and constants for the Serial Read handling   
const byte numChars = 64; // Number of bytes for the array
char receivedChars[numChars]; // Array to store received characters from Serial Read
char tempChars[numChars]; // Temporary array for parsing
//char *ptr = NULL;
//byte index = 0;
boolean newData = false;      // Needed to detect new input from the Serial Mon
boolean recvInProgress = false;
byte ndx = 0;
//boolean memoryWritten = 0;    // Boolean to prevent multiple writings each time connection is lost
uint8_t array_PW_Freq[4]; // array 1 for PW_HB, PW_LB, F_hz_HB, F_hz_LB
uint8_t array_T_on_on_Iset[4]; // array 2 for T_on_0,T_on_1,T_on_2,ON
uint8_t array_mode_ch_tel[4]; // array 3 for current amplitude, stimulation mode, stimulation channel
uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
bool writeResult;
bool stimProtect = 0;  
unsigned long previousMillis = 0;

// Define FSM:
enum TransmitterState {
  STATE_SEEKING_TAG,
  STATE_IDLE,
  STATE_WRITE_WAIT,
  STATE_READ_LOOP,
  STATE_TRIGGER_LOOP
}; 

TransmitterState currentState = STATE_SEEKING_TAG; 
      
void setup(void) {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero

  // Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  // Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  // Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  // Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  // Set timeout so seeking a tag does not permanently block the CPU
  nfc.setPassiveActivationRetries(0x01);
  
  // Serial.println("Waiting for an ISO14443A Card ...");
}

void loop(void) {
  unsigned long currentMillis = millis();

  // 1. First priority is to catch stop condition from GUI
  if (Serial.available() > 0 && Serial.peek() == ' ') {
    Serial.read();
    nfc.reset();
    nfc.begin();
    nfc.SAMConfig();
    currentState = STATE_SEEKING_TAG;
    stimProtect = 0;

    recvInProgress = false; 
    ndx = 0;
    newData = false;

    Serial.println("Emergency Stop: RF Field Dropped. Returning to idle.");
    return; // exit loop
  }
  // 2. Get commands from Serial Interface
  recvWithStartEndMarkers();

  // 3. FSM execution
  switch (currentState) {
    case STATE_SEEKING_TAG: // defining tag seeking state:
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength); // PN532 - enter tag reading mode
      if (success && uidLength == 7) { // Mifare Ultralight protocol ==> assume NT3H2211
        if (stimProtect == 0) { // As soon as the device found stop the stimulation: 
          uint8_t def_string[4] = {0,0,0,0}; // default array of zeros that overwrites all stimulation parameters
          nfc.mifareultralight_WritePage(5, def_string);
          nfc.mifareultralight_WritePage(6, def_string);
          delay(50); // Wait to rectify error states
          nfc.reset(); nfc.begin(); nfc.SAMConfig(); // reboot EFM8
          delay(1);
          success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
          stimProtect = 1;
        }
        Serial.println("sVNS implant detected\n");
        Serial.println("<0> = read once; <1> = write; <2> = loop read; <3> = stimulation loop with telemetry");
        currentState = STATE_IDLE;
      }
      break;
    case STATE_IDLE:
      if (newData == true) {
        uint8_t mode = atoi(receivedChars);
        newData = false;
        if (mode == 0) memRead();
        else if (mode == 1) {
          Serial.println("Enter the programming string:");
          currentState = STATE_WRITE_WAIT;
        }
        else if (mode == 2) { nfc.reset(); nfc.begin(); nfc.SAMConfig(); currentState = STATE_READ_LOOP; }
        else if (mode == 3) { nfc.reset(); nfc.begin(); nfc.SAMConfig(); currentState = STATE_TRIGGER_LOOP; }
      }
    break;

    case STATE_WRITE_WAIT: 
      if (newData == true) {
        memWrite();
        Serial.println("Programming complete. Returning to Idle.");
        Serial.println("<0> = read once; <1> = write; <2> = loop read; <3> = stimulation loop with telemetry");
        currentState = STATE_IDLE; // Safely return to listening mode
      }
      break;

    case STATE_READ_LOOP:
      if (currentMillis - previousMillis >= 800) {
        previousMillis = currentMillis;
        memRead();
      }
      break;

    case STATE_TRIGGER_LOOP:
      if (currentMillis - previousMillis >= 50) {
        previousMillis = currentMillis;
        memReadTrigger();
      }
    break;

  }

}

// Function to store the Serial Input to the string array

// Confirmation function to hold the loop
// void confirm() {
//     // Wait any user input before proceeding
//   Serial.println("\n\nSend any character via 'Send' or press Enter once to proceed");
//   while (!Serial.available());
//   while (Serial.available()) {
//     delay(2);
//   Serial.read();
//   }
// }
void modeSelect() {
 // char * strtokIndx; // this is used by strtok() as an index
  uint8_t mode;
  Serial.println("<0> = read all memory once; <1> = write to memory; <2> = read all memory until restart; <3> = read stim channels until restart");
  
  recvWithStartEndMarkers();

  if (newData == true) {
    //strcpy(tempChars, receivedChars);
    mode = atoi(receivedChars);
    Serial.println("Mode selected: ");
    Serial.println(mode);
    switch (mode) {
      case 0:
        memRead();
        break;
      case 1:
        memWrite();
        break;
      case 2:
        nfc.reset(); nfc.begin(); // this reboots the device to boot the stimulation parameters
        while (Serial.available() == 0) {
          memRead();
          delay(800); // delay of additional 800 ms to prevent cluttering of the serial interface
        }
        stimProtect = 0;
        break;
      case 3:
        nfc.reset(); nfc.begin(); // this reboots the device to boot the stimulation parameters
        memReadTrigger();
        stimProtect = 0;
        break;
    }
    newData = false;
  }
}

void memRead() {
    // Try to read general-purpose user page 4 to 8
  for (uint8_t pageNum = 4; pageNum <= 8; pageNum++){
    Serial.print("Reading page ");
    Serial.println(pageNum);
    switch (pageNum) { // Reading stimulation parameters 
      case 4:
      Serial.println("Pulse Width: HB, LB; Pulse Frequency: HB, LB");
      break;
      case 5:
      Serial.println("Stimulation Time On (Duty Cycle): HB, LB; On/Off byte; Current Amplitude");
      break;
     case 6:
      Serial.println("Stimulation Mode; Stimulation Channel (For Single-Channel Stim); Enable/Disable telemetry");
      break;
      case 7:
      Serial.println("Page 7:");
      break;
      case 8:
      Serial.println("Page 8:");
      break;
    }
    uint8_t data[32];
    delay(200);
    success = nfc.mifareultralight_ReadPage (pageNum, data);
    if (success)
    {
      // Data seems to have been read ... spit it out
      nfc.PrintHexChar(data, 4);
      // implement checksum for the selected page here
      Serial.println("");		

    }
    else
    {
      Serial.println("Ooops ... unable to read the requested page!?");
      nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength); // read the tag again
    }
  }
}

void memWrite() {
  strcpy(tempChars, receivedChars);
  arrayParse(); // convert received string into stimulation parameter integer arrays for the implant
  writeResult = nfc.mifareultralight_WritePage(4, array_PW_Freq); Serial.println("PW and Freq written? "); Serial.println(writeResult);
  writeResult = nfc.mifareultralight_WritePage(5, array_T_on_on_Iset); Serial.println("Stim time, On bit, current ampl. written? "); Serial.println(writeResult);
  writeResult = nfc.mifareultralight_WritePage(6, array_mode_ch_tel); Serial.println("Mode, channel, telemetry bit written? "); Serial.println(writeResult);

  newData = false;
}

// Function to store the Serial Input to the string array
void recvWithStartEndMarkers() {
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (!Serial.available());
    while (Serial.available()>0 && newData == false) {
        delay(2);
        rc = Serial.read();
        if (recvInProgress == true) {
          if (rc != endMarker) {
              receivedChars[ndx] = rc;
              ndx++;
              if (ndx >= numChars) {
                  ndx = numChars - 1;
              }
          }
          else {
              receivedChars[ndx] = '\0'; // terminate the string
              recvInProgress = false;
              ndx = 0;
              newData = true;
          }
        }
        else if (rc == startMarker){
          recvInProgress = true;
        }
    }
}

void arrayParse() {
  char * strtokIndx; // this is used by strtok() as an index
  int chksum_arduino = 0;
  uint8_t k = 0;

  strtokIndx = strtok(tempChars,",");      // get the first part
  array_PW_Freq[0] = atoi(strtokIndx);
  Serial.println(array_PW_Freq[0]);
  k++;

  for (uint8_t ii = 1; ii < 4; ii++) {
    strtokIndx = strtok(NULL,",");
    array_PW_Freq[ii] = atoi(strtokIndx);
    Serial.println(array_PW_Freq[ii]);
    chksum_arduino += tempChars[k];
    k++;
  }
  for (uint8_t ii = 0; ii < 4; ii++) {
    strtokIndx = strtok(NULL,",");
    array_T_on_on_Iset[ii] = atoi(strtokIndx);
    Serial.println(array_T_on_on_Iset[ii]);
    chksum_arduino += tempChars[k];
    k++;
  }
  for (uint8_t ii = 0; ii < 4; ii++) {
    strtokIndx = strtok(NULL,",");
    array_mode_ch_tel[ii] = atoi(strtokIndx);
    Serial.println(array_mode_ch_tel[ii]);
    chksum_arduino += tempChars[k];
    k++;
  }
  //array_Iset_mode_ch[4] = 0;
  //Serial.println(chksum_arduino);
}

void memReadTrigger() {
  uint8_t data[32];
  uint8_t pageNum = 8;
  // Static variables persist across non-blocking function calls
  static uint8_t last_raw = 0xFF; // sentinel, will never match first real value
  static uint8_t error_count = 0;

  success = nfc.mifareultralight_ReadPage(pageNum, data);
  if (success) {
    error_count = 0; // Reset RF fault monitor on success
    uint8_t raw      = data[0];
    uint8_t channel  = raw & 0x7F;
    bool    stim_off = raw & 0x80;

    if (raw != last_raw) {
      makeTrigger();
      if (stim_off) {
        uint16_t voltage_mV = (data[2] << 8) | data[1];
        Serial.print("Stimulation off, last channel: "); Serial.println(channel);
        Serial.print(voltage_mV); Serial.println(" mV");
      } else {
        Serial.print("Currently stimulating channel "); Serial.println(channel);
      }
      last_raw = raw;
    } else {
      error_count++;
      if (error_count >= 3) {
        Serial.println("FAULT: RF Link Lost! Attempting recovery.");
        nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
      }
    }
    
  }


}

void makeTrigger() {
  digitalWrite(13,HIGH);
  delay(2);
  digitalWrite(13, LOW);
}