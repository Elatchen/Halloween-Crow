/*
Code for a IOT Halloween crow with glowing eyes, based on an esp8266 board. 
Device opens an AP, which you can connect to and give credentials for actual wifi network. it then automatically connects to that wifi network and self-hosts a webpage under crow.local. You can change eye animations and color there.
Pins can be found below, documentation kind of missing at the moment.

For next helloween I'd like to implement a speaker on the crow, how exaclty or what will be played is not clear yet
*/
#include <ESP8266mDNS.h>
#include "MultiLoop.hpp"
#include <WiFiManager.h>

#define APSSID "crow"
#define APPSK "cawcawcaw"

// pin for wifi config reset button
#define WIFI_CONFIG_TRIGGER_PIN 14 // D5

// Pins of LED's
int redLed = 5; // D1
int greenLed = 4; // D2
int blueLed = 0; // D3

// needed for "multithreading"
MultiLoop multiLoop = MultiLoop();

WiFiManager wifiManager;

// Create an instance of the server, specify the port to listen on as an argument
WiFiServer server(80);

/* Set these to your desired credentials. for AP */
const char* apSsid = APSSID;
const char* apPassword = APPSK;

String rgbValue = "FF0000";
String rgbRedHex = "FF";
String rgbGreenHex = "00";
String rgbBlueHex = "00";
int rgbRed = 0;
int rgbGreen = 255;
int rgbBlue = 255;
int animationStatus = 0;
bool ledOff = true;
//set default pulse time here
int defaultPulseTime = 5;
float pulseTime = defaultPulseTime;
// calculated by taking pulsetime (in seconds), pultiplying by 1000 for ms and deviding by 180 (steps per cycle)   int pulseTimeDelay = pulseTime*1000/180;
float pulseTimeDelay = pulseTime * 1000 / 180;
//set default loop time here
int defaultLoopTime = 8;
float loopTime = defaultLoopTime;
// 765 steps of loopfuncdelay per looptime. (Seconds*1000/765)
float loopTimeDelay = loopTime * 1000 / 765;
// strobe delay in ms
int defaultStrobeHertz = 10;
int strobeHertz = defaultStrobeHertz;
float strobeDownTime = 10;
float strobeUpTime = 10;
int inputError = 0;


// ColorData for diffrent animation functions
int rgbColorData[10][3] = {
  { 0, 255, 255 },  // Red
  { 0, 185, 255 },  // Orange
  { 0, 85, 255 },   // Yellow
  { 55, 0, 255 },   // Lime
  { 255, 0, 255 },  // Green
  { 255, 0, 128 },  // Teal
  { 255, 0, 0 },    // Bright Blue
  { 255, 255, 0 },  // Dark Blue
  { 128, 255, 0 },  // Purple
  { 0, 255, 128 }   // Pink
};
// size for some animationfunctions
int rgbColorDataSize = sizeof rgbColorData / sizeof rgbColorData[0];

// Needed for CLient Request Pharsing
// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 200;
// Variable to store the HTTP request
String header;

// hex to dec function, used to convert color values
int HexadecimalToDecimal(char* hex) {
  uint32_t val = 0;
  while (*hex) {
    // get current character then increment
    uint8_t byte = *hex++;
    // transform hex character to the 4bit equivalent number, using the ascii table indexes
    if (byte >= '0' && byte <= '9') byte = byte - '0';
    else if (byte >= 'a' && byte <= 'f') byte = byte - 'a' + 10;
    else if (byte >= 'A' && byte <= 'F') byte = byte - 'A' + 10;
    // shift 4 to make space for new digit, and add the 4 bits of the new digit
    val = (val << 4) | (byte & 0xF);
  }
  return val;
}

// function to set LED values
void setColorRgb(unsigned int red, unsigned int green, unsigned int blue) {
  analogWrite(redLed, red);
  analogWrite(greenLed, 255-((255-green)/2)); // color correction since green seems to bright
  analogWrite(blueLed, 255-((255-blue)/1.3));
  return;
}

