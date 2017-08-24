/*
 * Project Cellular-Logger-MMA8452Q
 * Description: Cellular Connected Data Logger
 * Author: Chip McClelland
 * Date:18 May 2017
 */

 // Set parameters

 //Time Period Definitions - used for debugging
 #define HOURLYPERIOD Time.hour(t)   // Normally hour(t) but can use minute(t) for debugging
 #define DAILYPERIOD Time.day(t) // Normally day(t) but can use minute(t) or hour(t) for debugging

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
 #define SOFTWARERELEASENUMBER "0.1"
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
 // volatile bool watchdogPet = false; // keeps track of when we have pet the watchdog
 volatile bool doneEnabled = true;    // This enables petting the watchdog
 bool resetPIR = true;

 // FRAM and Unix time variables
 time_t t;
 byte lastHour = 0;                   // For recording the startup values
 byte lastDate = 0;                   // These values make sure we record events if time has lapsed
 unsigned int hourlyPersonCount = 0;  // hourly counter
 unsigned int hourlyPersonCountSent = 0;  // Person count in flight to Ubidots
 unsigned int dailyPersonCount = 0;   //  daily counter
 unsigned int dailyPersonCountSent = 0; // Daily person count in flight to Ubidots
 bool dataInFlight = false;           // Tracks if we have sent data but not yet cleared it from counts until we get confirmation
 byte currentHourlyPeriod;            // This is where we will know if the period changed
 byte currentDailyPeriod;             // We will keep daily counts as well as period counts
 int countTemp = 0;                   // Will use this to see if we should display a day or hours counts

 // Battery monitor
 int stateOfCharge = 0;            // stores battery charge level value

 //Menu and Program Variables
 unsigned long lastBump = 0;         // set the time of an event
 boolean ledState = LOW;             // variable used to store the last LED status, to toggle the light
 int menuChoice=0;                   // Menu Selection
 boolean refreshMenu = true;         //  Tells whether to write the menu
 boolean inTest = false;             // Are we in a test or not
 int numberHourlyDataPoints;         // How many hourly counts are there
 int numberDailyDataPoints;          // How many daily counts are there
 const char* releaseNumber = SOFTWARERELEASENUMBER;  // Displays the release on the menu
 String RSSIdescription = "";


 void setup()
 {
   Serial.begin(9600);
   Wire.begin();                       //Create a Wire object
   Serial.println("");                 // Header information
   Serial.print(F("Cellular-Logger-PIR - release "));
   Serial.println(releaseNumber);

   pinMode(donePin,OUTPUT);       // Allows us to pet the watchdog
   attachInterrupt(wakeUpPin, watchdogISR, RISING);   // The watchdog timer will signal us and we have to respond

   pinMode(wakeUpPin,INPUT_PULLDOWN);   // This pin is active HIGH
   pinMode(intPin,INPUT);            // PIR interrupt pinMode
   pinMode(blueLED, OUTPUT);           // declare the Red LED Pin as an output
   pinMode(tmp36Shutdwn,OUTPUT);      // Supports shutting down the TMP-36 to save juice
   digitalWrite(tmp36Shutdwn, HIGH);  // Turns on the temp sensor

   Particle.subscribe("hook-response/hourly", myHandler, MY_DEVICES);      // Subscribe to the integration response event
   Particle.subscribe("hook-response/daily", myHandler, MY_DEVICES);      // Subscribe to the integration response event
   Particle.variable("RSSIdesc", RSSIdescription);
   Particle.variable("ResetCount", resetCount);
   Particle.variable("Temperature",temperatureF);
   Particle.variable("Releaase",releaseNumber);
   Particle.variable("stateOfChg", stateOfCharge);
   Particle.function("startStop", startStop);
   Particle.function("resetFRAM", resetFRAM);
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
  printSignalStrength();           // Test signal strength at startup
  StartStopTest(1);                // Default action is for the test to be running

 }

 void loop() {
   if (refreshMenu) {
       refreshMenu = 0;
       Serial.println(F("Remote Trail Counter Program Menu"));
       Serial.println(F("0 - Display Menu"));
       Serial.println(F("1 - Display status"));
       Serial.println(F("2 - Set the time zone"));
       Serial.println(F("3 - Change the sensitivitiy"));
       Serial.println(F("4 - Change the debounce"));
       Serial.println(F("5 - Reset the counter"));
       Serial.println(F("6 - Reset the memory"));
       Serial.println(F("7 - Start / stop counting"));
       Serial.println(F("8 - Dump hourly counts"));
       Serial.println(F("9 - Last 14 day's counts"));
       NonBlockingDelay(100);
   }
   if (Serial.available() >> 0) {      // Only enter if there is serial data in the buffer
       switch (Serial.read()) {          // Read the buffer
           case '0':
               refreshMenu = 1;
               break;
           case '1':   // Display Current Status Information
               Serial.print(F("Current Time:"));
               t = Time.now();
               Serial.println(Time.timeStr(t)); // Prints time t - example: Wed May 21 01:08:47 2014  // Give and take the bus are in this function as it gets the current time
               Serial.print("Reset count: ");
               Serial.println(FRAMread8(RESETCOUNT));
               stateOfCharge = int(batteryMonitor.getSoC());
               Serial.print(F("State of charge: "));
               Serial.print(stateOfCharge);
               Serial.println(F("%"));
               Serial.print(F("Hourly count: "));
               Serial.println(FRAMread16(CURRENTHOURLYCOUNTADDR));
               Serial.print(F("Daily count: "));
               Serial.println(FRAMread16(CURRENTDAILYCOUNTADDR));
               printSignalStrength();
               Serial.print("Temperature in case: ");
               Serial.print(getTemperature(0)); // Returns temp in F
               Serial.println(" degrees F");
               Serial.println("Sending to Ubidots via Webhook");
               SendEvent(1);  // Send as an hourly event
               break;
           case '2':     // Set the time zone - to be implemented
               break;
           case '3':  // Change the sensitivity

               break;
           case '4':  // Change the debounce value

               break;
           case '5':  // Reset the current counters
               Serial.println(F("Counter Reset!"));
               FRAMwrite16(CURRENTDAILYCOUNTADDR, 0);   // Reset Daily Count in memory
               FRAMwrite16(CURRENTHOURLYCOUNTADDR, 0);  // Reset Hourly Count in memory
               hourlyPersonCount = 0;
               dailyPersonCount = 0;
               break;
           case '6': // Reset FRAM Memory
               ResetFRAM();
               break;
           case '7':  // Start or stop the test
               if (inTest == 0) {
                   StartStopTest(1);
               }
               else {
                   StartStopTest(0);
                   refreshMenu = 1;
               }
               break;
           case '8':   // Dump the hourly data to the monitor
               numberHourlyDataPoints = FRAMread16(HOURLYPOINTERADDR); // Put this here to reduce FRAM reads
               Serial.print("Retrieving ");
               Serial.print(HOURLYCOUNTNUMBER);
               Serial.println(" hourly counts");
               Serial.println(F("Hour Ending -   Count  - Battery %"));
               for (int i=0; i < HOURLYCOUNTNUMBER; i++) { // Will walk through the hourly count memory spots - remember pointer is already incremented
                   unsigned int address = (HOURLYOFFSET + (numberHourlyDataPoints + i) % HOURLYCOUNTNUMBER)*WORDSIZE;
                   countTemp = FRAMread16(address+HOURLYCOUNTOFFSET);
                   if (countTemp > 0) {
                       time_t unixTime = FRAMread32(address);
                       Serial.print(Time.timeStr(unixTime));
                       Serial.print(F(" - "));
                       Serial.print(countTemp);
                       Serial.print(F("  -  "));
                       Serial.print(FRAMread8(address+HOURLYBATTOFFSET));
                       Serial.println(F("%"));
                   }
               }
               Serial.println(F("Done"));
               break;
           case '9':  // Download all the daily counts
               numberDailyDataPoints = FRAMread8(DAILYPOINTERADDR);        // Put this here to reduce FRAM reads
               Serial.println(F("Date - Count - Battery %"));
               for (int i=0; i < DAILYCOUNTNUMBER; i++) {                  // Will walk through the 30 daily count memory spots - remember pointer is already incremented
                   int address = (DAILYOFFSET + (numberDailyDataPoints + i) % DAILYCOUNTNUMBER)*WORDSIZE;      // Here to improve readabiliy - with Wrapping
                   countTemp = FRAMread16(address+DAILYCOUNTOFFSET);       // This, again, reduces FRAM reads
                   if (countTemp > 0) {                                    // Since we will step through all 30 - don't print empty results
                       Serial.print(FRAMread8(address));
                       Serial.print(F("/"));
                       Serial.print(FRAMread8(address+DAILYDATEOFFSET));
                       Serial.print(F(" - "));
                       Serial.print(countTemp);
                       Serial.print(F("  -  "));
                       Serial.print(FRAMread8(address+DAILYBATTOFFSET));
                       Serial.println(F("%"));
                   }
               }
               Serial.println(F("Done"));
               break;
           default:
               Serial.println(F("Invalid choice - try again"));
       }
       Serial.read();  // Clear the serial buffer
   }
   if (inTest == 1) {
     if(hourlyPersonCountSent && !dataInFlight) {
       Serial.print("Receipt confirmed so ");
       Serial.print(hourlyPersonCount);
       Serial.print(" minus ");
       Serial.print(hourlyPersonCountSent);
       Serial.print(" yeilds ");
       hourlyPersonCount -= hourlyPersonCountSent;    // Confirmed that count was recevied - clearing
       Serial.println(hourlyPersonCount);
       hourlyPersonCountSent = 0;
     }
     CheckForBump();
   }

   if (watchdogPet)
   {
     Serial.println("We have pet the watchdog");
     watchdogPet = false;
   }

 }

 void CheckForBump() // This is where we check to see if an interrupt is set when not asleep or act on a tap that woke the Arduino
 {
     if (digitalRead(intPin) && resetPIR = 1)    // If int2 goes High, either p/l has changed or there's been a single/double tap
     {
         Serial.println(F("It is a tap - counting"));
         lastBump = millis();    // Reset last bump timer
         t = Time.now();
         if (HOURLYPERIOD != currentHourlyPeriod) {
             LogHourlyEvent();
         }
         if (DAILYPERIOD != currentDailyPeriod) {
             LogDailyEvent();
         }
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
         resetPIR = 0;    // Can't count again until PIR goes low
     }
     else if (!digitalRead(intPin))
     {
       resetPIR = 1;    // Ensures we wait for PIR to go low before counting again
     }
 }


 void StartStopTest(boolean startTest)  // Since the test can be started from the serial menu or the Simblee - created a function
 {
     if (startTest) {
         inTest = true;
         t = Time.now();                    // Gets the current time
         currentHourlyPeriod = HOURLYPERIOD;   // Sets the hour period for when the count starts (see #defines)
         currentDailyPeriod = DAILYPERIOD;     // And the day  (see #defines)
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
         readRegister(MMA8452_ADDRESS,0x22);     // Reads the PULSE_SRC register to reset it
         Serial.println(F("Test Started"));
     }
     else {
         inTest = false;
         readRegister(MMA8452_ADDRESS,0x22);  // Reads the PULSE_SRC register to reset it
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
       currentHourlyPeriod = HOURLYPERIOD;  // Change the time period
       Serial.println(F("Hourly Event Sent"));
     }
     printSignalStrength();
 }

 bool SendEvent(bool hourlyEvent)
 {
   // Take the temperature and report to Ubidots - may set up custom webhooks later
   digitalWrite(donePin, HIGH);
   digitalWrite(donePin,LOW);     // Pet the dog so we have a full period for a response
   doneEnabled = false;           // Can't pet the dog unless we get a confirmation via Webhook Response and the right Ubidots code.
   // Serial.println("Watchdog petting disabled");
   int currentTemp = getTemperature(0);  // 0 argument for degrees F
   stateOfCharge = int(batteryMonitor.getSoC());
   String data = String::format("{\"hourly\":%i, \"daily\":%i,\"battery\":%i, \"temp\":%i}",hourlyPersonCount, dailyPersonCount, stateOfCharge, currentTemp);
   if (hourlyEvent) Particle.publish("hourly", data, PRIVATE);
   else Particle.publish("daily", data, PRIVATE);
   return 1;
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
       dailyPersonCount = 0;    // Reset and increment the Person Count in the new period
       currentDailyPeriod = DAILYPERIOD;  // Change the time period
       Serial.println(F("Daily Event Sent"));
     }
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

