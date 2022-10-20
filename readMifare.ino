/**************************************************************************/
/*! 
    @file     readMifare.pde
    @author   Adafruit Industries
	@license  BSD (see license.txt)

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
const byte numChars = 32;
char receivedChars[numChars]; // Array to store received characters from Serial Read
boolean newData = false;
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
      Serial.println("Device not supported with this code");
//	  
//      // Now we need to try to authenticate it for read/write access
//      // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
//      Serial.println("Trying to authenticate block 4 with default KEYA value");
//      uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
//	  
//	  // Start with block 4 (the first block of sector 1) since sector 0
//	  // contains the manufacturer data and it's probably better just
//	  // to leave it alone unless you know what you're doing
//      success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya);
//	  
//      if (success)
//      {
//        Serial.println("Sector 1 (Blocks 4..7) has been authenticated");
//        uint8_t data[16];
//		
//        // If you want to write something to block 4 to test with, uncomment
//		// the following line and this text should be read back in a minute
//        //memcpy(data, (const uint8_t[]){ 'a', 'd', 'a', 'f', 'r', 'u', 'i', 't', '.', 'c', 'o', 'm', 0, 0, 0, 0 }, sizeof data);
//        // success = nfc.mifareclassic_WriteDataBlock (4, data);
//
//        // Try to read the contents of block 4
//        success = nfc.mifareclassic_ReadDataBlock(4, data);
//		
//        if (success)
//        {
//          // Data seems to have been read ... spit it out
//          Serial.println("Reading Block 4:");
//          nfc.PrintHexChar(data, 16);
//          Serial.println("");
//		  
//          // Wait a bit before reading the card again
//          delay(1000);
//        }
//        else
//        {
//          Serial.println("Ooops ... unable to read the requested block.  Try another key?");
//        }
//      }
//      else
//      {
//        Serial.println("Ooops ... authentication failed: Try another key?");
//      }
    }
    
    if (uidLength == 7)
    {
      // We probably have a Mifare Ultralight card ...
      Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
	  
      // Try to read general-purpose user page 4 to 15
      for (uint8_t pageNum = 4; pageNum <= 15; pageNum++){
        Serial.print("Reading page ");
        Serial.println(pageNum);
        switch (pageNum) {
          case 4:
          Serial.println("Pulse Width: HB, LB; Pulse Frequency: HB, LB");
          Serial.println("Default: 0 1 93 61");
          break;
        case 5:
          Serial.println("Stimulation Time On (Duty Cycle): msec Byte, sec LB, sec HB; On/Off byte");
          Serial.println("Default: 0 112 2 1");
          break;
        case 6:
          Serial.println("Current Amplitude; Stimulation Mode; Stimulation Channel (For Single-Channel Stim)");
          Serial.println("Default: 63 2 0 0");
          break;
        case 8:
          Serial.println("Current Stimulation Channel (channel scanning mode)");
          break;
        }
        uint8_t data[32];
        success = nfc.mifareultralight_ReadPage (pageNum, data);
        if (success)
        {
          // Data seems to have been read ... spit it out
          nfc.PrintHexChar(data, 4);
          Serial.println("");		
        // Wait a bit before reading the card again
        //delay(1000);
        }
        else
        {
          Serial.println("Ooops ... unable to read the requested page!?");
        }
      }
      
      // Try to write the specified data to the specified page
      
      // Select the page to write
      uint8_t pageWr;
      Serial.println("\n\n1) Enter the page to write");
      receiveNumber();
      pageWr = saveNumber();
      Serial.print("\nWill write to the page: "); Serial.println(pageWr);
      confirm();

      // Create data to write to the selected page

      uint8_t dataWr[4]; // 4-byte array for data
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

      delay(1000);
    }
  }
}

void receiveNumber() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    Serial.flush();
    while (!Serial.available());
    while (Serial.available()) {
        delay(2);
        rc = Serial.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
        }
    }
}

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

void confirm() {
    // Wait any user input before proceeding
  Serial.println("\n\nSend any character via 'Send' or press Enter once to proceed");
  while (!Serial.available());
  while (Serial.available()) {
    delay(2);
  Serial.read();
  }
}
