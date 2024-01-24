#include <Arduino.h>            // General
#include <WiFi.h>               // WIFI and Web server
#include <AsyncTCP.h>           // WIFI and Web server
#include <SPIFFS.h>             // WIFI and Web server
#include <ESPAsyncWebServer.h>  // WIFI and Web server
#include <DHT.h>                // DHT 22
#include <ESP32Servo.h>         // Servo
#include <ESP32PWM.h>           // Servo
//#include <analogWrite.h>      // NOT USED
//#include <ESP32Tone.h>        // NOT USED

AsyncWebServer server(80);

// USER SPECIFIED 
// NETWORK CREDENTIALS
const char* ssid = "REPLACE_WITH_WIFI_NAME";          // BUFALO24
const char* password = "REPLACE_WITH_WIFI_PASSWORD";  // arnis070668

// VALVE POSITIONS IN DEGREESE
#define VLV_POS_OPEN 0
#define VLV_POS_CLOSED 180

// WATER FLOW RATE (m^3/s)
#define WATER_FLOWRATE 0.00544 // Currently with pipe diam - 3cm ; pipe lenght - 0.3m ; drop - 0.5m

// Pins
#define PIN_DHT22 23
#define PIN_LED 19
#define PIN_FAN 18
#define PIN_HEATER 17
#define PIN_SERVO 16

// Defines servo
Servo waterServo;

// Defines DHT22
#define DHTTYPE DHT22

// Attaches DHT22
DHT dht(PIN_DHT22, DHTTYPE);

// Input params for webserver upload process
const char* PARAM_TEMP = "TrgtTemp";
const char* PARAM_HUMD = "TrgtHumid";
const char* PARAM_WPP = "TrgtWPP";
const char* PARAM_WTL = "TrgtWTL";
const char* PARAM_LED = "CrntLED";
const char* PARAM_FAN = "CrntFan";
const char* PARAM_HEAT = "CrntHeat";
const char* PARAM_VLV = "CrntVLV";
const char* PARAM_WTR = "FrcWTR";

const char* PARAM_LNL = "TrgtLedONl";
const char* PARAM_LFL = "TrgtLedOFFl";
const char* PARAM_TTNR = "FrcLedTTOnR";
const char* PARAM_TTFR = "FrcLedTTOffR";

// INPUTS FROM WEBPAGE
// TrgtTemp  TrgtHumid  TrgtWPP  TrgtWTL  CrntLED  CrntFan  CrntHeat  CrntVLV(replaced FrcWP)  FrcWTR

// INPUTS FROM SENSORS
// CrntTemp  CrntHumid

// CALCULATED
// TimeToNextPour  CrntUpTime

// Global variables
int GVTrgtTemp;        // Target Temperature
int GVTrgtHumid;       // Target Humidity
int GVTrgtWPP;         // Target Water Per Pour
int GVTrgtWTL;         // Target Water Timer Length
int GVTrgtLedONl;      // How long will LEDs stay on
int GVTrgtLedOFFl;     // How long will LED stay off
int GVCrntLED = 3;     // Current LED state           (0 auto off, 1 auto on, 2 forced off, 3 forced on)
int GVCrntFan;         // Current Fan state           (0 auto off, 1 auto on, 2 forced off, 3 forced on)
int GVCrntHeat;        // Current Heater state        (0 auto off, 1 auto on, 2 forced off, 3 forced on)
int GVCrntVLV;         // Current Valve state         (0 auto closed, 1 auto open, 2 forced closed, 3 forced open)
int GVFrcWTR;          // Force Water Timer Restart
int GVFrcLedTTOnR;     // Force restart LED Time till ON timer
int GVFrcLedTTOffR;    // Force restart LED Time till OFF timer

float GVCrntTemp;      // Current Temperature
float GVCrntHumid;     // Current Humidity

int GVTimeToNextPour;  // Time left until Next water Pour
int GVTimeToLedON;     // Time left until LED turns ON
int GVTimeToLedOFF;    // Time left until LED turns OFF

// NOT SHOWN ON WEBPAGE
unsigned long GVTTNPstart; // Millis at which "GVTimeToNextPour" timer starts
unsigned long GVLedTimeTillONStart; // Millis at which "GVTimeToLedON" timer starts
unsigned long GVLedTimeTillOFFStart; // Millis at which "GVTimeToLedOFF" timer starts

