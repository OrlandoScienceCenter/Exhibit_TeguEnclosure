/* 
 
 Orlando Science Center - Automated Tegu Enclosure Control Program
 
 This controls all the things for the tegu enclosure, and allows
 a webpage to make changes and provide feedback
 
 Author:    MKing - hybridsix
 
 Some code examples and much help was derrived from the tutorial on 
 http://startingelectronics.com , Written by W.A. Smith
 
 A big thanks to darkmoonsinger for late night code reviews
 
 Lots of code snippits and ideas obtained through examples provided with libraries
 
[[J]]: Look for comments from me prefaced with [[J]]. -(-*
 
 
 */

/*************************************************************************************************
 *                                         Libraries                                              *
 **************************************************************************************************/

#include <Wire.h>              // I2C Library
#include <SPI.h>               // SPI Library 
#include <Ethernet.h>          // Ethernet Library
#include <OneWireLite.h>           // One Wire Comms LIbrary 
#include <DallasTemperatureLite.h> // Dallas Temp Sensor Library
#include <DS1307RTC.h>         // DS1307 Real Time Clock Library
#include <TimeLite.h>              // TIme library for easier workings with time and dates
#include <SD.h>                // SD Card Library
#include <ServoLite.h>             // Servo Library  
#include <sht1xaltLite.h>          // New Library for SHT1x Sensor - works much better. 

/*************************************************************************************************
 *                                      SHT1x Settings                                            *
 **************************************************************************************************/

// Set these to whichever pins you connected the SHT1x to:
#define dataPin A1
#define clockPin A2
// Set this number larger to slow down communication with the SHT1x, which may
// be necessary if the wires between the Arduino and the SHT1x are long:
#define clockPulseWidth 1
#define supplyVoltage sht1xalt::VOLTAGE_5V
// If you want to report tempSHT1xerature units in Fahrenheit instead of Celcius, Change which line is commented/uncomented 
//#define temperatureUnits sht1xalt::UNITS_CELCIUS
#define temperatureUnits sht1xalt::UNITS_FAHRENHEIT
sht1xalt::Sensor sensor( dataPin, clockPin, clockPulseWidth, supplyVoltage, temperatureUnits );


/*************************************************************************************************
 *                                  Ethernet/HTTP Settings                                        *
 **************************************************************************************************/

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   40

//MAC Address
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0E, 0x40, 0x64, }; // Sets the Mac address of the program to that of the device. 
//Change these to real values
IPAddress ip(10,1,1,100); // Sets the manual IP address of the device. Change to real values
EthernetServer server(80);  // Create Server at port 8088 [[J]]: Do you mean 80 or 8088?
File webFile;
char HTTP_req[REQ_BUF_SZ] = {0}; // Buffered HTTP request stored as null terminated string
char req_index = 0;              // Index into HTTP_req buffer

/*************************************************************************************************
 *                                  Dallas 1 Wire Sensor Settings                                 *
 **************************************************************************************************/
#define ONE_WIRE_BUS A0               // Where is the data pin plugged in?
#define TEMPERATURE_PRECISION 9       // How many bits of temperature precision
OneWire oneWire(ONE_WIRE_BUS);        // Setup a oneWire instance to communicate with any OneWire devices 
DallasTemperature dSensors(&oneWire);
DeviceAddress dThermometer { 0x28, 0xA3, 0x27, 0x23, 0x05, 0x00, 0x00, 0x7F };//28A327230500007F; // Device address


/*************************************************************************************************
 *                                        Other Pin Defines                                       *
 **************************************************************************************************/
// These were done as defines instead of taking up an int to save space early on. 
#define T5_1 9
#define T5_2 8
#define MVL 7
#define AUTO_ENABLE_PIN 6
#define SERVICE_ENABLE_PIN 5
// Pin 4 is for SD Card  [[J]]: Why isn't this defined here?
#define PUMP 3
#define FAN 2
//#define A3 
//A4 SDA    I2C/TWI  [[J]]: Necessary?
//A5 SCL    I2C/TWI
Servo damperServos;
/*************************************************************************************************
 *                                   Set Global Variables                                         *
 **************************************************************************************************/

