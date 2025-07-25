/*
ROMemu - reads hex-intel records and writes them to a static RAM.
         see usage function for additional functional and test functions.
         The target is an Arduino Mega 2560, as it needs three 8-bits ports
         and some other control lines.
ToDo: - implement host isolation control to disable non-tristate signals
        from the target machine. In practice this are all signals.
      - alter the offset function behaviour to be active during data 
        generation too. Not sure this is really useful.
*/

#define VERSION "v0.11.4"

#define SERIALBUFSIZE         90
char serialBuffer[SERIALBUFSIZE];
byte setBufPointer = 0;

#define LED 13

#define CS 17            // 0x11
#define OE 16            // 0x10
#define WR 6             // 0x06
#define ARDUINOONLINE 7  // 0x07
#define RELAYPIN 8       

#define RECORDSIZE 16
#define DATARECORDTYPE 0

#define DUMPPAGE 0x0100
#define RELAYDELAY 100L
unsigned int lastEndAddress = 0;

unsigned int addressOffset = 0;

bool echo = 0;

// core routines

void setup() {
  Serial.begin(9600);
  delay(500);  
  Serial.print("ROMemu ");
  Serial.println(VERSION);
 
  pinMode(LED, OUTPUT);
  digitalWrite(RELAYPIN, HIGH);
  pinMode(RELAYPIN, OUTPUT);
  offlineMode();
  delay(1000);  
}


void loop() {
  commandCollector();
}  
 
void commandCollector() {
  if (Serial.available() > 0) {
    int inByte = Serial.read();
    if (echo) Serial.write(inByte);
    switch(inByte) {
    case '\r':
      if (!echo) break; // cr only works with echo on
    case '.':
    case '\n':
      commandInterpreter();
      clearSerialBuffer();
      setBufPointer = 0;
      break;
    default:
      serialBuffer[setBufPointer] = inByte;
      setBufPointer++;
      if (setBufPointer >= SERIALBUFSIZE) {
        Serial.println("Serial buffer overflow. Cleanup.");
        clearSerialBuffer();
        setBufPointer = 0;
      }
    }
  }
}

void commandInterpreter() {
  byte bufByte = serialBuffer[0];
  
  switch(bufByte) {
    case 'A':
    case 'a':
      ramTest();
      break;
    case 'B':
    case 'b':
      blinkPin();
      break;
    case 'C':
    case 'c':
      copyData();
      break;
    case 'D':  // dump memory
    case 'd':
      dumpMemory();
      break;
    case 'E':
    case 'e':
      generateEndHIRecord();
      break;  
    case 'F':  // set offset address
    case 'f':
      setOffset();
      break; 
    case 'G':  // set offset address
    case 'g':
      generateExorciserIRecord();
      break; 
    case 'H':  // help
    case 'h':
    case '?':  // help
//      Serial.println("F?:");
      usage();
      break; 
    case 'K':
    case 'k':
      calcChecksum(); // calculate variouos checksums
      break;
    case 'M':
    case 'm':
      modifyMem(); // modify memory location
      break;
    case 'N':
    case 'n':
      fillMemory(); // fill memory range
      break;
    case 'O':
    case 'o':
      echoManagement(); // control echo
      break;
    case 'R':
    case 'r':
      setRelay(); // set/reset relay
      break;
    case 'S':
    case 's':
      motoExorciserS1Interpreter(); // Motorola Exorciser S1 record
      break;
    case 'T':  // test ports
    case 't':
      portTest(serialBuffer[1]);
      break;
    case 'U':
    case 'u':
      setValue();
      break;
    case 'V':
    case 'v':
      viewPorts();
      break;
    case 'W':
    case 'w':
      writePin();
      break;
    case ':':  // hex-intel record
      hexIntelInterpreter();
      break;
    case ';':
      generateHexIntelRecords();
      break;
    default:
      Serial.print(bufByte);
      Serial.print(" ");
      Serial.println("unsupported");
      return;
  }
}