int GVWaterHandlingState = 0; // 0 - Set GVTTNPstart ; 1 - Wait ; 2 - Start water ; 3 - Wait ; 4 - End water and Run 0

int GVifAutoWater = 1; // Determines if water handling is automated or forced (0 - FORCED ; 1 - AUTOMATED)

int pos = 0; // Current servo position in degrees

String inputForUpload; // String variable, global variables write to this variable -> this variable writes to SPIFFS

// HTML web page code, anything between two precent symbols " %EXAMPLE% " is a placeholder, that is later replaced by SPIFFS file values, by the same name " EXAMPLE.txt "
// submitMessage() function is called when user presses any submit button. The function gives a pop-up alert and afterwards refreshes the page
// <style> contains CSS code, for styling the page

// The web server variable updating works like this:
// Placeholders in the HTML code are replaced by SPIFFS values, when the main page is loaded, but the SPIFFS values, that are placed in, have to be updated in the ESP32 code.
// When a user accesses the main page "CURRENT_IP/" the "server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)" function is called, that is located in the "setup" function and updates multiple SPIFFS files.
// When any value is inputted and submitted this page is accessed "CURRENT_IP/get?INPUTNAME=INPUTVALUE", where "get" calls "server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request)", "INPUTNAME" is the name of the input getting submitted (this is also what the input value will get written to), "INPUTVALUE" is the inputted value

