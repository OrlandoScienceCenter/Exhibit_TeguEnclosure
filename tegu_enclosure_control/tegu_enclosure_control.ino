/* 

Orlando Science Center - Automated Tegu Enclosure Control Program

This controls all the things for the tegu enclosure, and allows
a webpage to make changes and provide feedback

Author:    MKing - hybridsix

Some code examples and much help was derrived from the tutorial on 
http://startingelectronics.com , Written by W.A. Smith

A big thanks to darkmoonsinger for late night code reviews

Lots of code snippits and ideas obtained through examples provided with libraries


*/

/*************************************************************************************************
*                                         Libraries                                              *
**************************************************************************************************/

#include <Wire.h>              // I2C Library
#include <SPI.h>               // SPI Library 
#include <Ethernet.h>          // Ethernet Library
#include <OneWireLite.h>           // One Wire Comms LIbrary 
#include <DallasTemperatureLite.h> // Dallas Temp Sensor Library
//#include <SHT1x.h>             // Temp/Humidity Sensor Library // Old library - see sht1xalt.h
#include <DS1307RTC.h>         // DS1307 Real Time Clock Library
#include <TimeLite.h>              // TIme library for easier workings with time and dates
#include <SD.h>                // SD Card Library
#include <ServoLite.h>             // Servo Library  
#include <sht1xalt.h>          // New Library for SHT1x Sensor - works much better. 

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
  byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0x40, 0x64, }; // Sets the Mac Address of the program to that of the device. Change these to real values
  IPAddress ip(10,1,1,100);                                // Sets the manual IP address of the device. Change to real values
  EthernetServer server(80);                          // Create Server at port 8088
  File webFile;
  char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
  char req_index = 0;              // index into HTTP_req buffer


/*************************************************************************************************
*                                  Dallas 1 Wire Sensor Settings                                 *
**************************************************************************************************/
#define ONE_WIRE_BUS A0               // Where is the data pin plugged into?
  OneWire oneWire(ONE_WIRE_BUS);        // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas tempSHT1xerature ICs)
  DallasTemperature dallasSensors(&oneWire);  // Pass our oneWire reference to Dallas Temperature. 
  DeviceAddress dTemp1 = { 0x28, 0xA3, 0x27, 0x23, 0x5, 0x0, 0x0, 0x7F }; //28A327230500007F;
  

/*************************************************************************************************
*                                        Other Pin Defines                                       *
**************************************************************************************************/
// hese were done as defines instead of taking up an int to save space early on. 
#define T5_1 9
#define T5_2 8
#define MVL 7
#define AUTO_ENABLE_PIN 6
#define SERVICE_ENABLE_PIN 5
// Pin 4 is for SD Card
#define PUMP 3
#define FAN 2
//#define A3 
//A4 SDA    I2C/TWI
//A5 SCL    I2C/TWI
Servo damperServos;
/*************************************************************************************************
*                                   Set Global Variables                                         *
**************************************************************************************************/

  long lastReadingTime = 0;
  float tempSHT1x;
  float rhSHT1x;
  byte tempDallas1;
  tmElements_t tm;       
  byte targetTemp;
  byte targetRH;
  byte hysDrift;
  boolean hysActive;
  byte dayStartTime;
  byte nightStartTime;
  boolean ventMode;          // Ventilation mode - 0 Recirculate, 1 vent/cooling
/*
  #define eTargetTemp 1  // EEPROM Defines - TBD
  #define eTargetRH   2
  #define eDayStartTime 3
  #define eNightStartTime 4  
*/  
/*************************************************************************************************
*                                              SETUP                                             *
**************************************************************************************************/
void setup()
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
  pinMode(10, OUTPUT);        // Temporarily Disable the Ethernet Chip to select the SD Card
  digitalWrite(10, HIGH);
    SD.begin(4);             // Initialize SD Card

// Start Sensiron SHT1x Sensor(s)
  sensor.configureConnection();
  sensor.softReset();  // Reset the SHT1x, in case the Arduino was reset during communication:
  
// Initialize the Dallas OneWire Sensors
  dallasSensors.begin();


//Sets up servos to be controlled. One output going to three servos.
//  damperServos.attach(6); 