void usage() {
  Serial.print(F("-- ROM emulator "));
  Serial.print(VERSION);
  Serial.println(F(" --"));
  Serial.println(F("Operational commands:"));
  Serial.println(F(" Cssss-eeee-tttt - Copy data in range from ssss-eeee to tttt"));
  Serial.println(F(" D[ssss[-eeee]]- Dump memory from ssss to eeee"));
  Serial.println(F(" E             - Generate hex intel end record"));
  Serial.println(F(" Fhhhh         - AddressOffset; subtracted from hex intel addresses"));
  Serial.println(F(" Gssss-eeee    - Generate Motorola Exorciser S1 records"));
  Serial.println(F(" H             - This help text"));
  Serial.println(F(" :ssaaaatthhhh...hhcc - accepts hex intel record"));
  Serial.println(F(" ;ssss-eeee    - Generate hex intel data records"));
  Serial.println(F(" Kssss-eeee    - Generate checksums for address range"));
  Serial.println(F(" Maaaa-dd      - Modify memory"));
  Serial.println(F(" O             - Toggle echo"));
  Serial.println(F(" R[0|1]        - Switch the RESET relay"));
  Serial.println(F(" S1ccnnnndddd..ddss - accepts Motorola Exorciser S1 record"));
  Serial.println(F("Test commands:"));  
  Serial.println(F(" A             - test 32 kByte RAM with 00h, 55h. AAh and FFh patterns"));
  Serial.println(F(" Bpp           - blink pin p (in hex)"));
  Serial.println(F(" Nssss-eeee:v  - fill a memory range with a value"));
  Serial.println(F(" Tp            - exercise port p"));//      Serial.print("copy up ");

  Serial.println(F(" U             - view ports C, L, A, CS, OE, WR, ARDUINOONLINE"));
  Serial.println(F(" Wpp v         - Write pin (in hex) values 0, 1"));
  Serial.println(F(" ?             - This help text")); 
}

void setOffset() {
  if (setBufPointer == 1) {
    Serial.print("F");
    printWord(addressOffset);
    Serial.println();
    return;
  } else if (setBufPointer != 5) {
    Serial.println("ERROR F arg. size");
    clearSerialBuffer();
    return; 
  }
  addressOffset = get16BitValue(1);

  Serial.print("Address offset: ");
  printWord(addressOffset);
  Serial.println();
}

void modifyMem() {
  unsigned int address  = get16BitValue(1);
  byte newData = get8BitValue(6);
  Serial.print("M:");
  printWord(address);
  Serial.print("-");
//  printByte(newData);
//  Serial.print("/");
  onlineReadMode();
  byte oldData = readByte(address);
  printByte(oldData);
  Serial.print(" > ");
  onlineWriteMode();
  writeByte(address, newData);
  onlineReadMode();
  byte checkData = readByte(address);
  printByte(checkData);
  offlineMode();
  Serial.println();
}

void copyData() {
  unsigned int startAddress  = get16BitValue(1);
  unsigned int endAddress    = get16BitValue(6);
  unsigned int targetAddress = get16BitValue(11);
  Serial.print("C:");
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.print("-");
  printWord(targetAddress);
  Serial.println();

  unsigned int range = endAddress - startAddress;
  unsigned int te = targetAddress + range;
  unsigned int c = 0;
  onlineWriteMode();
  if (startAddress < targetAddress) {
    unsigned int s;
    unsigned int t;
    byte d;
    Serial.print("copy up ");
      printWord(startAddress);
      Serial.print("-");
      printWord(endAddress);
      Serial.print(" > ");
      printWord(targetAddress);
      Serial.print("-");
      printWord(te);
      Serial.print(" ");
      for (s = startAddress, t = targetAddress; s <= endAddress; s++, t++) {
        onlineReadMode();
        d = readByte(s);
        onlineWriteMode();
        writeByte(t, d);
        offlineMode();
        c++;
      }
      Serial.print(c, HEX);
      Serial.println("h bytes copied");
  } else {
    unsigned int e;
    byte d;
      Serial.print("copy down ");
      printWord(startAddress);  
      Serial.print("-");
      printWord(endAddress);  
      Serial.print(" > ");
      printWord(targetAddress);
      Serial.print("-");
      printWord(te);
      Serial.println();            
      for (e = endAddress; e >=  startAddress; e--, te--) {
        onlineReadMode();
        d = readByte(e);
        onlineWriteMode();
        writeByte(te, d);
        offlineMode();
        c++;
    }
      Serial.print(c, HEX);
      Serial.println("h bytes copied");
  }
  offlineMode();
}