// UNUSED FONT
// @import url('https://fonts.googleapis.com/css?family=Courier%20Prime:700|Courier%20Prime:400');

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
    <head>
        <title>ESP Input Form</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <script>
            function submitMessage() {
                alert("Saved value to ESP SPIFFS");
                setTimeout(function(){ document.location.reload(false); }, 500);
            }
        </script>
        <style>
            
            body{
                background-color: #0A1802;
                font-family: 'Courier Prime';
                font-weight: 700;
            }
            summary{
                width: 260px;
                height: 20px;
                cursor: pointer;
                background-color: #A7E587;
                color: #0A1802;
                border: 2px solid #77D441;
                margin: 10px;
                margin-left: 10px;
                padding: 20px;
                border-radius: 16px;
            }
            form{
                width: 470px;
                height: auto;
                background-color: #1D8A6E;
                color: #0A1802;
                border: 2px solid #77D441;
                margin: 10px;
                margin-left: 30px;
                padding: 10px;
                border-radius: 16px;            
            }
            form.Output-form{
                width: 520px;
                height: auto;
                font-size:larger;
                margin-left: 10px;
            }
            input{
                color: #37ffcd;
                background-color: #0A1802;
                border: 2px solid #77D441;
                margin-bottom: 4px;
            }
            input.Input-box{
                width: 250px;
            }
            input.Output-box{
                width: auto;
                margin-left: -7px;
                margin-right: -5px;
            }
        </style>
    </head>
    <body>
        <!-- OUTPUTS -->
        <form class="Output-form">
            Current Inside Air Temp: <input type="text" disabled value="%CrntTemp%" class="Output-box" name="CrntTemp"> C <br> Target: %TrgtTemp% C
        </form><br>

        <form class="Output-form">
            Current Inside Air Humid: <input type="text" disabled value="%CrntHumid%" class="Output-box" name="CrntHumid"> perc. <br> Target: %TrgtHumid% perc.
        </form><br>

        <form class="Output-form">
            LED state: <input type="text" disabled value="%CrntLED%" class="Output-box" name="CrntLED"> <br> OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)
        </form><br>

        <form class="Output-form">
            Fan state: <input type="text" disabled value="%CrntFan%" class="Output-box" name="CrntFan"> <br> OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)
        </form><br>

        <form class="Output-form">
            Heater state: <input type="text" disabled value="%CrntHeat%" class="Output-box" name="CrntHeat"> <br> OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)
        </form><br>

        <form class="Output-form">
            Valve state: <input type="text" disabled value="%CrntVLV%" class="Output-box" name="CrntVLV"> <br> CLOSED(0) / OPEN(1) / CLOSED F.(2) / OPEN F.(3)
        </form><br>

        <form class="Output-form">
            Time to next pour: <input type="text" disabled value="%TimeToNextPour%" class="Output-box" name="TimeToNextPour"> ms
        </form><br>

        <form class="Output-form">
            Water timer length: <input type="text" disabled value="%TrgtWTL%" class="Output-box" name="CrntWTL"> ms
        </form><br>

        <form class="Output-form">
            LED time till ON: <input type="text" disabled value="%TimeToLedON%" class="Output-box" name="TimeToLedON"> ms
        </form><br>

        <form class="Output-form">
            LED time till OFF: <input type="text" disabled value="%TimeToLedOFF%" class="Output-box" name="TimeToLedOFF"> ms
        </form><br>

        <form class="Output-form">
            LED ON length: <input type="text" disabled value="%TrgtLedONl%" class="Output-box" name="TrgtLedONl"> ms
        </form><br>

        <form class="Output-form">
            LED OFF length: <input type="text" disabled value="%TrgtLedOFFl%" class="Output-box" name="TrgtLedOFFl"> ms
        </form><br>

        <form class="Output-form">
            Time since StartUp: <input type="text" disabled value="%CrntUpTime%" class="Output-box" name="CrntUpTime"> ms <br> 
        </form><br>

        <!-- INPUTS -->
        <details>
            <summary class="pointer">Click to open inputs</summary>
            <span>
                <form action="/get" target="hidden-form">
                    Target Air Temp: <input type="number" value="%TrgtTemp%" class="Input-box" placeholder="5 - 40C" min="5" max="40" name="TrgtTemp"> C <br> (5 - 40C)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Target Air Humid: <input type="number" value="%TrgtHumid%" class="Input-box" placeholder="10 - 90 perc." min="10" max="90" name="TrgtHumid"> perc. <br> (10 - 90 perc.)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Water Amount per Pour: <input type="number" value="%TrgtWPP%" class="Input-box" placeholder="0 - 100000 ml" min="0" max="100000" name="TrgtWPP"> ml <br> (0 - 100000 ml)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Water Timer Length: <input type="number" value="%TrgtWTL%" class="Input-box" placeholder="0 - 2bil ms" min="0" max="2000000000" name="TrgtWTL"> ms <br> (0 - 2bil ms)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    LED ON Length: <input type="number" value="%TrgtLedONl%" class="Input-box" placeholder="0 - 2bil ms" min="0" max="2000000000" name="TrgtLedONl"> ms <br> (0 - 2bil ms)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    LED OFF Length: <input type="number" value="%TrgtLedOFFl%" class="Input-box" placeholder="0 - 2bil ms" min="0" max="2000000000" name="TrgtLedOFFl"> ms <br> (0 - 2bil ms)
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>
                
                <form action="/get" target="hidden-form">
                    Force LED state: <input type="number" value="%CrntLED%" class="Input-box" placeholder="OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)" min="0" max="3" name="CrntLED"> <br> (OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)) <br> 
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Force Fan state: <input type="number" value="%CrntFan%" class="Input-box" placeholder="OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)" min="0" max="3" name="CrntFan"> <br> (OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)) <br> 
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Force Heater state: <input type="number" value="%CrntHeat%" class="Input-box" placeholder="OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)" min="0" max="3" name="CrntHeat"> <br> (OFF(0) / ON(1) / OFF FORCED(2) / ON FORCED(3)) <br> 
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Force Valve state: <input type="number" value="%CrntVLV%" class="Input-box" placeholder="CLOSED(0) / OPEN(1) / CLOSED F.(2) / OPEN F.(3)" min="0" max="3" name="CrntVLV"> <br> CLOSED(0) / OPEN(1) / CLOSED F.(2) / OPEN F.(3) <br> 
                    <input type="submit" value="Submit" onclick="submitMessage()">
                </form><br>

                <form action="/get" target="hidden-form">
                    Force Water timer reset: <input type="number" value="%FrcWTR%" class="Input-box" placeholder="Type 1 and press GO" min="0" max="1" name="FrcWTR"> <br> (Type 1 and press GO)
                    <input type="submit" value="GO" onclick="submitMessage()">
                </form>

                <form action="/get" target="hidden-form">
                    Force LED time till ON reset: <input type="number" value="%FrcLedTTOnR%" class="Input-box" placeholder="Type 1 and press GO" min="0" max="1" name="FrcLedTTOnR"> <br> (Type 1 and press GO)
                    <input type="submit" value="GO" onclick="submitMessage()">
                </form>

                <form action="/get" target="hidden-form">
                    Force LED time till OFF reset: <input type="number" value="%FrcLedTTOffR%" class="Input-box" placeholder="Type 1 and press GO" min="0" max="1" name="FrcLedTTOffR"> <br> (Type 1 and press GO)
                    <input type="submit" value="GO" onclick="submitMessage()">
                </form>

            </span>
        </details>
        <iframe style="display:none" name="hidden-form"></iframe>
    </body>