// SERIAL
//  Serial.begin(9600);                  // Serial for debugging. Might need to drop the serial to save space. 
 
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
void loop(){
delay (100);
byte systemMode;
  
   if (digitalRead(AUTO_ENABLE_PIN)){
      systemMode = 1;
        getSensorData();      // Pull all the sensor data. No returns, all values plugged into global vars
        tempRegulation();     // Checks temp, opens/closes dampers as necessary
            if(isDay()){            // Check Day/Night States, and do the appropriate on/offs of equipment     
              //Serial.println("DayMode");
              dayMode();          // Day mode power settings 
              }
            else {
             //Serial.println("NightMode");
              nightMode();        // Night mode power settings 
              } 
    }
    else if (digitalRead(SERVICE_ENABLE_PIN)){
      systemMode = 2;
        getSensorData();
        serviceMode();
    }
    else{
      systemMode = 0; 
         getSensorData();
         offMode();
    }

listenForEthernetClients();          /// Listens for HTTP client requests
}


/*************************************************************************************************
*                                        Sensor Data Grab                                        *
**************************************************************************************************/
void getSensorData() { // check for a reading no more than once a second.
  if (millis() - lastReadingTime > 1000){
    // if there's a reading ready, read it:
      dallasSensors.requestTemperatures();
      tempDallas1 = dallasSensors.getTempC(dTemp1);
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
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    // remainder of header follows below, depending on if
                    // web page or XML page is requested
                    // Ajax request - send XML file
                    if (StrContains(HTTP_req, "ajax_inputs")) {

                      // send rest of HTTP header
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();
                        // send XML file containing input states
                        XML_response(client);
                    }
                    else {  // web page request
                        // send rest of HTTP header
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        // send web page
                        webFile = SD.open("index.htm");        // open web page file
                        if (webFile) {
                            while(webFile.available()) {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
}

// send the XML file with switch statuses and analog value
void XML_response(EthernetClient cl)
{
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    cl.print("<button1>");
    if (digitalRead(7)) {
        cl.print("ON");
    }
    else {
        cl.print("OFF");
    }
    cl.print("</button1>");
    cl.print("<button2>");
    if (digitalRead(8)) {
        cl.print("ON");
    }
    else {
        cl.print("OFF");
    }
    cl.print("</button2>");
    cl.print("<analog1>");
//    cl.print(anaogvalue);
    cl.print("</analog1>");
    cl.print("</inputs>");
}


void StrClear(char *str, char length) // sets every element of str to 0 (clears array)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// returns 1 if string found
// returns 0 if string not found
boolean StrContains(char *str, char *sfind)  // searches for the string sfind in the string str
{                                            // returns 1 if string found
                                             // returns 0 if string not found
  char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }

    return 0;
}


  /*  // listen for incoming clients
 EthernetClient client = server.available();
  if (client) {
    Serial.println("Got a client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
	  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // print the current readings, in HTML format:
          client.print("SHT1x Temperature and Humidity ");
          client.print(tempSHT1x);
          client.print("F    ");
          client.print(rhSHT1x);
          client.print("%");
          client.println("<br />");  
    //
           client.print("Dallas OneWire Temp ");
           client.print(tempDallas1*1.8+32);  
           client.print("F    ");
           client.println("<br />  Vent Mode - ");
           client.print(ventMode);
           client.println("<br />");  
           client.print("Time = ");
           client.print(tm.Hour);
           client.write(':');
           client.print(tm.Minute);
//
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  } 
} 


*/
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
damperControl(0);        // put dampers into open position

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
    damperControl(0);                      // go to damper control, and turn the dampers open for fresh air (0 = outside air, 1 = recirculate)
  //  Serial.println("Cooling Active");
  }  
  if ((tempSHT1x < targetTemp - hysDrift) && (hysActive)){
    ventMode = false ;                              // Indicate we're out of hysteresis condition
    hysActive = false;
    damperControl(1);                        //damper contro, turn the dampers closed for recirculate air
 // Serial.println("Recirculate Active");
  }  
  return;
}

boolean damperControl(int damperState){
  if (damperState){
    damperServos.write(90);
  }
  else {
    damperServos.write(0);
  }
}

