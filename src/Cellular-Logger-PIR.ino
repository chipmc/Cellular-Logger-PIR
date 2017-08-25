/*
 * Project Cellular-Logger-PIR
 * Description: Cellular Connected Data Logger - Passive InfraRed Sensor
 * Author: Chip McClelland
 * Date: 25 August 2017
 */

 // Set parameters

 //These defines let me change the memory map and configuration without hunting through the whole program
 #define VERSIONNUMBER 7             // Increment this number each time the memory map is changed
 #define WORDSIZE 8                  // For the Word size
 #define PAGESIZE 4096               // Memory size in bytes / word size - 256kb FRAM
 // First Word - 8 bytes for setting global values
 #define DAILYOFFSET 2               // First word of daily counts
 #define HOURLYOFFSET 30             // First word of hourly counts (remember we start counts at 1)
 #define DAILYCOUNTNUMBER 28         // used in modulo calculations - sets the # of days stored
 #define HOURLYCOUNTNUMBER 4064      // used in modulo calculations - sets the # of hours stored - 256k (4096-14-2)
 #define VERSIONADDR 0x0             // Memory Locations By Name not Number
 #define SENSITIVITYADDR 0x1         // For the 1st Word locations
 #define DEBOUNCEADDR 0x2            // One byte for debounce (stored in cSec mult by 10 for mSec)
 #define RESETCOUNT 0x3              // This is where we keep track of how often the Electron was reset
 #define DAILYPOINTERADDR 0x4        // One byte for daily pointer
 #define HOURLYPOINTERADDR 0x5       // Two bytes for hourly pointer
 #define CONTROLREGISTER 0x7         // This is the control register acted on by both Simblee and Arduino
 //Second Word - 8 bytes for storing current counts
 #define CURRENTHOURLYCOUNTADDR 0x8  // Current Hourly Count
 #define CURRENTDAILYCOUNTADDR 0xA   // Current Daily Count
 #define CURRENTCOUNTSTIME 0xC       // Time of last count
 //These are the hourly and daily offsets that make up the respective words
 #define DAILYDATEOFFSET 1           //Offsets for the value in the daily words
 #define DAILYCOUNTOFFSET 2          // Count is a 16-bt value
 #define DAILYBATTOFFSET 4           // Where the battery charge is stored
 #define HOURLYCOUNTOFFSET 4         // Offsets for the values in the hourly words
 #define HOURLYBATTOFFSET 6          // Where the hourly battery charge is stored
 // Finally, here are the variables I want to change often and pull them all together here
 #define SOFTWARERELEASENUMBER "0.3"
 #define PARKCLOSES 19
 #define PARKOPENS 7

 // Included Libraries
 #include "Adafruit_FRAM_I2C.h"                           // Library for FRAM functions
 #include "FRAM-Library-Extensions.h"                     // Extends the FRAM Library
 #include "electrondoc.h"                                 // Documents pinout

 // Prototypes
 STARTUP(System.enableFeature(FEATURE_RESET_INFO));
 FuelGauge batteryMonitor;                              // Prototype for the fuel gauge (included in Particle core library)

 // Pin Constants
 const int intPin = D3;              // Acclerometer interrupt pin
 const int blueLED = D7;              // This LED is on the Electron itself
 const int tmp36Pin = A0;             // Simple Analog temperature sensor
 const int tmp36Shutdwn = B5;         // Can turn off the TMP-36 to save energy
 const int donePin = D6;              // Pin the Electron uses to "pet" the watchdog
 const int wakeUpPin = A7;            // This is the Particle Electron WKP pin

 // Program Variables
 int temperatureF;                    // Global variable so we can monitor via cloud variable
 int resetCount;                      // Counts the number of times the Electron has had a pin reset
 volatile bool watchdogPet = false; // keeps track of when we have pet the watchdog
 volatile bool doneEnabled = true;    // This enables petting the watchdog

 // PIR Sensor variables
 volatile bool sensorDetect = false;       // This is the flag that an interrupt is triggered

 // FRAM and Unix time variables
 time_t t;
 byte lastHour = 0;                   // For recording the startup values
 byte lastDate = 0;                   // These values make sure we record events if time has lapsed
 int hourlyPersonCount = 0;  // hourly counter
 int hourlyPersonCountSent = 0;  // Person count in flight to Ubidots
 int dailyPersonCount = 0;   //  daily counter
 int dailyPersonCountSent = 0; // Daily person count in flight to Ubidots
 bool dataInFlight = false;           // Tracks if we have sent data but not yet cleared it from counts until we get confirmation
 byte currentHourlyPeriod;            // This is where we will know if the period changed
 byte currentDailyPeriod;             // We will keep daily counts as well as period counts
 int countTemp = 0;                   // Will use this to see if we should display a day or hours counts

 // Battery monitor
 int stateOfCharge = 0;            // stores battery charge level value

 //Menu and Program Variables
 unsigned long lastBump = 0;         // set the time of an event
 boolean ledState = LOW;             // variable used to store the last LED status, to toggle the light
 boolean inTest = false;             // Are we in a test or not
 int numberHourlyDataPoints;         // How many hourly counts are there
 int numberDailyDataPoints;          // How many daily counts are there
 const char* releaseNumber = SOFTWARERELEASENUMBER;  // Displays the release on the menu
 char Signal[17];                            // Used to communicate Wireless RSSI and Description
 char* levels[6] = {"Poor", "Low", "Medium", "Good", "Very Good", "Great"};


 void setup()
 {
   Serial.begin(9600);
   Wire.begin();                       //Create a Wire object
   Serial.println("");                 // Header information
   Serial.print(F("Cellular-Logger-PIR - release "));
   Serial.println(releaseNumber);

   pinMode(donePin,OUTPUT);       // Allows us to pet the watchdog
   pinMode(wakeUpPin,INPUT_PULLDOWN);   // This pin is active HIGH
   pinMode(intPin,INPUT);            // PIR interrupt pinMode
   pinMode(blueLED, OUTPUT);           // declare the Red LED Pin as an output
   pinMode(tmp36Shutdwn,OUTPUT);      // Supports shutting down the TMP-36 to save juice
   digitalWrite(tmp36Shutdwn, HIGH);  // Turns on the temp sensor

   attachInterrupt(wakeUpPin, watchdogISR, RISING);   // The watchdog timer will signal us and we have to respond
   attachInterrupt(intPin,sensorISR,RISING);          // Will know when the PIR sensor is triggered

   Particle.subscribe("hook-response/Hourly_Count", UbidotsHandler, MY_DEVICES);      // Subscribe to the integration response event
   Particle.subscribe("hook-response/Daily_Count", UbidotsHandler, MY_DEVICES);      // Subscribe to the integration response event
   Particle.variable("HourlyCount", hourlyPersonCount);
   Particle.variable("DailyCount", dailyPersonCount);
   Particle.variable("Signal", Signal);
   Particle.variable("ResetCount", resetCount);
   Particle.variable("Temperature",temperatureF);
   Particle.variable("Release",releaseNumber);
   Particle.variable("stateOfChg", stateOfCharge);
   Particle.function("startStop", startStop);
   Particle.function("resetFRAM", resetFRAM);
   Particle.function("resetCounts",resetCounts);
   Particle.function("SendNow",sendNow);

   if (fram.begin()) {                // you can stick the new i2c addr in here, e.g. begin(0x51);
       Serial.println(F("Found I2C FRAM"));
   } else {
       Serial.println(F("No I2C FRAM found ... check your connections"));
   }

  if (FRAMread8(VERSIONADDR) != VERSIONNUMBER) {  // Check to see if the memory map in the sketch matches the data on the chip
    Serial.print(F("FRAM Version Number: "));
    Serial.println(FRAMread8(VERSIONADDR));
    Serial.read();
    Serial.println(F("Memory/Sketch mismatch! Erase FRAM? (Y/N)"));
    while (!Serial.available());
    switch (Serial.read()) {    // Give option to erase and reset memory
      case 'Y':
        ResetFRAM();
        break;
      case 'y':
        ResetFRAM();
        break;
      default:
        Serial.println(F("Cannot proceed"));
        BlinkForever();
    }
  }

  resetCount = FRAMread8(RESETCOUNT);       // Retrive system recount data from FRAMwrite8
  if (System.resetReason() == RESET_REASON_PIN_RESET)  // Check to see if we are starting from a pin reset
  {
    resetCount++;
    FRAMwrite8(RESETCOUNT,resetCount);    // If so, store incremented number - watchdog must have done This
  }
  Serial.print("Reset count: ");
  Serial.println(resetCount);

  Time.zone(-4);                   // Set time zone to Eastern USA daylight saving time
  getSignalStrength();           // Test signal strength at startup
  StartStopTest(1);                // Default action is for the test to be running

 }

 void loop() {
   if(hourlyPersonCountSent && !dataInFlight) {   // Cleared here as there could be counts coming in while "in Flight"
     hourlyPersonCount -= hourlyPersonCountSent;    // Confirmed that count was recevied - clearing
     FRAMwrite16(CURRENTHOURLYCOUNTADDR, hourlyPersonCount);  // Load Hourly Count to memory
     hourlyPersonCountSent = 0;
   }
   if(dailyPersonCountSent && !dataInFlight) {
     hourlyPersonCount -= hourlyPersonCountSent;    // Confirmed that count was recevied - clearing both hourly and daily counts
     FRAMwrite16(CURRENTHOURLYCOUNTADDR, hourlyPersonCount);  // Load Hourly Count to memory
     hourlyPersonCountSent = 0;
     dailyPersonCount -= dailyPersonCountSent;
     FRAMwrite16(CURRENTDAILYCOUNTADDR,hourlyPersonCount);
     dailyPersonCountSent = 0;
   }
   if (sensorDetect && inTest) {
     recordCount();
   }
   if ((Time.hour() != currentHourlyPeriod) && hourlyPersonCount) {  // Spring into action each hour on the hour as long as we have counts
     LogHourlyEvent();
   }
   if (Time.day() != currentDailyPeriod) {
     LogDailyEvent();
   }
 }

 void recordCount() // This is where we check to see if an interrupt is set when not asleep or act on a tap that woke the Arduino
 {
   Serial.println(F("It is a tap - counting"));
   sensorDetect = false;      // Reset the flag
   lastBump = millis();    // Reset last bump timer
   t = Time.now();
   hourlyPersonCount++;                    // Increment the PersonCount
   FRAMwrite16(CURRENTHOURLYCOUNTADDR, hourlyPersonCount);  // Load Hourly Count to memory
   dailyPersonCount++;                    // Increment the PersonCount
   FRAMwrite16(CURRENTDAILYCOUNTADDR, dailyPersonCount);   // Load Daily Count to memory
   FRAMwrite32(CURRENTCOUNTSTIME, t);   // Write to FRAM - this is so we know when the last counts were saved
   Serial.print(F("Hourly: "));
   Serial.print(hourlyPersonCount);
   Serial.print(F(" Daily: "));
   Serial.print(dailyPersonCount);
   Serial.print(F("  Time: "));
   Serial.println(Time.timeStr(t)); // Prints time t - example: Wed May 21 01:08:47 2014
   ledState = !ledState;              // toggle the status of the LEDPIN:
   digitalWrite(blueLED, ledState);    // update the LED pin itself
 }


 void StartStopTest(boolean startTest)  // Since the test can be started from the serial menu or the Simblee - created a function
 {
     if (startTest) {
         inTest = true;
         t = Time.now();                    // Gets the current time
         currentHourlyPeriod = Time.hour();   // Sets the hour period for when the count starts (see #defines)
         currentDailyPeriod = Time.day();     // And the day  (see #defines)
         // Deterimine when the last counts were taken check when starting test to determine if we reload values or start counts over
         time_t unixTime = FRAMread32(CURRENTCOUNTSTIME);
         lastHour = Time.hour(unixTime);
         lastDate = Time.day(unixTime);
         dailyPersonCount = FRAMread16(CURRENTDAILYCOUNTADDR);  // Load Daily Count from memory
         hourlyPersonCount = FRAMread16(CURRENTHOURLYCOUNTADDR);  // Load Hourly Count from memory
         if (currentDailyPeriod != lastDate) {
             LogHourlyEvent();
             LogDailyEvent();
         }
         else if (currentHourlyPeriod != lastHour) {
             LogHourlyEvent();
         }
         Serial.println(F("Test Started"));
     }
     else {
         inTest = false;
         t = Time.now();
         FRAMwrite16(CURRENTDAILYCOUNTADDR, dailyPersonCount);   // Load Daily Count to memory
         FRAMwrite16(CURRENTHOURLYCOUNTADDR, hourlyPersonCount);  // Load Hourly Count to memory
         FRAMwrite32(CURRENTCOUNTSTIME, t);   // Write to FRAM - this is so we know when the last counts were saved
         hourlyPersonCount = 0;        // Reset Person Count
         dailyPersonCount = 0;         // Reset Person Count
         Serial.println(F("Test Stopped"));
     }
 }

 void LogHourlyEvent() // Log Hourly Event()
 {
     time_t LogTime = FRAMread32(CURRENTCOUNTSTIME);     // This is the last event recorded - this sets the hourly period
     unsigned int pointer = (HOURLYOFFSET + FRAMread16(HOURLYPOINTERADDR))*WORDSIZE;  // get the pointer from memory and add the offset
     LogTime -= (60*Time.minute(LogTime) + Time.second(LogTime)); // So, we need to subtract the minutes and seconds needed to take to the top of the hour
     FRAMwrite32(pointer, LogTime);   // Write to FRAM - this is the end of the period
     FRAMwrite16(pointer+HOURLYCOUNTOFFSET,hourlyPersonCount);
     stateOfCharge = int(batteryMonitor.getSoC());
     FRAMwrite8(pointer+HOURLYBATTOFFSET,stateOfCharge);
     unsigned int newHourlyPointerAddr = (FRAMread16(HOURLYPOINTERADDR)+1) % HOURLYCOUNTNUMBER;  // This is where we "wrap" the count to stay in our memory space
     FRAMwrite16(HOURLYPOINTERADDR,newHourlyPointerAddr);
     if (SendEvent(1))
     {
       hourlyPersonCountSent = hourlyPersonCount; // This is the number that was sent to Ubidots - will be subtracted once we get confirmation
       dataInFlight = true; // set the data in flight flag
       currentHourlyPeriod = Time.hour();  // Change the time period
       Serial.println(F("Hourly Event Sent"));
     }
 }

 void LogDailyEvent() // Log Daily Event()
 {
     time_t LogTime = FRAMread32(CURRENTCOUNTSTIME);// This is the last event recorded - this sets the daily period
     int pointer = (DAILYOFFSET + FRAMread8(DAILYPOINTERADDR))*WORDSIZE;  // get the pointer from memory and add the offset
     FRAMwrite8(pointer,Time.month(LogTime)); // The month of the last count
     FRAMwrite8(pointer+DAILYDATEOFFSET,Time.day(LogTime));  // Write to FRAM - this is the end of the period  - should be the day
     FRAMwrite16(pointer+DAILYCOUNTOFFSET,dailyPersonCount);
     stateOfCharge = batteryMonitor.getSoC();
     FRAMwrite8(pointer+DAILYBATTOFFSET,stateOfCharge);
     byte newDailyPointerAddr = (FRAMread8(DAILYPOINTERADDR)+1) % DAILYCOUNTNUMBER;  // This is where we "wrap" the count to stay in our memory space
     FRAMwrite8(DAILYPOINTERADDR,newDailyPointerAddr);
     if (SendEvent(0))
     {
       dailyPersonCountSent = dailyPersonCount; // This is the number that was sent to Ubidots - will be subtracted once we get confirmation
       dataInFlight = true; // set the data in flight flag
       currentDailyPeriod = Time.day();  // Change the time period
       Serial.println(F("Daily Event Sent"));
     }
 }

 bool SendEvent(bool hourlyEvent)
 {
   // Take the temperature and report to Ubidots - may set up custom webhooks later
   digitalWrite(donePin, HIGH);
   digitalWrite(donePin,LOW);     // Pet the dog so we have a full period for a response
   doneEnabled = false;           // Can't pet the dog unless we get a confirmation via Webhook Response and the right Ubidots code.
   int currentTemp = getTemperature(0);  // 0 argument for degrees F
   char data[256];                                         // Store the date in this character array - not global
   snprintf(data, sizeof(data), "{\"hourly\":%i, \"daily\":%i,\"battery\":%i, \"temp\":%i}",hourlyPersonCount, dailyPersonCount, stateOfCharge, currentTemp);
   if (hourlyEvent) Particle.publish("Hourly_Count", data, PRIVATE);
   else Particle.publish("Daily_Count", data, PRIVATE);
   return 1;
 }

 void UbidotsHandler(const char *event, const char *data)  // Looks at the response from Ubidots - Will reset Photon if no successful response
 {
   // Response Template: "{{hourly.0.status_code}}"
   if (!data) {                                            // First check to see if there is any data
     Particle.publish("UbidotsResp", "No Data");
     return;
   }
   int responseCode = atoi(data);                          // Response is only a single number thanks to Template
   if ((responseCode == 200) || (responseCode == 201))
   {
     Particle.publish("UbidotsHook","Success");
     Serial.println("Request successfully completed");
     dataInFlight = false;                                 // Data has been received
     doneEnabled = true;                                   // Successful response - can pet the dog again
     digitalWrite(donePin, HIGH);                          // If an interrupt came in while petting disabled, we missed it so...
     digitalWrite(donePin, LOW);                           // will pet the dog just to be safe
   }
   else Particle.publish("UbidotsHook", data);             // Publish the response code
 }