void fillMemory() {
  unsigned int startAddress  = get16BitValue(1);
  unsigned int endAddress    = get16BitValue(6);
  unsigned int value = get8BitValue(11);
  Serial.print("N:");
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.print("-");
  printByte(value);
  Serial.println();
        
  if (startAddress > endAddress) {
    Serial.println("Error: negative range. Aborting."); 
    return;
  }
  unsigned int s;
  onlineWriteMode();
  for (s = startAddress; s <= endAddress; s++) {
    writeByte(s, value);
  }
  offlineMode();

}

void dumpMemory() {
  unsigned int startAddress;
  unsigned int endAddress;
  if (setBufPointer == 1) {
    startAddress = lastEndAddress;
    endAddress   = startAddress + DUMPPAGE;
    lastEndAddress = endAddress;
  } else if (setBufPointer == 5) {
    startAddress = get16BitValue(1);
    endAddress   = startAddress + DUMPPAGE;
    lastEndAddress = endAddress;
  } else if (setBufPointer == 10) {
    startAddress = get16BitValue(1);
    endAddress   = get16BitValue(6);
    lastEndAddress = endAddress;
  } else {
    Serial.println("unsupported"); 
  }  
  unsigned char asChars[17];
  unsigned char *asCharsP = &asChars[0];
  unsigned char positionOnLine;
  asChars[16] = 0;
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.println();
  unsigned int i;
  byte data;
  onlineReadMode();
  for (i = startAddress; i < endAddress; i++) {
    positionOnLine = i & 0x0F;
    if (positionOnLine == 0) {
      printWord(i);  // Address at start of line
      Serial.print(": ");
    }
    data = readByte(i);

    printByte(data);   // actual value in hex 
    asChars[positionOnLine] = (data >= ' ' && data <= '~') ? data : '.';
    if ((i & 0x03) == 0x03) Serial.print(" ");
    if ((i & 0x0F) == 0x0F) {
      Serial.print (" ");
      printString(asCharsP); // print the ASCII part
      Serial.println();
      if (Serial.available() > 0) {
        clearSerialBuffer();
        setBufPointer = 0;
        break;      
      }
    }
  }
  offlineMode();
  Serial.println();
}

/*
 * S1ccnnnndddd..ddss
 *   | |   |       |
 *   | |   |       sumcheck (one-complement of summantion of byte count, address and data bytes)
 *   | |   data
 *   | address
 *   byte count 
 *
 */
void generateExorciserIRecord() {
  unsigned int startAddress;
  unsigned int endAddress;
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.println();

  unsigned int i, j;
  unsigned char addressMSB, addressLSB, data;
  unsigned char sumCheckCount = 0;

  onlineReadMode();  
  for (i = startAddress; i < endAddress; i += RECORDSIZE) {
    sumCheckCount = 0;
    Serial.print("S1");
    printByte(RECORDSIZE + 3);  
    sumCheckCount += RECORDSIZE + 3;
    addressMSB = i >> 8;
    addressLSB = i & 0xFF;
    printByte(addressMSB);
    printByte(addressLSB);
    sumCheckCount += addressMSB;
    sumCheckCount += addressLSB;
    for (j = 0; j < RECORDSIZE; j++) {
      data = readByte(i + j);
      printByte(data);
      sumCheckCount += data;
    }
    printByte(0xFF & (0xFF - sumCheckCount));
    Serial.println();
  }
  offlineMode();
  Serial.println("S9030000FC");
}

/*
 * :ccnnnn00dddd..ddss
 *  | |   | |       |
 *  | |   | |       sum check
 *  | |   | data
 *  | |   record type
 *  | address
 */

void generateHexIntelRecords() {
  unsigned int startAddress;
  unsigned int endAddress;
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.println();

  unsigned int i, j;
  unsigned char addressMSB, addressLSB, data;
  unsigned char sumCheckCount = 0;

  onlineReadMode();  
  for (i = startAddress; i < endAddress; i += RECORDSIZE) {
    sumCheckCount = 0;
    Serial.print(":");
    printByte(RECORDSIZE);  
    sumCheckCount -= RECORDSIZE;
    addressMSB = i >> 8;
    addressLSB = i & 0xFF;
    printByte(addressMSB);
    printByte(addressLSB);
    sumCheckCount -= addressMSB;
    sumCheckCount -= addressLSB;
    printByte(DATARECORDTYPE);
    sumCheckCount -= DATARECORDTYPE;
    for (j = 0; j < RECORDSIZE; j++) {
      data = readByte(i + j);
      printByte(data);
      sumCheckCount -= data;
    }
    printByte(sumCheckCount);
    Serial.println();
  }
  offlineMode();
}