</html>
)rawliteral";

// Function for displaying web pages and requests, that dont exist
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Function for reading SPIFFS files
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent+=String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}

// Function for writing SPIFFS files
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

// Function for returning requested SPIFFS file values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TrgtTemp"){
    return readFile(SPIFFS, "/TrgtTemp.txt");
  }
  else if(var == "TrgtHumid"){
    return readFile(SPIFFS, "/TrgtHumid.txt");
  }
  else if(var == "TrgtWPP"){
    return readFile(SPIFFS, "/TrgtWPP.txt");
  }
  else if(var == "TrgtWTL"){
    return readFile(SPIFFS, "/TrgtWTL.txt");
  }
  else if(var == "CrntLED"){
    return readFile(SPIFFS, "/CrntLED.txt");
  }
  else if(var == "CrntFan"){
    return readFile(SPIFFS, "/CrntFan.txt");
  }
  else if(var == "CrntHeat"){
    return readFile(SPIFFS, "/CrntHeat.txt");
  }
  else if(var == "CrntVLV"){
    return readFile(SPIFFS, "/CrntVLV.txt");
  }
  else if(var == "FrcWTR"){
    return readFile(SPIFFS, "/FrcWTR.txt");
  }
  else if(var == "CrntTemp"){
    return readFile(SPIFFS, "/CrntTemp.txt");
  }
  else if(var == "CrntHumid"){
    return readFile(SPIFFS, "/CrntHumid.txt");
  }
  else if(var == "TimeToNextPour"){
    return readFile(SPIFFS, "/TimeToNextPour.txt");
  }
  else if(var == "TrgtLedONl"){
    return readFile(SPIFFS, "/TrgtLedONl.txt");
  }
  else if(var == "TrgtLedOFFl"){
    return readFile(SPIFFS, "/TrgtLedOFFl.txt");
  }
  else if(var == "TimeToLedON"){
    return readFile(SPIFFS, "/TimeToLedON.txt");
  }
  else if(var == "TimeToLedOFF"){
    return readFile(SPIFFS, "/TimeToLedOFF.txt");
  }
  else if(var == "FrcLedTTOnR"){
    return readFile(SPIFFS, "/FrcLedTTOnR.txt");
  }
  else if(var == "FrcLedTTOffR"){
    return readFile(SPIFFS, "/FrcLedTTOffR.txt");
  }
  else if(var == "CrntUpTime"){
    return readFile(SPIFFS, "/CrntUpTime.txt");
  }  
  return String();
}

// Function for listing all existing SPIFFS files
void listAllFiles(){

  Serial.println("\nSPIFFS file check:");

  // Accsseses the "SPIFFS/" directory
  File root = SPIFFS.open("/");
 
  File file = root.openNextFile();

  // If any file exists in the current directory print "FILE: FILENAME" and move to next file
  while(file){
 
    Serial.print("FILE: ");
    Serial.println(file.name());
 
    file = root.openNextFile();
  }
  // When no more files remain, print empty line

  Serial.println("");
}

// Converts the values of these SPIFFS file to global variables for easier use in ESP32 code  
void SpiffsToGlobalVar(){
  // Temporary middle man file
  String inputMessage;

  // Write "/TrgtTemp.txt" value to temporary file. 
  inputMessage = readFile(SPIFFS, "/TrgtTemp.txt");
  // Write temporary file to global variable
  GVTrgtTemp = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/TrgtHumid.txt");
  GVTrgtHumid = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/TrgtWPP.txt");
  GVTrgtWPP = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/TrgtWTL.txt");
  GVTrgtWTL = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/CrntLED.txt");
  GVCrntLED = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/CrntFan.txt");
  GVCrntFan = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/CrntHeat.txt");
  GVCrntHeat = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/CrntVLV.txt");
  GVCrntVLV = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/FrcWTR.txt");
  GVFrcWTR = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/TimeToNextPour.txt");
  GVTimeToNextPour = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/TrgtLedONl.txt");
  GVTrgtLedONl = inputMessage.toInt();
  
  inputMessage = readFile(SPIFFS, "/TrgtLedOFFl.txt");
  GVTrgtLedOFFl = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/FrcLedTTOnR.txt");
  GVFrcLedTTOnR = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/FrcLedTTOffR.txt");
  GVFrcLedTTOffR = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/TimeToLedON.txt");
  GVTimeToLedON = inputMessage.toInt();

  inputMessage = readFile(SPIFFS, "/TimeToLedOFF.txt");
  GVTimeToLedOFF = inputMessage.toInt();

}

