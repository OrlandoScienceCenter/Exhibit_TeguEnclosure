/* 
 
 Orlando Science Center - Automated Tegu Enclosure Control Program
 
 This controls all the things for the tegu enclosure and allows
 a webpage to make changes and provide feedback
 
 Author:    MKing - hybridsix
 
 Some code examples and much help was derived from the tutorial on 
 http://startingelectronics.com , Written by W.A. Smith
 
 A big thanks to Darkmoonsinger for late night code reviews
 
 Lots of code snippits and ideas obtained through examples provided with libraries.

  Code uses init() and main() instead of setup() and loop() to save space.
 
 */

/*************************************************************************************************
 *                                         Libraries                                              *
 **************************************************************************************************/

//Libraries with "lite" in the name have been modified to remove unnecessary
//functions which consume space.
#include <Wire.h>              // I2C Library
#include <SPI.h>               // SPI Library 
#include <Ethernet.h>          // Ethernet Library
#include <OneWireLite.h>           // One Wire Comms LIbrary 
#include <DallasTemperatureLite.h> // Dallas Temp Sensor Library
#include <DS1307RTC.h>         // DS1307 Real Time Clock Library
#include <TimeLite.h>              // TIme library to more easily work with time and dates
#include <SD.h>                // SD Card Library
#include <ServoLite.h>             // Servo Library  
#include <sht1xaltLite.h>          // Library for SHT1x Sensor

/*************************************************************************************************
 *                                      SHT1x Settings                                            *
 **************************************************************************************************/

// SHT1x pin definitions
#define dataPin A1
#define clockPin A2
// This number can be set larger to slow down communication with the SHT1x, which may
// be necessary if the wires between the Arduino and the SHT1x are long.
#define clockPulseWidth 1
#define supplyVoltage sht1xalt::VOLTAGE_5V
#define temperatureUnits sht1xalt::UNITS_FAHRENHEIT
sht1xalt::Sensor sensor( dataPin, clockPin, clockPulseWidth, supplyVoltage, temperatureUnits );


/*************************************************************************************************
 *                                  Ethernet/HTTP Settings                                        *
 **************************************************************************************************/

// Size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   40

// Mac address of the device. 
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0x40, 0x64, }; 
// Sets the manual IP address of the device
IPAddress ip(10,1,1,100); 
// Create Server at port 80
EthernetServer server(80);
File webFile;
// Buffered HTTP request stored as null terminated string
char HTTP_req[REQ_BUF_SZ] = {0}; 
// Index into HTTP_req buffer
char req_index = 0;

/*************************************************************************************************
 *                                  Dallas 1 Wire Sensor Settings                                 *
 **************************************************************************************************/

//Data pin
#define ONE_WIRE_BUS A0
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dSensors(&oneWire);
//Device address: 28A327230500007F
DeviceAddress dThermometer { 
  0x28, 0xA3, 0x27, 0x23, 0x05, 0x00, 0x00, 0x7F };


/*************************************************************************************************
 *                                        Other Pin Defines                                       *
 **************************************************************************************************/

#define T5_1 9
#define T5_2 8
#define MVL 7
#define AUTO_ENABLE_PIN 6
#define SERVICE_ENABLE_PIN 5
#define SD_PIN 4
#define SERVOPIN 3
#define FAN 2
#define PUMP 1
#define ETH_PIN 10

/*************************************************************************************************
 *                                   Set Global Variables                                         *
 **************************************************************************************************/

#define OFF_STATE 0
#define ON_STATE 1
#define SERVICE_STATE 2

#define INDEX_FILENAME "index.htm"

#define DAMPERS_OPEN 0 //vent/cool
#define DAMPERS_CLOSED 90 //recirc

Servo damperServos;

//Typedef holding time elements
tmElements_t tm;
long lastReadingTime = 0;

float tempSHT1x;
float rhSHT1x;
float tempDallas1;

// Temp in degrees Farenheit
unsigned char targetTemp = 85;
// Number of degrees to drift over/under the setpoints
byte hysDrift = 2;
// Hysteresis on/off - allows function to operate
boolean hysActive = 0;

unsigned char dayStartTime = 8;
unsigned char nightStartTime = 17;
//Typed as a byte instead of boolean for XML exchange
byte isDayFlag;

boolean ventMode;
byte systemMode;