void generateEndHIRecord() {
  Serial.println(":00000001FF");
}

void hexIntelInterpreter() {
  unsigned int count;
  unsigned int sumCheck = 0;
  count  = getNibble(serialBuffer[1]) * (1 << 4);
  count += getNibble(serialBuffer[2]);
  sumCheck += count;                                    // add record length
  unsigned int addressLSB, addressMSB, baseAddress;
  addressMSB  = getNibble(serialBuffer[3]) * (1 << 4);
  addressMSB += getNibble(serialBuffer[4]);
  addressLSB  = getNibble(serialBuffer[5]) * (1 << 4);
  addressLSB += getNibble(serialBuffer[6]);
  sumCheck += addressMSB;                                    // add address bytes
  sumCheck += addressLSB;
  baseAddress = (addressMSB << 8) + addressLSB;
//  printWord(baseAddress - addressOffset);
//  Serial.println();
  unsigned int recordType;
  recordType  = getNibble(serialBuffer[7]) * (1 << 4);
  recordType += getNibble(serialBuffer[8]);
  if (recordType == 1) { // End of file record type
    offlineMode();
    return;
  }
  if (recordType != 0) { // ignore all but data records
    return; 
  }
  sumCheck +=  recordType;                                    // add record type
  unsigned int i, sbOffset;
  byte value;
  onlineWriteMode();
  for (i = 0; i < count; i++) {
    sbOffset = (i * 2) + 9;
    value  = getNibble(serialBuffer[sbOffset]) * (1 << 4);
    value += getNibble(serialBuffer[sbOffset + 1]); 
    sumCheck +=  value;                                    // add data bytes
    writeByte(baseAddress + i - addressOffset, value);
  }
  offlineMode();
  unsigned int sumCheckValue = 0;

  sbOffset += 2;
  sumCheckValue  = getNibble(serialBuffer[sbOffset]) * (1 << 4);
  sumCheckValue += getNibble(serialBuffer[sbOffset + 1]);
  sumCheck += sumCheckValue;                                    // add received checksum
  sumCheck &= 0xFF;
  if (sumCheck == 0) {
    printWord(baseAddress - addressOffset);
    Serial.println(" Ok.");
   } else {
     Serial.print("Sumcheck incorrect for ");
     printWord(baseAddress);
  //   Serial.print(" received: ");
  //   printByte(sumCheckValue);
  //   Serial.print(", calculated: "); 
  //   printByte(sumCheck); 
     Serial.println();
   }
}

// S1ccnnnndddd..ddss or S9030000FC end record
void motoExorciserS1Interpreter() {
  unsigned int count;
  unsigned int sumCheck = 0;
  unsigned int sumCheckReceived = 0;  
  // 1st byte should be '1', record type
  unsigned int recordType = serialBuffer[1];

  if (recordType == '9') {
    sumCheckReceived = get8BitValue(8);
    unsigned int endRecordChecksum = 0xFC;
    if (sumCheckReceived == endRecordChecksum) {
      Serial.println(F("End record Ok."));
    } else {
      Serial.print(F("Sumcheck incorrect for end record received: "));
      printByte(sumCheckReceived);
      Serial.print(F(", should be: ")); 
      printByte(endRecordChecksum);
      Serial.println();
    }
    return;                     // process 'S9'
  }
  if (recordType != '1') {
    return;                     // Ignore ap=ll record types other than 'S1'
  }
  count  = get8BitValue(2);
  sumCheck += count;                    // add record length
  count -= 3;                           // correct count to actual data bytes
  unsigned int baseAddressMSB = get8BitValue(4);
  unsigned int baseAddressLSB = get8BitValue(6);
  sumCheck += baseAddressMSB;  // MSB
  sumCheck += baseAddressLSB;  // LSB
  unsigned int baseAddress = baseAddressMSB * (1 << 8) + baseAddressLSB;
  unsigned int i, sbOffset;
  byte value;
  onlineWriteMode();
  for (i = 0; i < count; i++) {
    sbOffset = (i * 2) + 8;
    value  = get8BitValue(sbOffset);
    sumCheck +=  value;                                    // add data bytes
    writeByte(baseAddress + i - addressOffset, value);
  }
  offlineMode();
  
  sbOffset += 2;
  sumCheckReceived  = get8BitValue(sbOffset);
  sumCheck = 0xFF & (0xFF - sumCheck);
  if (sumCheck == sumCheckReceived) {
    printWord(baseAddress - addressOffset);
    Serial.println(" Ok.");
   } else {
     Serial.print("Sumcheck incorrect for ");
     printWord(baseAddress);
     Serial.print(" received: ");
     printByte(sumCheckReceived);
     Serial.print(", calculated: "); 
     printByte(sumCheck); 
     Serial.println();
   }
}