// setup() is called only once, when system is powered on
// However "server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)" and "server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request)" parts of the setup are called any time the user accesses the corresponding web page
void setup() {
  // Starts serial monitor at 115200 baud rate
  Serial.begin(115200);

  // Initializes pins using predefined values
  pinMode (PIN_LED, OUTPUT);
  pinMode (PIN_FAN, OUTPUT);
  pinMode (PIN_HEATER, OUTPUT);

  // Initializes servo control pin
  waterServo.attach(PIN_SERVO);

  // Starts DHT22
  dht.begin();

  // Initializes SPIFFS
  #ifdef ESP32
    if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
  #else
    if(!SPIFFS.begin()){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
  #endif

  // Lists all saved SPIFFS files, these files persist through system restarts
  listAllFiles();

  // SPIFFS are written to global variable, ensuring that any saved values are carried over, when system is restarted
  SpiffsToGlobalVar();

  // Starts and connects to WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  Serial.println();
  // PRINTS IP ADDRESS, FOR CONNECTING TO WEB PAGE
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Function is called when user connects to main web page "CURRENT_IP/"
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // Sends HTML code to user
    request->send_P(200, "text/html", index_html, processor);

    Serial.println("Client connected to main page");

    // Updates the following values form DHT22 sensor and ESP32 millis clock:
    // Current temperature
    inputForUpload = GVCrntTemp;
    writeFile(SPIFFS, "/CrntTemp.txt", inputForUpload.c_str());
    // Current humidity
    inputForUpload = GVCrntHumid;
    writeFile(SPIFFS, "/CrntHumid.txt", inputForUpload.c_str());
    // Time till next pour
    inputForUpload = GVTimeToNextPour;
    writeFile(SPIFFS, "/TimeToNextPour.txt", inputForUpload.c_str());
    // Times since last system start up
    inputForUpload = millis();
    writeFile(SPIFFS, "/CrntUpTime.txt", inputForUpload.c_str());
    // Time till LED turns ON
    inputForUpload = GVTimeToLedON;
    writeFile(SPIFFS, "/TimeToLedON.txt", inputForUpload.c_str());
    // Time till LED turns OFF
    inputForUpload = GVTimeToLedOFF;
    writeFile(SPIFFS, "/TimeToLedOFF.txt", inputForUpload.c_str());
  });

  // Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    if (request->hasParam(PARAM_TEMP)) {
      inputMessage = request->getParam(PARAM_TEMP)->value();
      GVTrgtTemp = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtTemp.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_HUMD)) {
      inputMessage = request->getParam(PARAM_HUMD)->value();
      GVTrgtHumid = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtHumid.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_WPP)) {
      inputMessage = request->getParam(PARAM_WPP)->value();
      GVTrgtWPP = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtWPP.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_WTL)) {
      inputMessage = request->getParam(PARAM_WTL)->value();
      GVTrgtWTL = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtWTL.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_LED)) {
      inputMessage = request->getParam(PARAM_LED)->value();
      GVCrntLED = inputMessage.toInt();
      writeFile(SPIFFS, "/CrntLED.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_FAN)) {
      inputMessage = request->getParam(PARAM_FAN)->value();
      GVCrntFan = inputMessage.toInt();
      writeFile(SPIFFS, "/CrntFan.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_HEAT)) {
      inputMessage = request->getParam(PARAM_HEAT)->value();
      GVCrntHeat = inputMessage.toInt();
      writeFile(SPIFFS, "/CrntHeat.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_VLV)) {
      inputMessage = request->getParam(PARAM_VLV)->value();
      GVCrntVLV = inputMessage.toInt();
      writeFile(SPIFFS, "/CrntVLV.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_WTR)) {
      inputMessage = request->getParam(PARAM_WTR)->value();
      GVFrcWTR = inputMessage.toInt();
      writeFile(SPIFFS, "/FrcWTR.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_LNL)) {
      inputMessage = request->getParam(PARAM_LNL)->value();
      GVTrgtLedONl = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtLedONl.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_LFL)) {
      inputMessage = request->getParam(PARAM_LFL)->value();
      GVTrgtLedOFFl = inputMessage.toInt();
      writeFile(SPIFFS, "/TrgtLedOFFl.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_TTNR)) {
      inputMessage = request->getParam(PARAM_TTNR)->value();
      GVFrcLedTTOnR = inputMessage.toInt();
      writeFile(SPIFFS, "/FrcLedTTOnR.txt", inputMessage.c_str());
    }
    else if (request->hasParam(PARAM_TTFR)) {
      inputMessage = request->getParam(PARAM_TTFR)->value();
      GVFrcLedTTOffR = inputMessage.toInt();
      writeFile(SPIFFS, "/FrcLedTTOffR.txt", inputMessage.c_str());
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/text", inputMessage);
  });
  server.onNotFound(notFound);
  server.begin();

  Serial.print("// Start up done in ");
  Serial.print(millis());
  Serial.println("ms //");
}

// Reads temperature with DHT22 and, if successful, updates current temperatures global variable and SPIFFS file
void readTemp(){
  
  float t = dht.readTemperature();

  if(isnan(t)){
    return;
  }
  else{
    GVCrntTemp = t;
    inputForUpload = GVCrntTemp;
    writeFile(SPIFFS, "/CrntTemp.txt", inputForUpload.c_str());
    Serial.print("Temperature set to:" );
    Serial.println(t);
  }
}

// Reads humidity with DHT22 and, if successful, updates current humidities global variable and SPIFFS file
void readHumid(){

  float h = dht.readHumidity();

  if(isnan(h)){
    return;
  }
  else{
    GVCrntHumid = h;
    inputForUpload = GVCrntHumid;
    writeFile(SPIFFS, "/CrntHumid.txt", inputForUpload.c_str());
    Serial.print("Humidity set to:" );
    Serial.println(h);
  }
}

// Compares if current temperature global variable is too high, too low or close enough to target temperature global variable
// Controls heater accordingly
void tempHandling(){
  Serial.println("Handling temp");
  if(GVCrntTemp > GVTrgtTemp - 1 && GVCrntTemp < GVTrgtTemp + 1){
    heaterHandling(0);
  }
  else if (GVCrntTemp < GVTrgtTemp - 2){
    heaterHandling(1);
  }
  else if (GVCrntTemp > GVTrgtTemp + 2){
    heaterHandling(0);
  }
}

// Compares if current humidity global variable is too high or current temperature global variable is too high
// Controls fan accordingly
void humidHandling(){
  Serial.println("Handling humidity");
  if(GVCrntTemp > GVTrgtTemp + 2 || GVCrntHumid > GVTrgtHumid + 3){
    fanHandling(1);
  }
  else{
    fanHandling(0);
  }
}

// Starts or stops fan, depending on the global fan state variable and functions called variable
void fanHandling(int fanStateCall){
  Serial.print(GVCrntFan);
  Serial.print(" <- Global var | Fan handling | Call var -> ");
  Serial.println(fanStateCall);

  // if global fan state variable is 3 or 2, fan is forced ON or OFF
  if(GVCrntFan == 3){
    digitalWrite(PIN_FAN, HIGH);
  }
  else if(GVCrntFan == 2){
    digitalWrite(PIN_FAN, LOW);
  }
  else{
    // else if function is called with 1 or 0, fan is auto ON or OFF 
    if(fanStateCall == 1)
    {
      GVCrntFan = 1;
      digitalWrite(PIN_FAN, HIGH);
    }
    else if(fanStateCall == 0){
      GVCrntFan = 0;
      digitalWrite(PIN_FAN, LOW);
    }
    // if fan value was updated automaticaly, update fan state SPIFFS file with new value
    inputForUpload = GVCrntFan;
    writeFile(SPIFFS, "/CrntFan.txt", inputForUpload.c_str());
  }
  
}

// Starts or stops heater, depending on the global heater state variable and functions called variable
// Almost identical to fanHandling
void heaterHandling(int heatStateCall){
  Serial.print(GVCrntHeat);
  Serial.print(" <- Global var | Heater handling | Call var -> ");
  Serial.println(heatStateCall);

  if(GVCrntHeat == 3){
    digitalWrite(PIN_HEATER, HIGH);
  }
  else if(GVCrntHeat == 2){
    digitalWrite(PIN_HEATER, LOW);
  }
  else{
    if(heatStateCall == 1)
    {
      GVCrntHeat = 1;
      digitalWrite(PIN_HEATER, HIGH);
    }
    else if(heatStateCall == 0){
      GVCrntHeat = 0;
      digitalWrite(PIN_HEATER, LOW);
    }
    inputForUpload = GVCrntHeat;
    writeFile(SPIFFS, "/CrntHeat.txt", inputForUpload.c_str());
  }
}

// Delay function, that delays in 10 ms increments, so that Async webserver can check for user requests
void softDelay(unsigned long delayLength){
  Serial.print("Soft delaying loop for ");
  Serial.print(delayLength);
  Serial.print(" ms");

  unsigned long delayStart = millis();
  while(millis() - delayStart <= delayLength){
    Serial.print(".");
    delay(10);
  }
  Serial.println("Resuming");
}

// Starts global water timer, also used to restart the timer
void startWaterTimer(){
  Serial.println("Starting Water Timer");
  GVTTNPstart = millis();
}

// Checks if water timer has expired
// Also updates automatic water handling progress
void checkWaterTimer(){
  unsigned long duration = GVTrgtWTL;
  unsigned long passed = millis() - GVTTNPstart;

  //Serial.print("Water check - Duration - ");
  //Serial.println(duration);
  //Serial.print("Water check - Passed - ");
  //Serial.println(passed);

  // If timerNow - timerStart >= timerDuration
  if(passed >= duration){
    Serial.println("Water timer has expired");
    GVTTNPstart = 0;
    GVWaterHandlingState = 2;
  }
  else{
    GVTimeToNextPour = duration - passed;
    Serial.print("Water timer remaining duration: " );
    Serial.println(GVTimeToNextPour);

    softDelay(500);
  }
}

// Opens water valve
void startWater(){
  Serial.println("Opening Water Valve // START");

  // Opens water valve servo at 1 degree per 15ms
  if(pos >= 179){
    for (pos = VLV_POS_CLOSED; pos >= VLV_POS_OPEN; pos -= 1) {
      waterServo.write(pos);
      delay(15);
    }
    Serial.println("Opening Water Valve // END");
  }
}

// Closes water valve
void endWater(){
  Serial.println("Closing Water Valve // START");

  // Closes water valve servo at 1 degree per 15ms
  if(pos <= 1){
    for (pos = VLV_POS_OPEN; pos <= VLV_POS_CLOSED; pos += 1) {
      waterServo.write(pos);
      delay(15);
    }
    Serial.println("Closing Water Valve // END");
  }
}

// Forced and automated water and water timer handler
void waterHandling(){
  Serial.println("Handling water");
  
  // If global variable valve state is 3 or 2 -> forced ON or OFF
  if(GVCrntVLV == 3){
    startWater();
    GVifAutoWater = 0;
  }
  else if(GVCrntVLV == 2){
    endWater();
    GVifAutoWater = 0;
  }
  else{
    // Else engage automatic water system
    Serial.println("Starting auto water");
    GVifAutoWater = 1;
  }

  // global variable force water timer reset is 1, set it and automated water system progress to 0 (restart it)
  if(GVFrcWTR == 1){
    Serial.println("Forcing water timer reset");
    GVFrcWTR = 0;
    GVWaterHandlingState = 0;
    inputForUpload = GVFrcWTR;
    writeFile(SPIFFS, "/FrcWTR.txt", inputForUpload.c_str());
  }

  // If automatic water system is 0 (OFF) skip it
  if(GVifAutoWater == 0){
    Serial.println("Skipping auto water hangling");
    softDelay(500);
  }

  else{
    // Automated water system has 3 stages:
    // Stage 0 - Starts water timer and go to Stage 1
    if(GVWaterHandlingState == 0){
      Serial.println("Auto water state 0 // START");
      startWaterTimer();
      GVWaterHandlingState = 1;
    }

    // Stage 1 - Checks water timer, which, when expired, will set progress to Stage 2
    else if(GVWaterHandlingState == 1){
      Serial.println("Auto water state 1 // START");
      checkWaterTimer();
    }

    else if(GVWaterHandlingState == 2){
      // Stage 2 - Open water valve, Calculates how long water valve has to be open, to drop users set amount of water, soft delays for that long, Closes water valve, Sets Stage to 0  
      Serial.println("Auto water state 2 // START");
      startWater();
      int delayTime = GVTrgtWPP / (WATER_FLOWRATE * 1000);
      Serial.print("Delay for: ");
      Serial.print(delayTime);
      Serial.println(" ms");
      softDelay(delayTime);
      endWater();
      GVWaterHandlingState = 0;
    }
  }
}

// Start timer, that goes while LED is OFF
void startLedTimeTillON(){
  GVLedTimeTillONStart = millis();
}

// Start timer, that goes while LED is ON
void startLedTimeTillOFF(){
  GVLedTimeTillOFFStart = millis();
}

// Checks if LED time till ON timer has expired and returns: Expired(true) , In progress(false)
bool checkLedTimeTillON(){
  unsigned long LEDTONduration = GVTrgtLedOFFl;
  unsigned long LEDTONpassed = millis() - GVLedTimeTillONStart; 

  if(LEDTONpassed >= LEDTONduration){
    Serial.println("LED time till ON expired");
    return true;
  }
  else{
    GVTimeToLedON = LEDTONduration - LEDTONpassed;
    Serial.print("LED OFF remaining: " );
    Serial.println(GVTimeToLedON);
    return false;
  }
}

// Checks if LED time till OFF timer has expired and returns: Expired(true) , In progress(false)
bool checkLedTimeTillOFF(){
  unsigned long LEDTOFFduration = GVTrgtLedONl;
  unsigned long LEDTOFFpassed = millis() - GVLedTimeTillOFFStart; 

  if(LEDTOFFpassed >= LEDTOFFduration){
    Serial.println("LED time till OFF expired");
    return true;
  }
  else{
    GVTimeToLedOFF = LEDTOFFduration - LEDTOFFpassed;
    Serial.print("LED ON remaining: " );
    Serial.println(GVTimeToLedOFF);
    return false;
  }
}

// Handles LED lighting
void ledHangling(){

  // Checks if LED timers have to be reset
  if(GVFrcLedTTOffR == 1){
    GVFrcLedTTOffR = 0;
    startLedTimeTillOFF();
    inputForUpload = GVFrcLedTTOffR;
    writeFile(SPIFFS, "/FrcLedTTOffR.txt", inputForUpload.c_str());

  }
  else if(GVFrcLedTTOnR == 1){
    GVFrcLedTTOnR = 0;
    startLedTimeTillON();
    inputForUpload = GVFrcLedTTOnR;
    writeFile(SPIFFS, "/FrcLedTTOnR.txt", inputForUpload.c_str());
  }

  // LED is forced ON(3) or OFF(2)
  Serial.println("Handling LEDs");
  if(GVCrntLED == 3){
    Serial.println("LED forced ON");
    digitalWrite(PIN_LED, HIGH);
  }
  else if(GVCrntLED == 2){
    Serial.println("LED forced OFF");
    digitalWrite(PIN_LED, LOW);
  }
  // If LED is not forced, then auto LED
  else{  
    Serial.println("Auto LED start");

    // If LED is OFF and time till ON has expired -> execute the following
    if(GVCrntLED == 0 && checkLedTimeTillON() == true){

      Serial.println("Auto LED, LED turned ON");
      digitalWrite(PIN_LED, HIGH);
      startLedTimeTillOFF();
      GVTimeToLedON = 0;
      GVCrntLED = 1;

      inputForUpload = GVCrntLED;
      writeFile(SPIFFS, "/CrntLED.txt", inputForUpload.c_str());
    }
    // Else if LED is ON and time till OF has expired -> execute the following
    else if(GVCrntLED == 1 && checkLedTimeTillOFF() == true){
      
      Serial.println("Auto LED, LED turned OFF");
      digitalWrite(PIN_LED, LOW);
      startLedTimeTillON();
      GVTimeToLedOFF = 0;
      GVCrntLED = 0;

      inputForUpload = GVCrntLED;
      writeFile(SPIFFS, "/CrntLED.txt", inputForUpload.c_str());
    }

  }
}

// Loop function is executed continuously
void loop() {
  // Denotes the start of a new loop in the serial monitor and starts a timer
  Serial.println("\n// New loop //");
  unsigned long loopMillis = millis();

  // Calls the following functions:
  readTemp();
  readHumid();
  tempHandling();
  humidHandling();
  ledHangling();
  waterHandling();

  // Denotes the end of a loop in the serial monitor and prints how long this loop took, to execute
  Serial.print("// Loop done in ");
  loopMillis = millis() - loopMillis;
  Serial.print(loopMillis);
  Serial.println("ms //\n");
}