int main(void) {

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

    // Temporarily disable the ethernet chip to select the SD card
    pinMode(ETH_PIN, OUTPUT);
    digitalWrite(ETH_PIN, HIGH);
    // Initialize SD card
    SD.begin(SD_PIN);

    // Start Sensiron SHT1x Sensor(s)
    sensor.configureConnection();
    // Reset the SHT1x in case the arduino was reset during communication:
    sensor.softReset();

    // Initialize the Dallas OneWire Sensors
    dSensors.begin();

    //Sets up servos to be controlled. One output going to three servos.
    damperServos.attach(SERVOPIN); 

    // Starts ethernet and server
    //Initialize Ethernet device
    Ethernet.begin (mac, ip);
    //Start to listen for clients
    server.begin();

    //Initial state of the system defaults to off.
    offMode();
  }

  /*************************************************************************************************
   *                                              LOOP                                              *
   **************************************************************************************************/
  while (1) {
    delay (100);
    // Pull all the sensor data. No returns, all values plugged into global vars
    getSensorData();
    if (digitalRead(AUTO_ENABLE_PIN)){
      systemMode = ON_STATE;
      
      // Checks temp, opens/closes dampers as necessary
      tempRegulation();
      // Check day/night states, and do the appropriate on/offs of equipment     
      if(isDay()){
        // Day mode power settings 
        dayMode();
        isDayFlag = 1;
      } else {
        // Night mode power settings
        nightMode();
        isDayFlag = 0;
      } 
    } else if (digitalRead(SERVICE_ENABLE_PIN)){
      systemMode = SERVICE_STATE;
      serviceMode();
    } else {
      systemMode = OFF_STATE; 
      offMode();
    }

    // Listens for HTTP client requests
    listenForEthernetClients();
  }
}

/*************************************************************************************************
 *                                        Sensor Data Grab                                        *
 **************************************************************************************************/

// Check for a reading no more than once a second
void getSensorData() {
  if (millis() - lastReadingTime > 1000){
    // Get the temperatures from the dallas sensors.
    dSensors.requestTemperatures();
    // Request temperature back in Farenheit
    tempDallas1 = dSensors.getTempF(dThermometer);
    sensor.measure(tempSHT1x, rhSHT1x);
    RTC.read(tm);
    lastReadingTime = millis();
  }
} 

/*************************************************************************************************
 *                                     Day/night mode check                                      *
 **************************************************************************************************/
boolean isDay() { 
  return ((tm.Hour >= dayStartTime) && (tm.Hour <= nightStartTime));
}

/*************************************************************************************************
 *                              Ethernet/HTTP Request Processing                                 *
 **************************************************************************************************/

