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
// If you want to report temperature units in Fahrenheit instead of Celcius, Change which line is commented/uncomented 
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
//File webFile;
  char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
  char req_index = 0;              // index into HTTP_req buffer


/*************************************************************************************************
*                                  Dallas 1 Wire Sensor Settings                                 *
**************************************************************************************************/
#define ONE_WIRE_BUS A0               // Where is the data pin plugged into?
  OneWire oneWire(ONE_WIRE_BUS);        // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
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
Servo dampers;
/*************************************************************************************************
*                                       Global Variables                                         *
**************************************************************************************************/

  long lastReadingTime = 0;
  float temp;
  float rh;
  tmElements_t tm;                
  
  
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
dampers.attach(6); 

// SERIAL
  Serial.begin(9600);                  // Serial for debugging. Might need to drop the serial to save space. 
 
 // Starts ethernet and Server
  Ethernet.begin (mac, ip);            //Initialize Ethernet device
  server.begin();                      //start to listen for clients
 
}



/*************************************************************************************************
*                                              LOOP                                              *
**************************************************************************************************/
void loop(){
  {
    
// check for a reading no more than once a second.
  if (millis() - lastReadingTime > 1000){
    // if there's a reading ready, read it:
      sensor.measure(temp, rh);          //SHT1x reading
      RTC.read(tm);                      //RTC Reading
      dTemp.requestTemperatures();
      // Set Alarms/Setpoints when temp is higher than 31C
      dTemp.setHighAlarmTemp(dTemp1, 31);
      // alarn when temp is lower than 27C
      dTemp.setLowAlarmTemp(dTemp1, 21);
  ///
   lastReadingTime = millis();
  }
  }
  // listen for incoming Ethernet connections:
  // Case statement here?
  listenForEthernetClients();
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
          client.print(temp);
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

// Dampers closed / controlled
dampers.write(90);    // Closed position

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

// Dampers closed / controlled
dampers.write(90);    // Closed position

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

// Dampers Open
dampers.write(0);    // Open position

// Heat lamps off
digitalWrite(MVL, LOW);

}

boolean checkTemp(){
 boolean alarm;
  if (temp > 75){
    alarm = 1;
  }
  else{
    alarm = 0;
  return alarm;
  }  
}