//[[J]]: Just a note- the lower-scoped you can make your vars, the less memory you use and 
//memory fragmentation you experience - locals sit in registers unless it runs out of registers, and only
//then does it get pushed on the stack

#define OFF_STATE 0   //[[J]]: fixing magic numbers
#define ON_STATE 1
#define SERVICE_STATE 2

#define INDEX_FILENAME "index3.htm"

#define DAMPERS_OPEN 0
#define DAMPERS_CLOSED 90

long lastReadingTime = 0;
float tempSHT1x;
float rhSHT1x;
float tempDallas1;
tmElements_t tm; //[[J]]: Explanatory comment?
byte targetTemp;
byte targetRH;
byte hysDrift; //[[J]]: A comment on what this is might be appropriate here
boolean hysActive;
byte dayStartTime;  //[[J]]: Might be worth defining default here
byte nightStartTime;
boolean ventMode;          // Ventilation mode - 0 Recirculate, 1 vent/cooling [[J]]: Make this a byte, to match use?
/*
  #define eTargetTemp 1  // EEPROM Defines - TBD  [[J]]: Of course, comment this well.
 #define eTargetRH   2
 #define eDayStartTime 3
 #define eNightStartTime 4 
 */

int main(void) {
  //void setup()

  /*************************************************************************************************
   *                                              SETUP                                             *
   **************************************************************************************************/
  init();
  {

    //Set pin modes
    pinMode (T5_1, OUTPUT);
    pinMode (T5_2, OUTPUT);
    pinMode (MVL, OUTPUT);
    pinMode (PUMP, OUTPUT);
    pinMode (FAN, OUTPUT);
    pinMode (AUTO_ENABLE_PIN, INPUT);
    pinMode (SERVICE_ENABLE_PIN, INPUT); 

    // Initialize the SD Card
    pinMode(10, OUTPUT);        // Temporarily Disable the Ethernet Chip to select the SD Card [[J]]: Why is this a magic number?
    digitalWrite(10, HIGH);
    SD.begin(4);             // Initialize SD Card [[J]]: magic number

    // Start Sensiron SHT1x Sensor(s)
    sensor.configureConnection();
    sensor.softReset();  // Reset the SHT1x, in case the Arduino was reset during communication:

    // Initialize the Dallas OneWire Sensors
    dSensors.begin();
    dSensors.setResolution(dThermometer, TEMPERATURE_PRECISION);
  
    //Sets up servos to be controlled. One output going to three servos.
    damperServos.attach(6); 

    // SERIAL
      Serial.begin(9600);                  // Serial for debugging. Might need to drop the serial to save space. 
    // [[J]]: Just as an aside, you can leave the serial commands and surround them with #if/#endif

    // Starts ethernet and Server
    Ethernet.begin (mac, ip);            //Initialize Ethernet device
    server.begin();                      //start to listen for clients

    // Target Temperature ane RH Initial Setpoints
    targetTemp = 85;      // Degrees Farenheit
    targetRH = 85;        // % Relative Humidity
    hysActive = 0;          // Sets hysteresis to off - allows function to operate
    hysDrift = 2;        // Number of degrees to drift over/under the setpoints

    // Time Setpoints
    dayStartTime = 8;
    nightStartTime = 14;
  }

  /*************************************************************************************************
   *                                              LOOP                                              *
   **************************************************************************************************/
  //void loop(){
  while (1) {
    delay (100);
    byte systemMode;

    //[[J]]: This saved 8 bytes. Not a lot, but something. Unless there's a reason have it in each branch.
    getSensorData(); // Pull all the sensor data. No returns, all values plugged into global vars
    if (digitalRead(AUTO_ENABLE_PIN)){
      systemMode = ON_STATE;
      tempRegulation();     // Checks temp, opens/closes dampers as necessary
      if(isDay()){            // Check Day/Night States, and do the appropriate on/offs of equipment     
        //Serial.println("DayMode");
        dayMode();          // Day mode power settings 
      } else {
        //Serial.println("NightMode");
        nightMode();        // Night mode power settings 
      } 
    } else if (digitalRead(SERVICE_ENABLE_PIN)){
      systemMode = SERVICE_STATE;
      serviceMode();
    } else {
      systemMode = OFF_STATE; 
      offMode();
    }

    listenForEthernetClients();          /// Listens for HTTP client requests
  }
}