void NonBlockingDelay(int millisDelay)  // Used for a non-blocking delay
{
    unsigned long commandTime = millis();
    while (millis() <= millisDelay + commandTime) {
      Particle.process();
    }
    return;
}

void BlinkForever() // When something goes badly wrong...
{
    Serial.println(F("Error - Cannot Proceed"));
    while(1) {
        digitalWrite(blueLED,HIGH);
        delay(200);
        digitalWrite(blueLED,LOW);
        delay(200);
    }
}

void getSignalStrength()
{
    CellularSignal sig = Cellular.RSSI();  // Prototype for Cellular Signal Montoring
    int rssi = sig.rssi;
    int strength = map(rssi, -131, -51, 0, 5);
    sprintf(Signal, "%s: %d", levels[strength], rssi);
}

int startStop(String command)   // Will reset the local counts
{
  if (command == "1" && !inTest)
  {
    StartStopTest(1);
    return 1;
  }
  else if (command == "0" && inTest)
  {
    StartStopTest(0);
    return 1;
  }
  else
  {
    Serial.print("Got here but did not work: ");
    Serial.println(command);
    return 0;
  }
}

int resetFRAM(String command)   // Will reset the local counts
{
  if (command == "1")
  {
    ResetFRAM();
    return 1;
  }
  else return 0;
}

