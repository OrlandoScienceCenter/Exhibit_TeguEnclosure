  /* 
   
   Orlando Science Center - Automated Tegu Enclosure Control Program
   
   This controls all the things for the tegu enclosure and allows
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
  #define temperatureUnits sht1xalt::UNITS_FAHRENHEIT
  sht1xalt::Sensor sensor( dataPin, clockPin, clockPulseWidth, supplyVoltage, temperatureUnits );
  
  
  /*************************************************************************************************
   *                                  Ethernet/HTTP Settings                                        *
   **************************************************************************************************/
  
  // size of buffer used to capture HTTP requests
  #define REQ_BUF_SZ   60
  
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
  //define TEMPERATURE_PRECISION 9       // How many bits of temperature precision
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
  
  #define INDEX_FILENAME "index.htm"
  
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
  byte isDayFlag;  //[[J]]: You're using this as a boolean; make it more HR by making it a boolean?
  boolean ventMode;          // Ventilation mode - 0 Recirculate, 1 vent/cooling [[J]]: Make this a byte, to match use?
  byte systemMode;
  /*
    define eTargetTemp 1  // EEPROM Defines - TBD  [[J]]: Of course, comment this well.
   define eTargetRH   2
   define eDayStartTime 3
   define eNightStartTime 4 
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
   
      //Sets up servos to be controlled. One output going to three servos.
      damperServos.attach(6); 
  
      // SERIAL
       Serial.begin(9600);                  // Serial for debugging. Might need to drop the serial to save space. 
      // [[J]]: Just as an aside, you can leave the serial commands and surround them with #if/#endif
  
      // Starts ethernet and Server
      Ethernet.begin (mac, ip);            //Initialize Ethernet device
      server.begin();                      //start to listen for clients
  
      // Target Temperature ane RH Initial Setpoints
      //[[J]]: I think it would be cleaner/easier to set these all on 
      //declaration? It may not save space, but it'll be easier on you, 
      //since this code is never revisited.
      targetTemp = 85;      // Degrees Farenheit
      targetRH = 85;        // % Relative Humidity
      hysActive = 0;          // Sets hysteresis to off - allows function to operate
      hysDrift = 2;        // Number of degrees to drift over/under the setpoints
  
      // Time Setpoints
      dayStartTime = 8;
      nightStartTime = 14;
      offMode();
    }
  
    /*************************************************************************************************
     *                                              LOOP                                              *
     **************************************************************************************************/
    //void loop(){
    while (1) {
      delay (100);
      //[[J]]: This saved 8 bytes. Not a lot, but something. Unless there's a reason have it in each branch.
      getSensorData(); // Pull all the sensor data. No returns, all values plugged into global vars
      if (digitalRead(AUTO_ENABLE_PIN)){
        systemMode = ON_STATE;
        tempRegulation();     // Checks temp, opens/closes dampers as necessary
        if(isDay()){            // Check Day/Night States, and do the appropriate on/offs of equipment     
          //Serial.println("DayMode");
          dayMode();          // Day mode power settings 
          isDayFlag = 1;
        } else {
          //Serial.println("NightMode");
          nightMode();        // Night mode power settings 
          isDayFlag = 0;
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
      dSensors.requestTemperatures();                 // Send the Command to get the temperatures from the dallas sensors.
      tempDallas1 = dSensors.getTempF(dThermometer);  // Request temperature back in Farenheit (easy to change to celcius..)
      sensor.measure(tempSHT1x, rhSHT1x);             //SHT1x reading
      RTC.read(tm);                                   //RTC Reading
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
            Serial.println(HTTP_req);
            client.println("HTTP/1.1 200 OK");
            // remainder of header follows below, depending on if
            // web page or XML page is requested
            // Ajax request - send XML file
            if (strstr(HTTP_req, "ajax")) {
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
              setEnviroControls();
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
    cl.print("<?xml version = \"1.0\" ?>\n<data>\n<sen>");
//    cl.print("<data>");
//      cl.print("<sen>");
      cl.print(tempDallas1);
      cl.print("</sen>\n<sen>");
    
//      cl.print("<sen>");
      cl.print(tempSHT1x);
      cl.print("</sen>\n<sen>");
    
//      cl.print("<sen>");
      cl.print(rhSHT1x);
      cl.print("</sen>\n<m>");
    
//      cl.print("<m>");
      cl.print(systemMode);
      cl.print("</m>\n<m>");
      
//      cl.print("<m>");
      cl.print(isDayFlag);
      cl.print("</m>\n<t>");
     
//      cl.print("<t>");
      cl.print(tm.Hour);
      cl.print(":");
      cl.print(tm.Minute);
      cl.print("</t>\n<o>");
  
//      cl.print("<o>");
      cl.print(digitalRead(T5_1));
      cl.print("</o>\n<o>");
  
//      cl.print("<o>");
      cl.print(digitalRead(T5_2));
      cl.print("</o>\n<o>");
  
//      cl.print("<o>");
      cl.print(digitalRead(MVL));
      cl.print("</o>\n<o>");
  
//      cl.print("<o>");
      cl.print(digitalRead(FAN));
      cl.print("</o>\n<o>");
  
//      cl.print("<o>");
      cl.print(digitalRead(PUMP));
      cl.print("</o>\n</data>");
  
//      cl.print("</data>");   
  }
  
  void setEnviroControls()
  {
  /* okay, so, this is what a normal request looks like 
  GET /ajax_inputs&nocache=359634.24970390L
  
  then, when we get a request from the form, it'll look something like this
  GET /?dayStart=8&nightStart=15 HTTP/1.10L
  
  and, if your'e curious, on first page load, what the request looks like
  GET / HTTP/1.1
  Host: 10.1.1.100
  Conne
  GET /ajax_inputs&nocache=522250.52053100L
  
  Ideally, I'd like to be able to at least set the day start time, night start time, 
  requested temperature and requested humidity. there are others, but we'll see what we can come up with. 
  
  I can always change what the return values are, even add a special character into it?
  Also of note, if one value is entered, and the other is not, it returns nothing after the =
  
  This is the form section of the HTML code. 
        <form id="systemSettings" name="settingsForm">
           <input type="number" name="dayStart" size=2 max=23 min=0 value="">Day Start Hour<br /><br />
           <input type="number" name="nightStart" size=2 max=23 min=0 value="">Night Start Hour<br /><br />
           <input type="submit" value="Update Settings">
        </form>  
  
  I think i'm also running up against ram limits, or something, because with some functions,
  it'll cause the web page to no longer be displayed. 
  The GET request is still processsed, but does not return the webFile
  to the browser.
  */
  
        // This section will return a byte of where the first 
        //character of the "value" right after the equals would start
        // this works up until you have a two digit time. 
        //It also always returnst he first value of the nocache= number. *shrug*
  //char* cindex = (strchr(HTTP_req,'dayStart='));
  //byte index = (cindex-HTTP_req+1);
  //while(index!=NULL){
  //Serial.println(HTTP_req[index]);
  //}
  
  
  /*    I really think this would work (or similar) but It crashes/runs out of ram or something.
  
  char * cvalue = (strtok(HTTP_req, "?=&")); // it is supposed to seperate out strings and make things do. not in this implementation. cause reasons. 
  Serial.println(cvalue);
  */
  
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
  
    // Mercury Vapor Lights off
    digitalWrite(MVL, LOW);
  
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
  

