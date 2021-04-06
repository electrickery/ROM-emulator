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

#define VERSION "v0.6"

#define SERIALBUFSIZE         80
char serialBuffer[SERIALBUFSIZE];
byte setBufPointer = 0;

#define LED 13

#define CS 17            // 0x11
#define OE 16            // 0x10
#define WR 6             // 0x06
#define ARDUINOONLINE 7  // 0x07

#define RECORDSIZE 16
#define DATARECORDTYPE 0

#define DUMPPAGE 0x0100
unsigned int lastEndAddress = 0;

unsigned int addressOffset = 0;
// core routines

void setup() {
  Serial.begin(9600);
  Serial.print("ROMemu ");
  Serial.println(VERSION);
 
  pinMode(LED, OUTPUT);
  offlineMode();
  delay(1000);  
}


void loop() {
  commandCollector();
}  
 


void commandCollector() {
  if (Serial.available() > 0) {
    int inByte = Serial.read();
    switch(inByte) {
    case '.':
//    case '\r':
    case '\n':
      commandInterpreter();
      clearSerialBuffer();
      setBufPointer = 0;
      break;
    case '\r':
      break;  // ignore carriage return
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
      generateEndRecord();
      break;  
    case 'F':  // set offset address
    case 'f':
      setOffset();
      break; 
    case 'H':  // help
    case 'h':
    case '?':  // help
//      Serial.println("F?:");
      usage();
      break; 
    case 'S':
    case 's':
      setValue(); // fill memory range with a value
      break;
    case 'T':  // test ports
    case 't':
      portTest(serialBuffer[1]);
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
      generateDataRecords();
      break;
    default:
      Serial.print(bufByte);
      Serial.print(" ");
      Serial.println("unsupported");
      return;
  }
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
//  addressOffset  = getNibble(serialBuffer[1]) * (1 << 12);
//  addressOffset += getNibble(serialBuffer[2]) * (1 << 8);
//  addressOffset += getNibble(serialBuffer[3]) * (1 << 4);
//  addressOffset += getNibble(serialBuffer[4]);
  Serial.print("Address offset: ");
  printWord(addressOffset);
  Serial.println();
}

int getNibble(unsigned char myChar) {
  int nibble = myChar;
  if (nibble > 'F') nibble -= ' ';  // lower to upper case
  nibble -= '0';
  if (nibble > 9) nibble -= 7; // offset 9+1 - A
  return nibble;
}

void copyData() {
  unsigned int startAddress  = get16BitValue(1);
  unsigned int endAddress    = get16BitValue(6);
  unsigned int targetAddress = get16BitValue(11);
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
//      Serial.print("copy up ");
//      printWord(s);
//      Serial.print(" > ");
//      printWord(t);
//      Serial.println();
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
//      Serial.print("copy down ");
//      printWord(e);  
//      Serial.print(" > ");
//      printWord(te);
//      Serial.println();          
    }
      Serial.print(c, HEX);
      Serial.println("h bytes copied");
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
  unsigned int i, data;
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

void generateDataRecords() {
  unsigned int startAddress;
  unsigned int endAddress;
    startAddress = get16BitValue(1);
    endAddress   = get16BitValue(6);
//  startAddress  = getNibble(serialBuffer[1]) * (1 << 12);
//  startAddress += getNibble(serialBuffer[2]) * (1 << 8);
//  startAddress += getNibble(serialBuffer[3]) * (1 << 4);
//  startAddress += getNibble(serialBuffer[4]);
//  endAddress  = getNibble(serialBuffer[6]) * (1 << 12);
//  endAddress += getNibble(serialBuffer[7]) * (1 << 8);
//  endAddress += getNibble(serialBuffer[8]) * (1 << 4);
//  endAddress += getNibble(serialBuffer[9]);
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

void generateEndRecord() {
  Serial.println(":00000001FF");
}

void hexIntelInterpreter() {
  unsigned int count;
  unsigned int sumCheck = 0;
  count  = getNibble(serialBuffer[1]) * (1 << 4);
  count += getNibble(serialBuffer[2]);
  sumCheck += count;
  unsigned int addressLSB, addressMSB, baseAddress;
  addressMSB  = getNibble(serialBuffer[3]) * (1 << 4);
  addressMSB += getNibble(serialBuffer[4]);
  addressLSB  = getNibble(serialBuffer[5]) * (1 << 4);
  addressLSB += getNibble(serialBuffer[6]);
  sumCheck += addressMSB;
  sumCheck += addressLSB;
  baseAddress = (addressMSB << 8) + addressLSB;
  printWord(baseAddress - addressOffset);
  Serial.println();
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
  sumCheck +=  recordType;
  unsigned int i, sbOffset;
  unsigned int value;
  onlineWriteMode();
  for (i = 0; i < count; i++) {
    sbOffset = (i * 2) + 9;
    value  = getNibble(serialBuffer[sbOffset]) * (1 << 4);
    value += getNibble(serialBuffer[sbOffset + 1]); 
    sumCheck +=  value;
    writeByte(baseAddress + i - addressOffset, value);
//    printWord(baseAddress + i - addressOffset);
//    Serial.print(": ");
//    printByte(value);
//    Serial.println();
  }
  offlineMode();
  unsigned sumCheckValue;
  sbOffset += 2;
  sumCheckValue  = getNibble(serialBuffer[sbOffset]) * (1 << 4);
  sumCheckValue += getNibble(serialBuffer[sbOffset + 1]);
  sumCheck += sumCheckValue;
  sumCheck &= 0xFF;
  if (sumCheck != 0) {
    Serial.print("Sumcheck incorrect: ");
    printByte(sumCheck); 
  }
}

unsigned int readByte(unsigned int address) {
  unsigned int data = 0;
  unsigned int addressLSB = address & 0xFF;
  unsigned int addressMSB = address >> 8;
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

void writeByte(unsigned int address, unsigned int value) {
  unsigned int addressLSB = address & 0xFF;
  unsigned int addressMSB = address >> 8;
  
  PORTA = value;
  PORTL = addressLSB;
  PORTC = addressMSB;
  digitalWrite(CS, LOW);
  digitalWrite(WR, LOW);
//  delay(1);
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);
}

void printByte(unsigned char data) {
  unsigned char dataMSN = data >> 4;
  unsigned char dataLSN = data & 0x0F;
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

void usage() {
  Serial.print("-- ROM emulator ");
  Serial.print(VERSION);
  Serial.println(" command set");
  Serial.println("Operational commands:");
  Serial.println("Cssss-eeee-tttt - Copy data in range from ssss-eeee to tttt");
  Serial.println("D[ssss[-eeee]]- Dump memory from ssss to eeee");
  Serial.println("Fhhhh         - AddressOffset; subtracted from hex intel addresses");
  Serial.println("H             - This help text");
  Serial.println(":ssaaaatthhhh...hhcc - accepts hex intel record");
  Serial.println(";ssss-eeee    - Generate hex intel data records");
  Serial.println("E             - Generate hex intel end record");
  Serial.println("Test commands");  
  Serial.println("Bpp           - blink pin p (in hex)");
  Serial.println("Sssss-eeee:v  - fill a memory range with a value");
  Serial.println("Tp            - exercise port p");
  Serial.println("V             - view ports C, L, A, CS, OE, WR, ARDUINOONLINE");
  Serial.println("Wpp v         - Write pin (in hex) values 0, 1");
  Serial.println("?             - This help text"); 
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
  pin  = getNibble(serialBuffer[1]) * (1 << 4);
  pin += getNibble(serialBuffer[2]);
  
  pinMode(pin, OUTPUT);
  while(1) {
    digitalWrite(pin, !digitalRead(pin));
    delay(500);
  }
}

void setValue() {
  unsigned int startAddress;
  unsigned int endAddress;
  unsigned char value;
    startAddress = get16BitValue(1);
    endAddress   = get16BitValue(6);
//  startAddress  = getNibble(serialBuffer[1]) * (1 << 12);
//  startAddress += getNibble(serialBuffer[2]) * (1 << 8);
//  startAddress += getNibble(serialBuffer[3]) * (1 << 4);
//  startAddress += getNibble(serialBuffer[4]);
//  endAddress  = getNibble(serialBuffer[6]) * (1 << 12);
//  endAddress += getNibble(serialBuffer[7]) * (1 << 8);
//  endAddress += getNibble(serialBuffer[8]) * (1 << 4);
//  endAddress += getNibble(serialBuffer[9]);
  value  = getNibble(serialBuffer[11]) * (1 << 4);
  value += getNibble(serialBuffer[12]);
  
  onlineWriteMode();
  unsigned int i;
  for (i = startAddress; i < endAddress; i++) {
    writeByte(i, value);
  }
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