bool setStrobeTiming(int strobeHertzFunc) {
  if (strobeHertzFunc < 1 || strobeHertzFunc > 100) {
    strobeHertz = defaultStrobeHertz;
    float tempHertzInMs = 1000 / strobeHertz;
    strobeDownTime = tempHertzInMs / 1000 * 800;
    strobeUpTime = tempHertzInMs / 1000 * 200;
    return false;
  }

  float tempHertzInMs = 1000 / strobeHertzFunc;
  strobeDownTime = tempHertzInMs / 1000 * 800;
  strobeUpTime = tempHertzInMs / 1000 * 200;
  strobeHertz = strobeHertzFunc;
  return true;
}

void pharseRequestSetGlobals(String req) {
  //rgb value in hex from url, then convert to 3 seperate integers in dec, then set led colors
  if (req.substring(4, 13) == "/led/rgb/") {
    ledOff = false;
    //stop animationstatus first
    animationStatus = 0;
    rgbValue = req.substring(13, 19);

    rgbRedHex = req.substring(13, 15);
    rgbRed = HexadecimalToDecimal(&(rgbRedHex[0]));
    rgbRed = 255 - rgbRed;

    rgbGreenHex = req.substring(15, 17);
    rgbGreen = HexadecimalToDecimal(&(rgbGreenHex[0]));
    rgbGreen = 255 - rgbGreen;

    rgbBlueHex = req.substring(17, 19);
    rgbBlue = HexadecimalToDecimal(&(rgbBlueHex[0]));
    rgbBlue = 255 - rgbBlue;

    setColorRgb(rgbRed, rgbGreen, rgbBlue);
  } else if (req.substring(4, 19) == "/led/animation/") {
    ledOff = false;
    animationStatus = atoi(&(req.substring(19, 20)[0]));
    if (animationStatus == 1) {
      //looptime parsed out of req string; it counts string length, substrings from last "/"-symbol to end of string, no matter how long, then converts to int.
      loopTime = atoi(&(req.substring(21, strlen(&(req[0])))[0]));
      if (loopTime < 1) {
        loopTime = defaultLoopTime;
        inputError = 1;
      }
      loopTimeDelay = loopTime * 1000 / 765;
    } else if (animationStatus == 2 || animationStatus == 5) {
      //pulsetime parsed out of req string; it counts string length, substrings from last "/"-symbol to end of string, no matter how long, then converts to int.
      pulseTime = atoi(&(req.substring(21, strlen(&(req[0])))[0]));
      if (pulseTime < 1) {
        pulseTime = defaultPulseTime;
        inputError = 1;
      }
      pulseTimeDelay = pulseTime * 1000 / 180;
    } else if (animationStatus == 3 || animationStatus == 4) {
      //strobehertz parsed out of req string; it counts string length, substrings from last "/"-symbol to end of string, no matter how long, then converts to int.
      strobeHertz = atoi(&(req.substring(21, strlen(&(req[0])))[0]));
      if (setStrobeTiming(strobeHertz) == false) {
        inputError = 1;
      }
    }
  } else if (req.substring(4, 13) == "/led/off/") {
    //disable all leds
    ledOff = true;
    animationStatus = 0;
    setColorRgb(255, 255, 255);
  }
  return;
}