void myHandler(const char *event, const char *data)
{
  if (!data) {              // First check to see if there is any data
    Serial.print("No data returned from WebHook ");
    Serial.println(event);
    return;
  }
  Serial.print(event);
  Serial.print(" returned ");
  Serial.println(data);
  String response = data;   // If there is data - copy it into a String variable
  int datainResponse = response.indexOf("hourly") + 24; // Find the "hourly" field and add 24 to get to the value
  String responseCodeString = response.substring(datainResponse,datainResponse+3);  // Trim all but the value
  int responseCode = responseCodeString.toInt();  // Put this into an int for comparisons
  switch (responseCode) {   // From the Ubidots API refernce https://ubidots.com/docs/api/#response-codes
    case 200:
      Serial.println("Request successfully completed");
      doneEnabled = true;   // Successful response - can pet the dog again
      digitalWrite(donePin, HIGH);  // If an interrupt came in while petting disabled, we missed it so...
      digitalWrite(donePin, LOW);   // will pet the fdog just to be safe
      break;
    case 201:
      Serial.println("Successful request - new data point created");
      dataInFlight = false;  // clear the data in flight flag
      doneEnabled = true;   // Successful response - can pet the dog again
      digitalWrite(donePin, HIGH);  // If an interrupt came in while petting disabled, we missed it so...
      digitalWrite(donePin, LOW);   // will pet the fdog just to be safe
      break;
    case 400:
      Serial.println("Bad request - check JSON body");
      break;
    case 403:
      Serial.println("Forbidden token not valid");
      break;
    case 404:
      Serial.println("Not found - verify variable and device ID");
      break;
    case 405:
      Serial.println("Method not allowed for API endpoint chosen");
      break;
    case 501:
      Serial.println("Internal error");
      break;
    default:
      Serial.print("Ubidots Response Code: ");    // Non-listed code - generic response
      Serial.println(responseCode);
      break;
  }

}