int resetCounts(String command)   // Resets the current hourly and daily counts
{
  if (command == "1")
  {
    FRAMwrite16(CURRENTDAILYCOUNTADDR, 0);   // Reset Daily Count in memory
    FRAMwrite16(CURRENTHOURLYCOUNTADDR, 0);  // Reset Hourly Count in memory
    hourlyPersonCount = 0;                    // Reset count variables
    dailyPersonCount = 0;
    hourlyPersonCountSent = 0;                // In the off-chance there is data in flight
    dataInFlight = false;
    return 1;
  }
  else return 0;
}

int sendNow(String command) // Function to force sending data in current hour
{
  if (command == "1")
  {
    getSignalStrength();
    LogHourlyEvent();
    return 1;
  }
  else return 0;
}


 int getTemperature(bool degC)
{
  //getting the voltage reading from the temperature sensor
  int reading = analogRead(tmp36Pin);

  // converting that reading to voltage, for 3.3v arduino use 3.3
  float voltage = reading * 3.3;
  voltage /= 4096.0;        // This is different than the Arduino where there are only 1024 steps

  // now print out the temperature
  int temperatureC = int(((voltage - 0.5) * 100));  //converting from 10 mv per degree with 500 mV offset to degrees ((voltage - 500mV) times 100) - 5 degree calibration
  // now convert to Fahrenheit
  temperatureF = int((temperatureC * 9.0 / 5.0) + 32.0);

  if (degC)
  {
      return temperatureC;
  }
  else
  {
      return temperatureF;
  }
}

void watchdogISR()
{
  if (doneEnabled)
  {
    digitalWrite(donePin, HIGH);
    digitalWrite(donePin, LOW);
    watchdogPet = true;
  }
}

void sensorISR()
{
  sensorDetect = true;  // sets the sensor flag for the main loop
}