void httpResponseHTMLCSSJSCRIPT(WiFiClient client) {
  // HTML Tags, Body, title, includes
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nCache-Control: max-age=0\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n<head>\r\n"));
  client.print(F("<title>Halloween Crow</title>"));
  client.print(F("\r\n<link rel='icon' type='image/png' href='https://www.favicon.cc/logo3d/396100.png'>"));

  client.print(F("\r\n<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap@4.1.3/dist/css/bootstrap.min.css' integrity='sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO' crossorigin='anonymous'>"));
  client.print(F("\r\n<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.6.0/jquery.min.js'></script>"));
  client.print(F("\r\n<script src='https://cdn.jsdelivr.net/npm/popper.js@1.12.9/dist/umd/popper.min.js' crossorigin='anonymous'></script>"));
  client.print(F("\r\n<script src='https://cdn.jsdelivr.net/npm/bootstrap@4.1.3/dist/js/bootstrap.min.js' integrity='sha384-ChfqqxuZUCnJSK3+MXmPNIyE6ZbWh2IMqE241rYiqJxyMiZ6OW/JmZQ5stwEULTy' crossorigin='anonymous'></script>"));
  client.print(F("<style>body {margin: auto;text-align: center;padding: 10px;} .navbar {color:#"));
  client.print(rgbValue);
  client.print(F(";text-shadow: -1px 0 white, 0 1px white, 1px 0 white, 0 -1px white;} .btm-margin {margin: 6px;}@media (orientation: landscape) {body{zoom: 100%;}}@media (orientation: portrait) {body{zoom: 200%;}}</style>"));
  client.print(F("\r\n</head>\r\n<body>\r\n"));

  // Navbar
  client.print(F("<nav class='navbar navbar-expand-lg navbar-dark bg-dark'><h2 style='margin: auto;text-align: center;'>Halloween Crow</h2></nav>"));


  // Color Picker
  client.print(F("<div class='container-fluid'>"));  // bootstrap container start
  client.print(F("</br><h5>Select Color</h5>"));
  client.print(F("<div><label for='color' style='display:none;'>Colorinput</label><input type='color' id='color' name='color' value='#"));
  client.print(rgbValue);
  client.print(F("' style='width:200px; height:60px; margin: 10px;'></div><div id='choosen-color'></div>"));
  client.print(F("<div id='color-button'><a href='/../../../../led/rgb/"));
  client.print(rgbValue);
  client.print(F("/'><button type='button' class='btn btn-secondary btn-lg btm-margin'>Update color</button></a></div>"));
  // script to update color picker button a-tag
  client.print(F("<script type='text/javascript'>jQuery('#color').on('change',function(){var cID = jQuery(this).val();var colorHex = cID.substring(1, cID.length);jQuery('#color-button').html(\"<a href='/../../../../led/rgb/\"+colorHex+\"/'><button type='button' class='btn btn-secondary btn-lg btm-margin'>Update color</button></a>\")});</script>"));

  // LED OFF
  client.print(F("<div><a href='/../../../../led/off/'><button type='button' class='btn btn-secondary btn-lg btm-margin'>Turn LEDs off</button></a></div>"));
  client.print(F("</div>"));  // bootstrap container end

  // Loop Animation
  client.print(F("<div class='container-fluid'>"));  // bootstrap container start
  client.print(F("</br><h5>Loop Animation</h5>"));
  // Time Selector for Pulse Button
  client.print(F("<label for='loop_seconds'>Looplength in seconds: </label></br><input type='number' id='loop_seconds' name='loop_seconds' value='"));
  client.print(int(loopTime));
  client.print(F("'>"));
  // Loop Button
  client.print(F("<div><div id='loop-button'><a href='/../../../../led/animation/1/"));
  client.print(int(loopTime));
  client.print(F("'><button type='button' class='btn btn-secondary btm-margin'>Loop colors</button></a></div></div>"));
  // script to update loop button a-tag
  client.print(F("<script type='text/javascript'>jQuery('#loop_seconds').on('change',function(){var cID = jQuery(this).val();var loop_secondsVal = cID;jQuery('#loop-button').html(\"<a href='/../../../../led/animation/1/\"+loop_secondsVal+\"/'><button type='button' class='btn btn-secondary btm-margin'>Loop colors</button></a>\")});</script>"));
  client.print(F("</div>"));  // bootstrap container end


  // Pulse Animations
  client.print(F("<div class='container-fluid'>"));  // bootstrap container start
  client.print(F("</br><h5>Pulse Animations</h5>"));
  // Time Selector for Pulse Buttons
  client.print(F("<label for='pulse_seconds'>Pulselenght in seconds: </label></br><input type='number' id='pulse_seconds' name='pulse_seconds' value='"));
  client.print(int(pulseTime));
  client.print(F("'>"));
  // Pulse current color Button
  client.print(F("<div><div id='pulse-button'><a href='/../../../../led/animation/2/"));
  client.print(int(pulseTime));
  client.print(F("'><button type='button' class='btn btn-secondary btm-margin'>Pulse current color</button></a>"));
  // Pulse RGB Button
  client.print(F("<a href='/../../../../led/animation/5/"));
  client.print(int(pulseTime));
  client.print(F("'><button type='button' class='btn btn-secondary btm-margin'>Pulse RGB</button></a></div></div>"));
  // script to update pulse button a-tags
  client.print(F("<script type='text/javascript'>jQuery('#pulse_seconds').on('change',function(){var cID = jQuery(this).val();var pulse_secondsVal = cID;jQuery('#pulse-button').html(\"<a href='/../../../../led/animation/2/\"+pulse_secondsVal+\"/'><button type='button' class='btn btn-secondary btm-margin'>Pulse current color</button></a><a href='/../../../../led/animation/5/\"+pulse_secondsVal+\"/'><button type='button' class='btn btn-secondary btm-margin'>Pulse RGB</button></a>\")});</script>"));
  client.print(F("</div>"));  // bootstrap container end

  // Strobe Animations
  client.print(F("<div class='container-fluid'>"));  // bootstrap container start
  client.print(F("</br><h5>Strobe Animations</h5>"));
  // Hertz Selector for strobe Button
  client.print(F("<label for='hertz'>Strobe effect in Hertz: </label></br><input type='number' id='hertz' name='hertz' value='"));
  client.print(int(strobeHertz));
  client.print(F("'>"));
  // Info/Warning Popup for strobe Button
  client.print(F("<a tabindex='0' class='btn btn-lg btn-danger btm-margin' data-toggle='popover' data-trigger='focus' data-placement='top' title='Epilepsy warning!' data-content='Some hertz ranges might trigger epilepsy. This input accepts values from 1-100'><img src='https://cdn.pixabay.com/photo/2012/04/24/23/56/information-41225_1280.png' alt='info' width='20' height='20'></a>"));
  client.print(F("<script type='text/javascript'>$(function() {$(\"[data-toggle='popover']\").popover()})</script>"));
  client.print(F("<script type='text/javascript'>$('.popover-dismiss').popover({trigger: 'focus'})</script>"));
  // Strobe current color Button
  client.print(F("<div><div id='strobe-button'><a href='/../../../../led/animation/3/"));
  client.print(int(strobeHertz));
  client.print(F("'><button type='button' class='btn btn-secondary btm-margin'>Strobe current color</button></a><a href='/../../../../led/animation/4/"));
  client.print(int(strobeHertz));
  client.print(F("'><button type='button' class='btn btn-secondary btm-margin'>Strobe RGB</button></a></div></div>"));
  // script to update strobe Button a-tag
  client.print(F("<script type='text/javascript'>jQuery('#hertz').on('change',function(){var cID = jQuery(this).val();var hertzVal = cID;jQuery('#strobe-button').html(\"<a href='/../../../../led/animation/3/\"+hertzVal+\"/'><button type='button' class='btn btn-secondary btm-margin'>Strobe current color</button></a><a href='/../../../../led/animation/4/\"+hertzVal+\"/'><button type='button' class='btn btn-secondary btm-margin'>Strobe RGB</button></a>\")});</script>"));
  client.print(F("</div>"));  // bootstrap container end

  // Alerts
  if (inputError != 0) {
    client.print(F("<div class='container-fluid'>"));  // bootstrap container start
    client.print(F("</br><div class='alert alert-warning alert-dismissible fade show' role='alert'><strong>Oops!</strong> An input was invalid. Value has been reset to default instead!<button type='button' class='close' data-dismiss='alert' aria-label='Close'><span aria-hidden='true'>&times;</span></button></div>"));
    client.print(F("</div>"));  // bootstrap container end
  }
  inputError = 0;

  //html end tags
  client.print(F("\r\n</body>\r\n</html>"));
  client.println(F("\r\n"));  // The HTTP response ends with another blank line

  return;
}