void listenForEthernetClients() {
  //Determine if the server has an available client connection
  EthernetClient client = server.available();

  // got client?
  if (client) {
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      // Client has data available to read
      if (client.available()) {
        // Read 1 byte (character) from client
        char c = client.read();
        // Buffer first part of HTTP request in HTTP_req array (string)
        // Leave last element  (REQ_BUF_SZ - 1) in array as \0 to null-terminate string
        if (req_index < (REQ_BUF_SZ - 1)) {
          // Save HTTP request character
          HTTP_req[req_index] = c;
          req_index++;
        }
        
        // Last line of client request is blank and ends with \n
        // Respond to client only after last line received
        if ('\n' == c && currentLineIsBlank) {
          // Send a standard http response header
          client.println("HTTP/1.1 200 OK");
          
          // Remainder of header follows below, 
          // whose content depends on if HTML or XML page is requested.

          // Ajax request - send XML file
          if (strstr(HTTP_req, "ajax")) {
            // Send rest of HTTP header
            client.println("Content-Type: text/xml");
            client.println("Connection: keep-alive"); 
            client.println();
            
            if (strstr(HTTP_req, "?")){
              setEnviroControls();
            }
            
            // Send XML file containing input states
            XML_response(client);
            
          } else {
            // Send rest of HTTP header
            client.println("Content-Type: text/html");
            client.println("Connection: keep-alive");
            client.println();
            
            if (strstr(HTTP_req, "?")){
              setEnviroControls();
              }
            
            // Send HTML
            
            // Open web page file
            webFile = SD.open(INDEX_FILENAME);
            // Check if the file has been opened
            if (webFile) {
              while(webFile.available()) {
                // Send HTML to client
                client.write(webFile.read());
              }
              webFile.close();
            }
          }
          
          // Reset buffer index and all buffer elements to 0
          req_index = 0;
          //Set the string's memory to all 0s for length REQ_BUF_SZ
          memset(HTTP_req, '0', REQ_BUF_SZ);
          break;
        }
        
        // Every line of text received from the client ends with \r\n
        if ('\n' == c) {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        } else if ('\r' != c) {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
    } // end while (client.connected())

    // Give the web browser time to receive the data
    delay(1);
    // Close the connection
    client.stop();
  } // end if (client)
}
/*************************************************************************************************
 *                                      XML Response Generator                                   *
 *************************************************************************************************/

// Send the XML file with switch statuses and analog value
void XML_response(EthernetClient cl)
{
  cl.print("<?xml version = \"1.0\" ?>\n<data>\n<sen>");
  cl.print(tempDallas1);
  cl.print("</sen>\n<sen>");

  cl.print(tempSHT1x);
  cl.print("</sen>\n<sen>");

  cl.print(rhSHT1x);
  cl.print("</sen>\n<m>");

  cl.print(systemMode);
  cl.print("</m>\n<m>");

  cl.print(isDayFlag);
  cl.print("</m>\n<t>");

  cl.print(tm.Hour);
  cl.print(":");
  cl.print(tm.Minute);
  cl.print("</t>\n<o>");

  cl.print(digitalRead(T5_1));
  cl.print("</o>\n<o>");

  cl.print(digitalRead(T5_2));
  cl.print("</o>\n<o>");

  cl.print(digitalRead(MVL));
  cl.print("</o>\n<o>");

  cl.print(digitalRead(FAN));
  cl.print("</o>\n<o>");

  cl.print(digitalRead(PUMP));
  cl.print("</o>\n</data>"); 
}

  // This section parses and pulls the numbers from the web page form submission GET request.
void setEnviroControls()
{
  //Thanks to Darkmoonsinger for the hard work on this; it's amazing code. 
  unsigned char index;
  crudeParse(HTTP_req, &dayStartTime, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);
  crudeParse(HTTP_req, &nightStartTime, &index);
  strncpy(HTTP_req, &HTTP_req[index], sizeof(HTTP_req)-1);
  crudeParse(HTTP_req, &targetTemp, &index);
}

/*************************************************************************************************
 *                                         Mode Definitions                                       *
 **************************************************************************************************/

// This will set what is on/off in day mode. 
void dayMode(){
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

// This will set what is on/off in night mode. 
void nightMode(){
  // T5 Lights off
  digitalWrite(T5_1, LOW);
  digitalWrite(T5_2, LOW);

  // Waterfall off
  digitalWrite(PUMP, LOW);

  // Fan on
  digitalWrite(FAN, LOW);

  // Heat Lamps off
  digitalWrite(MVL, LOW);
}

// This will set what is on/off in service mode. 
void serviceMode(){
  // T5 Lights at half power
  digitalWrite(T5_1, HIGH);
  digitalWrite(T5_2, LOW);

  // Waterfall Off
  digitalWrite(PUMP, LOW);

  // Fan on 
  digitalWrite(FAN, HIGH);

  // Damper servos open 
  damperServos.write(DAMPERS_OPEN);

  // Heat lamps off
  digitalWrite(MVL, LOW);
}

// This will set what is on/off in OFF mode. 
void offMode(){
  // T5 Lights at half power
  digitalWrite(T5_1, LOW);
  digitalWrite(T5_2, LOW);

  // Mercury vapor lights off
  digitalWrite(MVL, LOW);

  // Waterfall Off
  digitalWrite(PUMP, LOW);

  // Fan on 
  digitalWrite(FAN, LOW);
}

void tempRegulation(){
  boolean cooling = false;
  
  // If the temperature from the SHT1x sensor is greater than 
  // the target temperature plus the hysteresis drift, 
  // and if hystereis is off, set cooling and indicate that
  // we're in a hysteresis condition, to avoid toggling
  // the dampers on/off too fast. Open the dampers for fresh air.
  if ((tempSHT1x > targetTemp + hysDrift) && (!hysActive)){
    ventMode = true;
    hysActive = true;
    damperServos.write(DAMPERS_OPEN);
  }  

  // Otherwise, indicate we're out of hysteresis condition
  // and set vars appropriately.
  if ((tempSHT1x < targetTemp - hysDrift) && (hysActive)){
    ventMode = false;
    hysActive = false; 
    damperServos.write(DAMPERS_CLOSED);
  }  

  return;
}

/*
 * Crude parsing function to scan a given char[] string for the first
 * number found after the first '='. Stops at following '&', ' ', or null.
 * 
 * Parameters: str[] - string to parse; ret - blank unsigned char * to 
 * hold found number; index - blank char * to hold index of next '&'
 * Returns: void
 *
 * CAUTION: This code is BRITTLE. It does not handle strings longer
 * than 255 characters nor can it parse numbers greater than 255 digits.
 * This is done for efficiency, given the ranges of numbers for the expected
 * use- day and night are 0-23, temp is < 212, humidity is < 101. 
 */
void crudeParse(char str[], unsigned char * ret, unsigned char * index) {

  unsigned char i = 0;
  unsigned char num = 0;
  while (str[i]) {
    //The expected parse string is in the format 
    //"GET /?dayStart=8&nightStart=15&targetTemp=85 HTTP/1.10L"
    //So read to the first '=', then grab everything after until
    //we get to a '&'. Subtract the value of '0', which gives us
    //the actual number from the ascii value (as each successive number
    //is offset from '0' by its own value). Add that value to the
    //previous value multiplied by 10 (to shift the previous leftwards
    //value a tens place. Repeat until fin.
    if ('=' == str[i]) {
      num = num*10 + (str[++i] - '0');
      i++;

      while (!('\0' == str[i] || '&' == str[i] || ' ' == str[i])) {
        num = num*10 + (str[i]-'0');
        i++;
      }
      *index = i;

      break;
    }
    i++;
  }

  *ret = num;
}