byte readByte(unsigned int address) {
  byte data = 0;
  byte addressLSB = address & 0xFF;
  byte addressMSB = address >> 8;
  PORTA = 0xFF;
  DDRA  = 0x00;
  PORTL = addressLSB;
  PORTC = addressMSB;
  digitalWrite(CS, LOW);
  digitalWrite(OE, LOW);
  data = PINA;
  digitalWrite(OE, HIGH);
  digitalWrite(CS, HIGH); 
  return data; 
}

void clearSerialBuffer() {
  byte i;
  for (i = 0; i < SERIALBUFSIZE; i++) {
    serialBuffer[i] = 0;
  }
}

void writeByte(unsigned int address, byte value) {
  byte addressLSB = address & 0xFF;
  byte addressMSB = address >> 8;
  
  PORTA = value;
  PORTL = addressLSB;
  PORTC = addressMSB;
  digitalWrite(CS, LOW);
  digitalWrite(WR, LOW);
//  delay(1);
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);
}

void printByte(byte data) {
  byte dataMSN = data >> 4;
  byte dataLSN = data & 0x0F;
  Serial.print(dataMSN, HEX);
  Serial.print(dataLSN, HEX);
}

void printWord(unsigned int data) {
  printByte(data >> 8);
  printByte(data & 0xFF);
}

void offlineMode() { // Tri-state data bus, address bus, controls, enable hostaccess to RAM
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);
  digitalWrite(OE, HIGH);
  pinMode(WR, INPUT);
  pinMode(OE, INPUT);
  pinMode(CS, INPUT);
  pinMode(ARDUINOONLINE, OUTPUT); 
  digitalWrite(ARDUINOONLINE, LOW);
  DDRA = 0x00; // 0x00 = input, 0xFF = output
  DDRC = 0x00;
  DDRL = 0x00;
}

void onlineWriteMode() { // Disable host access to RAM, enable data bus, address bus, controls
  PORTA = 0xFF; // needed for switch from tristate to output low
  DDRA  = 0xFF;  
  PORTC = 0xFF;
  DDRC  = 0xFF;
  PORTL = 0xFF;
  DDRL  = 0xFF;
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);
  digitalWrite(OE, HIGH);
  pinMode(WR, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(ARDUINOONLINE, OUTPUT); 
  digitalWrite(ARDUINOONLINE, HIGH);
}

void onlineReadMode() { // Disable host access to RAM, enable data bus, address bus, controls
  PORTA = 0xFF; // needed for switch from tristate to output low
  DDRA  = 0x00;  
  PORTC = 0xFF;
  DDRC  = 0xFF;
  PORTL = 0xFF;
  DDRL  = 0xFF;
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);
  digitalWrite(OE, HIGH);
  pinMode(WR, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(ARDUINOONLINE, OUTPUT); 
  digitalWrite(ARDUINOONLINE, HIGH);
}

void portTest(byte port) {
  byte i = 0;
  switch(port) {
    case 'A':
    case 'a':
      Serial.println("Testing PORTA");
      PORTA = 0xFF;
      DDRA  = 0xFF;
      while (1) {
        PORTA = i++;
        i &= 0xFF;
        delay(10);
      }
    break; 
    case 'C':
    case 'c':
      Serial.println("Testing PORTC");
      PORTC = 0xFF;
      DDRC  = 0xFF;
      while (1) {
        PORTC = i++;
        i &= 0xFF;
        delay(10);
      }
    break;
    case 'D':
    case 'd':
      Serial.println("Testing PORTD");
      PORTD = 0xFF;
      DDRD  = 0xFF;
      while(1) {
        PORTD = i++;
        i &= 0xFF; 
        delay(10);
      }
    break;
    case 'L':
    case 'l':    
      Serial.println("Testing PORTL");
      PORTL = 0xFF;
      DDRL  = 0xFF;
      while(1) {
        PORTL = i++; 
        i &= 0xFF;
        delay(10);
      }
    default:
    break;
    case 'H':
    case 'h':
      Serial.println("Testing PORTH");
      PORTH = 0xFF;
      DDRH  = 0xFF;
      while(1) {
        PORTH = i++;
        i &= 0xFF; 
        delay(10);
      }
    break;
    return;
   }
}