// animation functions

//function 1 - loop all colors
void loopColors() {
  unsigned int rgbColour[3];
  // Start off with red.
  rgbColour[0] = 255;
  rgbColour[1] = 0;
  rgbColour[2] = 0;
  // Choose the colours to increment and decrement.
  for (;;) {
    for (int decColour = 0; decColour < 3; decColour += 1) {
      int incColour = decColour == 2 ? 0 : decColour + 1;
      // cross-fade the two colours.
      for (int i = 0; i < 255; i += 1) {
        if (animationStatus != 1) {
          return;
        }
        rgbColour[decColour] -= 1;
        rgbColour[incColour] += 1;
        setColorRgb(255 - rgbColour[0], 255 - rgbColour[1], 255 - rgbColour[2]);
        multiLoop.delay(loopTimeDelay - 0.5);
      }
    }
  }
}
//function 2 - pulse  current color
void pulseColor() {
  unsigned int rgbColourPulse[3];
  rgbColourPulse[0] = 255 - rgbRed;
  rgbColourPulse[1] = 255 - rgbGreen;
  rgbColourPulse[2] = 255 - rgbBlue;
  float sinValRed;
  int ledValRed;
  float sinValGreen;
  int ledValGreen;
  float sinValBlue;
  int ledValBlue;

  for (;;) {
    for (int x = 0; x < 180; x++) {
      if (animationStatus != 2) {
        return;
      }
      // convert degrees to radians then obtain sin value
      sinValRed = (sin(x * (3.1412 / 180)));
      ledValRed = int(sinValRed * rgbColourPulse[0]);

      sinValGreen = (sin(x * (3.1412 / 180)));
      ledValGreen = int(sinValGreen * rgbColourPulse[1]);

      sinValBlue = (sin(x * (3.1412 / 180)));
      ledValBlue = int(sinValBlue * rgbColourPulse[2]);

      setColorRgb(255 - ledValRed, 255 - ledValGreen, 255 - ledValBlue);
      multiLoop.delay(pulseTimeDelay);
    }
  }
}
//function 3 - strobe current color
void strobeColor() {
  for (;;) {
    setColorRgb(255, 255, 255);  // leds off
    if (animationStatus != 3) {  // this fuckery is needed to avoid strange behaviour when transitioning between inputs
      if (ledOff == false) {
        setColorRgb(rgbRed, rgbGreen, rgbBlue);
      }
      return;
    }
    multiLoop.delay(strobeDownTime);
    setColorRgb(rgbRed, rgbGreen, rgbBlue);  // leds on
    if (animationStatus != 3) {              // this fuckery is needed to avoid strange behaviour when transitioning between inputs
      if (ledOff == true) {
        setColorRgb(255, 255, 255);
      }
      return;
    }
    multiLoop.delay(strobeUpTime);
  }
}
//function 4 - strobe color in rgb
void strobeRgb() {
  int strobeRgbCounter = 0;
  for (;;) {
    setColorRgb(255, 255, 255);  // leds off
    if (animationStatus != 4) {
      if (ledOff == false) {  // this fuckery is needed to avoid strange behaviour when transitioning between inputs
        setColorRgb(rgbRed, rgbGreen, rgbBlue);
      }
      return;
    }
    multiLoop.delay(strobeDownTime);
    setColorRgb(rgbColorData[strobeRgbCounter][0], rgbColorData[strobeRgbCounter][1], rgbColorData[strobeRgbCounter][2]);  // leds on
    strobeRgbCounter++;
    if (strobeRgbCounter >= rgbColorDataSize) {
      strobeRgbCounter = 0;
    }
    if (animationStatus != 4) {  // this fuckery is needed to avoid strange behaviour when transitioning between inputs
      if (ledOff == true) {
        setColorRgb(255, 255, 255);
      }
      return;
    }
    multiLoop.delay(strobeUpTime);
  }
}
//function 5 - pulse RGB colors
void pulseRgb() {
  float sinValRed;
  int ledValRed;
  float sinValGreen;
  int ledValGreen;
  float sinValBlue;
  int ledValBlue;
  int pulseRgbCounter = 0;

  for (;;) {
    for (int x = 0; x < 180; x++) {
      if (animationStatus != 5) {
        return;
      }
      // convert degrees to radians then obtain sin value
      sinValRed = (sin(x * (3.1412 / 180)));
      ledValRed = int(sinValRed * (255 - rgbColorData[pulseRgbCounter][0]));

      sinValGreen = (sin(x * (3.1412 / 180)));
      ledValGreen = int(sinValGreen * (255 - rgbColorData[pulseRgbCounter][1]));

      sinValBlue = (sin(x * (3.1412 / 180)));
      ledValBlue = int(sinValBlue * (255 - rgbColorData[pulseRgbCounter][2]));

      setColorRgb(255 - ledValRed, 255 - ledValGreen, 255 - ledValBlue);
      multiLoop.delay(pulseTimeDelay);
    }
    pulseRgbCounter++;
    if (pulseRgbCounter >= rgbColorDataSize) {
      pulseRgbCounter = 0;
    }
  }
}

