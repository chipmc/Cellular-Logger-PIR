// FRAM Library Extensions Header File

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C(); // Init the FRAM

// Begin section
uint8_t FRAMread8(unsigned int address)  // Read 8 bits from FRAM
{
    uint8_t result;
    //Serial.println("In FRAMread8");
    result = fram.read8(address);
    return result;
}

void FRAMwrite8(unsigned int address, uint8_t value)    // Write 8 bits to FRAM
{
    fram.write8(address,value);
}

int FRAMread16(unsigned int address)
{
    long two;
    long one;
    //Read the 2 bytes from  memory.
    two = fram.read8(address);
    one = fram.read8(address + 1);
    //Return the recomposed long by using bitshift.
    return ((two << 0) & 0xFF) + ((one << 8) & 0xFFFF);
}

void FRAMwrite16(unsigned int address, int value)   // Write 16 bits to FRAM
{
    //This function will write a 2 byte (16bit) long to the eeprom at
    //the specified address to address + 1.
    //Decomposition from a long to 2 bytes by using bitshift.
    //One = Most significant -> Four = Least significant byte
    byte two = (value & 0xFF);
    byte one = ((value >> 8) & 0xFF);
    //Write the 2 bytes into the eeprom memory.
    fram.write8(address, two);
    fram.write8(address + 1, one);
}

unsigned long FRAMread32(unsigned long address)
{
    long four;
    long three;
    long two;
    long one;
    //Read the 4 bytes from memory.
    four = fram.read8(address);
    three = fram.read8(address + 1);
    two = fram.read8(address + 2);
    one = fram.read8(address + 3);
    //Return the recomposed long by using bitshift.
    return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void FRAMwrite32(int address, unsigned long value)  // Write 32 bits to FRAM
{
    //This function will write a 4 byte (32bit) long to the eeprom at
    //the specified address to address + 3.
    //Decomposition from a long to 4 bytes by using bitshift.
    //One = Most significant -> Four = Least significant byte
    byte four = (value & 0xFF);
    byte three = ((value >> 8) & 0xFF);
    byte two = ((value >> 16) & 0xFF);
    byte one = ((value >> 24) & 0xFF);
    //Write the 4 bytes into the eeprom memory.
    fram.write8(address, four);
    fram.write8(address + 1, three);
    fram.write8(address + 2, two);
    fram.write8(address + 3, one);
}


void ResetFRAM()  // This will reset the FRAM - set the version and preserve delay and sensitivity
{
    // Note - have to hard code the size here due to this issue - http://www.microchip.com/forums/m501193.aspx
    Serial.println("Resetting Memory");
    for (unsigned long i=4; i < 32768; i++) {  // Start at 4 to not overwrite debounce and sensitivity
        FRAMwrite8(i,0x0);
        if (i==8192) Serial.println(F("25% done"));
        if (i==16384) Serial.println(F("50% done"));
        if (i==(24576)) Serial.println(F("75% done"));
        if (i==32767) Serial.println(F("Done"));
    }
    FRAMwrite8(VERSIONADDR,VERSIONNUMBER);  // Reset version to match #define value for sketch
}
