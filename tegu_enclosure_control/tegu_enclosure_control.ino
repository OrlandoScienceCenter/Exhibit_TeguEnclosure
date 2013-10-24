/* 

Orlando Science Center - Automated Tegu Enclosure Control Program

This controls all the things for the tegu enclosure, and allows
a webpage to make changes and provide feedback

Author:    MKing - hybridsix

Some code examples and much help was derrived from the tutorial on 
http://startingelectronics.com , Written by W.A. Smith

*/

/*************************************************************************************************
*                                         Libraries                                              *
**************************************************************************************************/

#include <Wire.h>              // I2C Library
#include <SPI.h>               // SPI Library 
#include <Ethernet.h>          // Ethernet Library
#include <DallasTemperature.h> // Dallas Temp Sensor Library
#include <OneWire.h>           // One Wire Comms LIbrary 
//#include <SHT1x.h>             // Temp/Humidity Sensor Library // Old library - see sht1xalt.h
#include <DS1307RTC.h>         // DS1307 Real Time Clock Library
#include <Time.h>              // TIme library for easier workings with time and dates
#include <SD.h>                // SD Card Library
#include <Servo.h>             // Servo Library  
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
//#define tempSHT1xeratureUnits sht1xalt::UNITS_CELCIUS
#define tempSHT1xeratureUnits sht1xalt::UNITS_FAHRENHEIT
sht1xalt::Sensor sensor( dataPin, clockPin, clockPulseWidth, supplyVoltage, tempSHT1xeratureUnits );



/*************************************************************************************************
*                                  Ethernet/HTTP Settings                                        *
**************************************************************************************************/

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   40

//MAC Address
  byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0x40, 0x64, }; // Sets the Mac Address of the program to that of the device. Change these to real values
  IPAddress ip(10,1,1,100);                                // Sets the manual IP address of the device. Change to real values
  EthernetServer server(80);                          // Create Server at port 8088
//File webFile;
  char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
  char req_index = 0;              // index into HTTP_req buffer


/*************************************************************************************************
*                                  Dallas 1 Wire Sensor Settings                                 *
**************************************************************************************************/
#define ONE_WIRE_BUS A0               // Where is the data pin plugged into?
  OneWire oneWire(ONE_WIRE_BUS);        // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas tempSHT1xerature ICs)
  DallasTemperature dTemp(&oneWire);  // Pass our oneWire reference to Dallas Temperature. 
  DeviceAddress dTemp1;
  

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
  float rh;
  tmElements_t tm;       
  byte targetTemp;
  byte targetRH;
  byte hysDrift;
  boolean hysActive;
  
  
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
dTemp.begin();

//Sets up servos to be controlled. One output going to three servos.
damperServos.attach(6); 

// SERIAL
  Serial.begin(9600);                  // Serial for debugging. Might need to drop the serial to save space. 
 
 // Starts ethernet and Server
  Ethernet.begin (mac, ip);            //Initialize Ethernet device
  server.begin();                      //start to listen for clients
  
// Target Temperature ane RH Initial Setpoints
targetTemp = 85;      // Degrees Farenheit
//targetRH = 85;        // % Relative Humidity
hysActive = 0;          // Sets hysteresis to off - allows function to operate
hysDrift = 2;        // Number of degrees to drift over/under the setpoints
}



/*************************************************************************************************
*                                              LOOP                                              *
**************************************************************************************************/
void loop(){
  {
    
// check for a reading no more than once a second.
  if (millis() - lastReadingTime > 1000){
    // if there's a reading ready, read it:
      sensor.measure(tempSHT1x, rh);          //SHT1x reading
      RTC.read(tm);                      //RTC Reading
      lastReadingTime = millis();
  }
  }
  // listen for incoming Ethernet connections:
  // Case statement here?
  listenForEthernetClients();
  dayMode();
}

/*************************************************************************************************
*                              Ethernet/ HTTP Request Processing                                 *
**************************************************************************************************/
void listenForEthernetClients() {
  // listen for incoming clients
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
          client.print(rh);
          client.print("%");
          client.println("<br />");  
    //
           client.print("Dallas OneWire Temp ");
           client.print((dTemp.getTempCByIndex(0))*1.8+33);  
           client.print("F    ");
           client.println("<br />  Alarms - ");
           client.print(checkTemp());
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
damperServos.write(0);    // Open position

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

boolean checkTemp(){
 boolean cooling;
  if ((tempSHT1x > targetTemp + hysDrift) && (!hysActive)){  // if the Temperature from the SHT1x sensor is greater than the target temperature, plus the hysteresis drift, and if Hystereis is off
    cooling = 1;                                 // sets the return value to 1, indicating we're in a cooling stage
    hysActive = 1;                           // Indicaate we're in a hystereis condition, to not toggle the dampers on/off too fast
    damperControl(0);                      // go to damper control, and turn the dampers open for fresh air (0 = outside air, 1 = recirculate)
  }  
  if ((tempSHT1x < targetTemp - hysDrift) && (hysActive)){
    cooling = 0;                              // Indicate we're out of hysteresis condition
    damperControl(1);                        //damper contro, turn the dampers closed for recirculate air
  }  
  return cooling;
}

boolean damperControl(int damperState){
  if (damperState){
    damperServos.write(90);
  }
  else {
    damperServos.write(0);
  }
}