void blinkPin() {
  int pin;
  pin  = get8BitValue(1);
  
  pinMode(pin, OUTPUT);
  while(1) {
    digitalWrite(pin, !digitalRead(pin));
    delay(500);
  }
}

void setValue() {
  unsigned int startAddress;
  unsigned int endAddress;
  byte value;
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);
  value        = get8BitValue(11);
  
  onlineWriteMode();
  unsigned int i;
  for (i = startAddress; i <= endAddress; i++) {
    writeByte(i, value);
  }
  Serial.print("S:");
  printWord(startAddress);
  Serial.print("-");
  printWord(endAddress);
  Serial.print(" with ");
  printByte(value);
  Serial.println();
  offlineMode();
}

void viewPorts() {
  while(1) {
//    Serial.print(PINC, BIN);
    printBin(PINC);
    Serial.print(" ");
//    Serial.print(PINL, BIN);
    printBin(PINL);
    Serial.print(" ");
//    Serial.print(PINA, BIN);
    printBin(PINA);
    Serial.print("  ");
    Serial.print(digitalRead(CS), BIN);
    Serial.print(digitalRead(OE), BIN);
    Serial.print(digitalRead(WR), BIN);
    Serial.print("  ");
    Serial.print(digitalRead(ARDUINOONLINE), BIN);
    Serial.println();
    delay(1000);
  } 
}

void writePin() {
  unsigned char pin;
  pin  = getNibble(serialBuffer[1]) * (1 << 4);
  pin += getNibble(serialBuffer[2]);
//Serial.print(pin, HEX);
//Serial.print("  ");
//Serial.println(serialBuffer[1], HEX);
  digitalWrite(pin, (serialBuffer[4] == 0x30) ? 0 : 1);
}

void printBin(unsigned char value) {
  Serial.print((value & 0b10000000) ? "1" : "0");
  Serial.print((value & 0b01000000) ? "1" : "0");
  Serial.print((value & 0b00100000) ? "1" : "0");
  Serial.print((value & 0b00010000) ? "1" : "0");
  Serial.print((value & 0b00001000) ? "1" : "0");
  Serial.print((value & 0b00000100) ? "1" : "0");
  Serial.print((value & 0b00000010) ? "1" : "0");
  Serial.print((value & 0b00000001) ? "1" : "0");  
}

void printString(unsigned char *asCharP) {
  unsigned char i = 0;
  while(asCharP[i] != 0) {
    Serial.write(asCharP[i]); 
//    Serial.print(asCharP[i], HEX);
    i++;
  }
}

unsigned int get16BitValue(byte index) {
  byte i = index;
  unsigned address;
  address  = getNibble(serialBuffer[i++]) * (1 << 12);
  address += getNibble(serialBuffer[i++]) * (1 << 8);
  address += getNibble(serialBuffer[i++]) * (1 << 4);
  address += getNibble(serialBuffer[i++]);
  return address;
}

byte get8BitValue(byte index) {
  byte i = index;
  byte data;
  data  = getNibble(serialBuffer[i++]) * (1 << 4);
  data += getNibble(serialBuffer[i++]);
  return data;
}

int getNibble(byte myChar) {
  int nibble = myChar;
  if (nibble > 'F') nibble -= ' ';  // lower to upper case
  nibble -= '0';
  if (nibble > 9) nibble -= 7; // offset 9+1 - A
  return nibble;
}

