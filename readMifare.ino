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
//const byte numChars = 32;     // Number of bytes for the array
const byte numChars = 13;
char receivedChars[numChars]; // Array to store received characters from Serial Read
char tempChars[numChars]; // Temporary array for parsing
boolean newData = false;      // Needed to detect new input from the Serial Mon
char array_PW_Freq[4]; // array 1 for PW_HB, PW_LB, F_hz_HB, F_hz_LB
char array_T_on_on[4]; // array 2 for T_on_0,T_on_1,T_on_2,ON
char array_Iset_mode_ch[4]; // array 3 for current amplitude, stimulation mode, stimulation channel

uint8_t dataNumber = 0;       

void setup(void) {
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
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    
    if (uidLength == 4)
    {
      // We probably have a Mifare Classic card ... 
      Serial.println("Seems to be a Mifare Classic card (4 byte UID)");
      Serial.println("Device not supported with this code");  // NT3H is 7-byte UID. Mifare Classic not supported to save the memory.
    }
    
    if (uidLength == 7)
    {
      // We probably have a 7 byte UID Mifare Ultralight card ...
      Serial.println("sVNS implant detected (7 byte UID)"); // NT3H detected
      // Wait for the command byte from the GUI
      // Need to correctly receive the command word (byte by byte?)
      while (!Serial.available());
      recvWithStartEndMarkers(); 
      parseData();
      Serial.println(array_PW_Freq);  
      Serial.println(array_T_on_on);
      Serial.println(array_Iset_mode_ch);
      confirm();
      // Break the command byte into bytes and write individually into the corresponding address

      // Try to write the specified data to the specified page
      // for (byte page_num = 4; page_num < 7; page_num++)
      //   for (byte i = 0; i < 4; i++)
      //   // array data
      //   nfc.mifareultralight_WritePage(page_num, command_byte[k]);
      //   k++;


     // Serial.print("\nWill write to the page: "); Serial.println(pageWr);
     // confirm();

     // Select the page to write
      
/*      uint8_t pageWr;
      Serial.println("\n\n1) Enter the page to write");
      receiveNumber();
      pageWr = saveNumber();


      // Create data to write to the selected page

      uint8_t dataWr[4]; // 4-byte array for the stimulation data
      Serial.println("\n\n2) Enter data");
      for (byte i = 0; i < 4; i++) {
          Serial.print("\n\nEnter byte "); Serial.println(i);
          receiveNumber();
          dataWr[i] = saveNumber();
      }

      Serial.print("\nWill write the following data page: "); 
      for (byte i = 0; i < 4; i++) {
          if (i == 3){
              Serial.print(dataWr[i], HEX); Serial.print("\n");
              continue;
        }
        Serial.print(dataWr[i], HEX); Serial.print(", ");
      }
      confirm();

      nfc.mifareultralight_WritePage(pageWr, dataWr);

*/
      // Try to read general-purpose user page 4 to 15
      for (uint8_t pageNum = 4; pageNum <= 15; pageNum++){
        Serial.print("Reading page ");
        Serial.println(pageNum);
        switch (pageNum) { // Reading stimulation parameters 
          case 4:
          Serial.println("Pulse Width: HB, LB; Pulse Frequency: HB, LB");
          break;
        case 5:
          Serial.println("Stimulation Time On (Duty Cycle): msec Byte, sec LB, sec HB; On/Off byte");
          break;
        case 6:
          Serial.println("Current Amplitude; Stimulation Mode; Stimulation Channel (For Single-Channel Stim)");
          break;
        case 8:
          Serial.println("Current Stimulation Channel (channel scanning mode)");
          break;
        }
        uint8_t data[numChars];
        success = nfc.mifareultralight_ReadPage (pageNum, data);
        if (success)
        {
          // Data seems to have been read ... spit it out
          nfc.PrintHexChar(data, 4);
          Serial.println("");		
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested page!?");
        }
      }
      

      delay(1000);
    }
  }
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

void parseData() {
  uint8_t k = 1; // local counter for the command word slicing
  char * strtokIndx;
  strtokIndx = strtok(tempChars,",");
  array_PW_Freq[0] = atoi(strtokIndx);
  for (uint8_t ii = 1; ii < 4; ii++) { 
    strtokIndx = strtok(NULL,",");
    array_PW_Freq[k] = atoi(strtokIndx);
    k++;
  }
  for (uint8_t ii = 0; ii < 4; ii++) {
    strtokIndx = strtok(NULL,",");
    array_T_on_on[k] = atoi(strtokIndx);
    k++;
  }
  for (uint8_t ii = 0; ii < 3; ii++) {
    strtokIndx = strtok(NULL,",");
    array_Iset_mode_ch[k] = atoi(strtokIndx);
    k++;
  }
  array_Iset_mode_ch[3] = 0;
}

// Function to convert the string array input to the integer
uint8_t saveNumber() {
    if (newData == true) {
        dataNumber = 0;             // new for this version
        dataNumber = atoi(receivedChars);   // new for this version
        Serial.print("\nThe following input has been received on the Serial Monitor: ");
        Serial.println(receivedChars);
        Serial.print("\nData represented as a Number ... ");
        Serial.println(dataNumber);     // new for this version
        Serial.print("\nData represented as a HEX Number ... ");
        Serial.println(dataNumber, HEX);     
        
        newData = false;
    }
    return dataNumber;
}

// Confirmation function to hold the loop
void confirm() {
    // Wait any user input before proceeding
  Serial.println("\n\nSend any character via 'Send' or press Enter once to proceed");
  while (!Serial.available());
  while (Serial.available()) {
    delay(2);
  Serial.read();
  }
}