void printSignalStrength()
{
  CellularSignal sig = Cellular.RSSI();  // Prototype for Cellular Signal Montoring
  int rssi = sig.rssi;
  int strength = map(rssi, -131, -51, 0, 5);
  Serial.print("The signal strength is: ");
  switch (strength)
  {
    case 0:
      RSSIdescription = "Poor Signal";
      break;
    case 1:
      RSSIdescription = "Low Signal";
      break;
    case 2:
      RSSIdescription = "Medium Signal";
      break;
    case 3:
      RSSIdescription = "Good Signal";
      break;
    case 4:
      RSSIdescription = "Very Good Signal";
      break;
    case 5:
      RSSIdescription = "Great Signal";
      break;
  }
  Serial.println(RSSIdescription);
}

int resetCounts(String command)   // Will reset the local counts
{
  if (command == "reset")
  {
    Serial.println(F("Counter Reset!"));
    FRAMwrite16(CURRENTDAILYCOUNTADDR, 0);   // Reset Daily Count in memory
    FRAMwrite16(CURRENTHOURLYCOUNTADDR, 0);  // Reset Hourly Count in memory
    hourlyPersonCount = 0;
    hourlyPersonCountSent = 0;                // In the off-chance there is data in flight
    dataInFlight = false;
    dailyPersonCount = 0;
    return 1;
  }
  else return 0;
}

int startStop(String command)   // Will reset the local counts
{
  if (command == "start" && !inTest)
  {
    StartStopTest(1);
    return 1;
  }
  else if (command == "stop" && inTest)
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
  if (command == "reset")
  {
    ResetFRAM();
    return 1;
  }
  else return 0;
}



int sendNow(String command) // Function to force sending data in current hour
{
  if (command == "send")
  {
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
    //watchdogPet = true;
  }
}
