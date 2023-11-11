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
//boolean memoryWritten = 0;    // Boolean to prevent multiple writings each time connection is lost
uint8_t array_PW_Freq[4]; // array 1 for PW_HB, PW_LB, F_hz_HB, F_hz_LB
uint8_t array_T_on_on[4]; // array 2 for T_on_0,T_on_1,T_on_2,ON
uint8_t array_Iset_mode_ch[4]; // array 3 for current amplitude, stimulation mode, stimulation channel
uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
bool writeResult;
bool stimProtect = 0;  
      
void setup(void) {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Serial.begin(115200);
  while (!Serial) delay(10); // for Leonardo/Micro/Zero

  Serial.println("Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Waiting for an ISO14443A Card ...");
}


void loop(void) {
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    uint8_t def_string[4] = {0,0,0,0};

    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    
    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ... value > 5 : ! successs
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
      Serial.println("Device not supported with this code");  // NT3H is 7-byte UID. Mifare Classic not supported to save the memory.
    }
    
    if (uidLength == 7)
    {
      if (stimProtect == 0) {
        // As soon as the device found try to stop stimulation
        
        writeResult = nfc.mifareultralight_WritePage(5, def_string);
        Serial.println("Write status: "); Serial.print(writeResult); Serial.print("\n");
        delay(50); // Wait to rectify error states

        nfc.reset(); nfc.begin(); // restart NFC and the device in order for changes to work in the sVNS mcu
        delay(1);
        // Experiment with reset and begin. Possibly this:
        nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
        stimProtect = 1;
      }

      // We probably have a 7 byte UID Mifare Ultralight card ...
      Serial.println("sVNS implant detected (7 byte UID)\n"); // NT3H detected
      modeSelect();
    }
  }
  else {
    Serial.println("NFC contact lost");
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
        nfc.reset(); nfc.begin();
        while (Serial.available() == 0) {
          memRead();
        }
        stimProtect = 0;
        break;
      case 3:
        nfc.reset(); nfc.begin();
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
      Serial.println("Stimulation Time On (Duty Cycle): HB, LB; 0; On/Off byte");
      break;
     case 6:
      Serial.println("Current Amplitude; Stimulation Mode; Stimulation Channel (For Single-Channel Stim); Enable/Disable telemetry");
      break;
      case 7:
      Serial.println("Page 7:");
      break;
      case 8:
      Serial.println("Page 8:");
      break;
    }
    uint8_t data[32];

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
  // Ensure the serial input buffer is empty to not write garbage into memory
  // while (Serial.available() > 0) {
  //   Serial.read();
  // }
  newData = false; // needs to be reset because is true from the previous entry before entering the function
  Serial.println("Enter the programming string in format:");
  Serial.println("<PW_HB,PW_LB,T_HB,T_LB,T_on_HB,T_on_LB,0,On/OFF,Iset,Mode,Channel_Nr,Telemetry On/Off>");
  recvWithStartEndMarkers(); // Receive command word from Serial Interface
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    arrayParse(); // Create memory integer arrays for the sVNS implant 
    // Write arrays
    writeResult = nfc.mifareultralight_WritePage(4, array_PW_Freq); Serial.println("Write result: "); Serial.println(writeResult);
    writeResult = nfc.mifareultralight_WritePage(5, array_T_on_on); Serial.println("Write result: "); Serial.println(writeResult);
    writeResult = nfc.mifareultralight_WritePage(6, array_Iset_mode_ch); Serial.println("Write result: "); Serial.println(writeResult);
    newData = false;
  }
  delay(50);
  //nfc.reset(); nfc.begin(); // restart NFC and the device in order for changes to work in the sVNS mcu
  //nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength); // read the tag again
  // memReadTrigger();
  // while(Serial.available()==0) { 
  //   memRead();
  // }
}

// Function to store the Serial Input to the string array
void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
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
    array_T_on_on[ii] = atoi(strtokIndx);
    Serial.println(array_T_on_on[ii]);
    chksum_arduino += tempChars[k];
    k++;
  }
  for (uint8_t ii = 0; ii < 4; ii++) {
    strtokIndx = strtok(NULL,",");
    array_Iset_mode_ch[ii] = atoi(strtokIndx);
    Serial.println(array_Iset_mode_ch[ii]);
    chksum_arduino += tempChars[k];
    k++;
  }
  //array_Iset_mode_ch[4] = 0;
  //Serial.println(chksum_arduino);
}

void memReadTrigger() {
  uint8_t data[32];
  uint8_t pageNum = 8;
  uint8_t channel_nr = 100; // assign a non-existing channel to make the trigger work when the first channel (0) is read 
 // bool zero_state = 0;
  while (Serial.available() == 0) {
  success = nfc.mifareultralight_ReadPage (pageNum, data); // read stimulation channel data
    if (success) // NFC page 8 read ok
    {
      if (data[0] != channel_nr) { // channel changed
          digitalWrite(13, HIGH);
          delay(2);
          Serial.println("Channel changed");
          digitalWrite(13, LOW);
      }      
      // if (channel_nr == 0 && !zero_state) { // first time read channel 0
      //   zero_state = 1;
      //   digitalWrite(13, HIGH);
      //   delay(2);
      //   digitalWrite(13, LOW);
      // }
      channel_nr = data[0]; // save channel number in memory
      Serial.println("Currently stimulating channel "); Serial.println(channel_nr);
    }
    else { // NFC page 8 read error
      Serial.println("Communication error");
      nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength); // Re-read if error in comms
    }
  }
}