void serverLoop() {
  //update MDNS, not quite sure why or if this is needed
  MDNS.update();
  // Check if a client has connected
  WiFiClient client = server.available();  // Listen for incoming clients

  if (client) {                     // If a new client connects,
    Serial.println("New Client.");  // print a message out in the serial port
    String currentLine = "";        // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // pharses Request and Sets Animation/error/colorvariables
            pharseRequestSetGlobals(header);
            httpResponseHTMLCSSJSCRIPT(client);
            // Break out of the while loop
            break;
          } else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // The client will actually be *flushed* then disconnected
    // when the function returns and 'client' object is destroyed (out-of-scope)
    // flush = ensure written data are received by the other side
    client.flush();
    client.stop();
    Serial.println(F("Disconnecting from client"));
  }
}

void animationLoop() {
  if (animationStatus == 1) {
    loopColors();
  } else if (animationStatus == 2) {
    pulseColor();
  } else if (animationStatus == 3) {
    strobeColor();
  } else if (animationStatus == 4) {
    strobeRgb();
  } else if (animationStatus == 5) {
    pulseRgb();
  }
}

// SETUP FUNCTION - RUNS BEFORE ANY LOOP
void setup() {
  Serial.begin(115200);
  delay(10);  // not sure why delay is needed, but I kept if from the example
  Serial.println('\n');

  //prepare LED
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);

  setColorRgb(128, 255, 0);  // Purple Eyes while connecting to WIFI
  
  //prepare config reset button
  pinMode(WIFI_CONFIG_TRIGGER_PIN, INPUT_PULLUP);

  // if reset-pin on startup is low, reset wifi settings
  if (digitalRead(WIFI_CONFIG_TRIGGER_PIN) == LOW) {
    Serial.println("Resettig Wifi Config");
    wifiManager.resetSettings();
  }

  // just to set the esp to station mode, not client
  WiFi.mode(WIFI_STA);

  // insane wifi connection library, still stuff to configure
  wifiManager.autoConnect(apSsid, apPassword);

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());  // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());  // Send the IP address of the ESP8266 to the computer

  if (!MDNS.begin("crow")) {  // Start the mDNS responder for crow.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  // Start the server
  server.begin();
  Serial.println(F("Server started"));


  setColorRgb(rgbRed, rgbGreen, rgbBlue);  // set eyes to default color for crow stuff
  // Setting up "multithreading" Loops
  // Exectute loop0 as fast as possible
  multiLoop.addLoop(serverLoop);
  // Exectute animationLoop as fast as possible
  multiLoop.addLoop(animationLoop);
}

void loop() {
  // needed to start "multithreading"
  multiLoop.dispatch();
}