void calcChecksum() {
  unsigned int startAddress;
  unsigned int endAddress;
  unsigned char value;
  // Cssss eeee
  startAddress = get16BitValue(1);
  endAddress   = get16BitValue(6);

  Serial.print("Checksum block ");
  Serial.print(startAddress, HEX);
  Serial.print("h - ");
  Serial.print(endAddress, HEX);
  Serial.print("h : ");
  unsigned long checkSum = blockChecksum(startAddress, endAddress);
  uint8_t andOrChecksum = andOrDiff(startAddress, endAddress);
  Serial.print(checkSum, HEX);
  Serial.print("h, ");
  Serial.print(checkSum, DEC);
  Serial.print(", ");
  Serial.print(checkSum & 0xFF, DEC);
  Serial.print(", and/or: ");
  Serial.print(andOrChecksum, DEC);  
  Serial.println();
}

unsigned int blockChecksum(unsigned long startAddress, unsigned long endAddress)
{
  onlineReadMode();
  unsigned long checksum = 0;
  for (unsigned long i=startAddress; i<=endAddress; i++) {
    checksum += readByte(i);  
  }
  offlineMode();
  return checksum;
}

uint8_t andOrDiff(unsigned long startAddress, unsigned long endAddress)
{
  onlineReadMode();
  uint8_t checksum = 0;
  for (unsigned long i=startAddress; i<=endAddress; i++) {
    checksum = (readByte(i) & checksum) - (readByte(i) | checksum); //  P = (Q OR P) - (Q AND P)
  }
  offlineMode();
  return checksum;
}

// e - toggle echo; e0 - echo off, e1 - echo on
void echoManagement() {
  if (setBufPointer == 1) {
    echo = !echo;
    printEchoState();
  } else {
    Serial.println("unsupported"); 
  }
}

void printEchoState() {
    if (echo) {
      Serial.println("echo on");
    }  
}

void relayOff() {
  digitalWrite(RELAYPIN, HIGH);
}

void relayOn() {
  digitalWrite(RELAYPIN, LOW);
}

void setRelay() {
  if (setBufPointer == 1) {             // no argument, echo state
    Serial.print(digitalRead(RELAYPIN));
  } else if (setBufPointer == 2) {      // one argument, set relay
//      Serial.print("Relay ");
    if (serialBuffer[1] == '1') {       // arg is "1", switch on
      relayOn();
//      Serial.print(!digitalRead(RELAYPIN));
    } else if (serialBuffer[1] == '0') { // arg is "0", switch off
      relayOff();
//      Serial.print(!digitalRead(RELAYPIN));      
    } else {
      Serial.print("? ");
      Serial.print(serialBuffer[1]);
    }
  } else {                              //  two or more arguments, error
    Serial.println("unsupported"); 
  }
  Serial.println();
}

#define RAMSTART 0x0000
#define RAMEND   0x7FFF

void ramTest() {
  bool firstPhase = 1;
  Serial.println("32k Byte RAM test");
  Serial.println(" Phase 1: ?? > 0x00");
  if (!ramTestPhase(firstPhase, 0, 0)) {
    return;
  }
  firstPhase = 0;
  Serial.println(" Phase 2: 0x00 > 0x55");
  if (!ramTestPhase(firstPhase, 0, 0x55)) {
    return;
  }
  Serial.println(" Phase 3: 0x55 > 0xAA");
  if (!ramTestPhase(firstPhase, 0x55, 0xAA)) {
    return;
  }
  Serial.println(" Phase 4: 0xAA > 0xFF");
  if (!ramTestPhase(firstPhase, 0xAA, 0xFF)) {
    return;
  }
  Serial.println("RAM test OK");
}

bool ramTestPhase(bool firstPhase, byte expectValue, byte newValue) {
  bool success = 0;
  for (int i = RAMSTART; i < RAMEND; i++) {
    byte returnValue;
    if (!firstPhase) {
      onlineReadMode();
      returnValue = readByte(i);
      if (returnValue != expectValue) {
        Serial.print("  memory at ");
        Serial.print(i, HEX);
        Serial.print("not expected ");
        Serial.print(expectValue, HEX);
        Serial.print(" but ");
        Serial.println(returnValue, HEX);
        offlineMode();
        return success;
      }
    }
    onlineWriteMode();
    writeByte(i, newValue);
    onlineReadMode();
    returnValue = readByte(i);
    if (returnValue != newValue) {
      Serial.print(" memory at ");
      Serial.print(i, HEX);
      Serial.print("not new ");
      Serial.print(newValue, HEX);
      Serial.print(" but ");
      Serial.println(returnValue, HEX);
      offlineMode();
      return success;
    }
  }
  offlineMode();
  success = 1;
  return success;
}