/*************************************************************************************************
 *                                        Sensor Data Grab                                        *
 **************************************************************************************************/
void getSensorData() { // check for a reading no more than once a second.
  if (millis() - lastReadingTime > 1000){
//
    dSensors.requestTemperatures();            // Send the Command to get the temperatures
    tempDallas1 = dSensors.getTempF(dThermometer);  // Request temperature back in Farenheit (easy to change to celcius..)
    sensor.measure(tempSHT1x, rhSHT1x);          //SHT1x reading
    RTC.read(tm);                           //RTC Reading
    lastReadingTime = millis();
  }
} 




/*************************************************************************************************
 *                                     Day/ Night mode check                                      *
 **************************************************************************************************/
boolean isDay() { // is it day or night?
  // Serial.print("isDay  ");
  // Serial.println((tm.Hour >= dayStartTime) && (tm.Hour <= nightStartTime));
  return ((tm.Hour >= dayStartTime) && (tm.Hour <= nightStartTime));
}




/*************************************************************************************************
 *                              Ethernet/ HTTP Request Processing                                 *
 **************************************************************************************************/
void listenForEthernetClients() {
  EthernetClient client = server.available();  // try to get client

  if (client) {  // got client?
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {   // client data available to read
        char c = client.read(); // read 1 byte (character) from client
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }
        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if ('\n' == c && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          // remainder of header follows below, depending on if
          // web page or XML page is requested
          // Ajax request - send XML file
          if (strstr(HTTP_req, "ajax_inputs")) {
            // send rest of HTTP header
            // [[J]]: Memory savings of 8B if concatenated with next line, but readability decreases
            client.println("Content-Type: text/xml");
            client.println("Connection: keep-alive"); 
            client.println();
            setEnviroControls();  //[[J]]: Why is this in the middle of this?
            // send XML file containing input states
            XML_response(client);
          } else {  // web page request
            // send rest of HTTP header
            client.println("Content-Type: text/html"); //[[J]]: Oddly enough, no memory savings here
            client.println("Connection: keep-alive");
            client.println();
            // send web page
            webFile = SD.open(INDEX_FILENAME);        // open web page file
            if (webFile) {
              while(webFile.available()) {
                client.write(webFile.read()); // send web page to client
              }
              webFile.close();
            }
          }
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          memset(HTTP_req, '0', REQ_BUF_SZ); //Set the string's memory to all 0s for length REQ_BUF_SZ
          break;
        }
        // every line of text received from the client ends with \r\n
        if ('\n' == c) {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        } 
        else if ('\r' != c) {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
    } // end while (client.connected())
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection
  } // end if (client)
}
/*************************************************************************************************
 *                                      XML Response Generator                                   *
 *************************************************************************************************/

// send the XML file with switch statuses and analog value
void XML_response(EthernetClient cl)
  {
  //[[J]]: Concatenating what was on separate lines saves us 12B/line, give or take
  cl.print("<?xml version = \"1.0\" ?>");
  cl.print("<inputs>");
  cl.print("<sensor>");
  cl.print(tempDallas1);
  Serial.print(tempDallas1);
  cl.print("</sensor>");
//
  cl.print("<sensor>");
  cl.print(tempSHT1x);
  cl.print("</sensor>");
//
  cl.print("<sensor>");
  cl.print(rhSHT1x);
  cl.print("</sensor>");
//
  cl.print("<modesw>");
//  cl.print(systemMode);
  cl.print("</modesw>");
//

  cl.print("</inputs>");
}

void setEnviroControls()
{
    // LED 1 (pin 6)
    if (strstr(HTTP_req, "DayStart")) {
       // LED_state[0] = 1;  // save LED state
      //  digitalWrite(6, HIGH);
    }
    else if (strstr(HTTP_req, "NightStart")) {
      //  LED_state[0] = 0;  // save LED state
      //  digitalWrite(6, LOW);
      
    }
}

/*************************************************************************************************
 *                                         Mode Definitions                                       *
 **************************************************************************************************/
void dayMode(){
  // This will determine what is on/off in day mode. 
  // T5 lights on
  digitalWrite(T5_1, HIGH);
  digitalWrite(T5_2, HIGH);

  // Waterfall On
  digitalWrite(PUMP, HIGH);

  // Fan On        
  digitalWrite(FAN, HIGH);

  // Heat lamps on / controlled
  digitalWrite(MVL, HIGH);
}

void nightMode(){
  // This will determine what is on/off in night mode. 
  // T5 Lights off
  digitalWrite(T5_1, LOW);
  digitalWrite(T5_2, LOW);

  // Waterfall off
  digitalWrite(PUMP, LOW);

  // Fan on /low?
  digitalWrite(FAN, LOW);

  // Heat Lamps off
  digitalWrite(MVL, LOW);
}


void serviceMode(){
  // This will determine what is on/off in service mode. 
  // T5 Lights at half power
  digitalWrite(T5_1, HIGH);
  digitalWrite(T5_2, LOW);

  // Waterfall Off
  digitalWrite(PUMP, LOW);

  // Fan on 
  digitalWrite(FAN, HIGH);

  // damperServos Open
  damperServos.write(DAMPERS_OPEN);

  // Heat lamps off
  digitalWrite(MVL, LOW);

}

void offMode(){
  // This will determine what is on/off in OFF mode. 
  // T5 Lights at half power
  digitalWrite(T5_1, LOW);
  digitalWrite(T5_2, LOW);

  // Waterfall Off
  digitalWrite(PUMP, LOW);

  // Fan on 
  digitalWrite(FAN, LOW);
}

void tempRegulation(){
  /*  Serial.println("Temp Regulation");
   Serial.println(tempSHT1x);
   Serial.println(targetTemp);
   Serial.println(hysActive);
   */
  boolean cooling = false;
  if ((tempSHT1x > targetTemp + hysDrift) && (!hysActive)){  // if the Temperature from the SHT1x sensor is greater than the target temperature, plus the hysteresis drift, and if Hystereis is off
    ventMode = true ;                                 // sets the return value to 1, indicating we're in a cooling stage
    hysActive = true;                           // Indicaate we're in a hystereis condition, to not toggle the dampers on/off too fast
    damperServos.write(DAMPERS_OPEN);// go to damper control, and turn the dampers open for fresh air (0 = outside air, 1 = recirculate)
    //  Serial.println("Cooling Active");
  }  
  if ((tempSHT1x < targetTemp - hysDrift) && (hysActive)){
    ventMode = false ;                              // Indicate we're out of hysteresis condition
    hysActive = false;
    damperServos.write(DAMPERS_CLOSED);
    // Serial.println("Recirculate Active");
  }  
  return;
}

/*
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS A0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer { 0x28, 0xA3, 0x27, 0x23, 0x05, 0x00, 0x00, 0x7F };

void setup(void)
{
  Serial.begin(9600);
  sensors.begin();
  
}


void loop(void)
{ 
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  float temptoprint = sensors.getTempF(insideThermometer);
  Serial.println("DONE");
  
  Serial.print(" Temp F: ");
  Serial.println(temptoprint); // Makes a second call to getTempC and then converts to Fahrenheit
}
*/
