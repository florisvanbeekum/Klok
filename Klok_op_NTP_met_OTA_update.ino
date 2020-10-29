/*
 * Rui Santos 
 * Complete Project Details http://randomnerdtutorials.com
 */

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ezTime.h>
#include <FastLED.h>

const char* host = "esp32_klok";
const char* ssid = "Reef19";
const char* password = "fl70an73ma03da05";

WebServer server(80);

/*
 * Login page
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32_klok Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

Timezone myTZ;
String formattedDate;
String formattedTime;
int hours_int = 0;
int hours_int_60 = 0;
int minutes_int = 0;
int seconds_int = 5;
int correction = 0;

// How many leds in your strip?
#define NUM_LEDS 60
#define DATA_PIN 15
CRGB leds[NUM_LEDS];

const int buttonPin = 4;  // the number of the pushbutton pin
int buttonState = 0;
unsigned long startTime = 0;
unsigned long timePast = 0;
unsigned long timeLeft = 0;
unsigned long timerDelay = 1000 * 60 * 10; // 1000 milisec * 60sec * 10 minuten
bool timerRunning = false;
bool playAlarm = false;
int lightIntensity = 64;

/*
 * setup function
 */
void setup(void) 
{
  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  Serial.println("HTTP server started");

  // Uncomment the line below to see what it does behind the scenes van de NTP
  //setDebug(INFO);
  waitForSync();

  // See if local time can be obtained (does not work in countries that span multiple timezones)
  Serial.print(F("Local (GeoIP):   "));
  if (myTZ.setLocation()) 
  {
    Serial.println(myTZ.dateTime());
  } else 
  {
    Serial.println(errorString());
  }
  setInterval(900); // Interval waarmee de NTP wordt opgevraagd. Hier om het kwartier dus

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  pinMode(buttonPin, INPUT);
}

void loop(void) 
{
  server.handleClient();
  delay(1);
  events();

  if (digitalRead(buttonPin) && !timerRunning)
  {
    timerRunning = true;
    startTime = millis();
  }
  if (timerRunning)
  { 
    countDown(timerDelay);
      timerRunning = false;
      playAlarmFunction();
    
  }
  
  
  
  hours_int = myTZ.dateTime("g").toInt();
  minutes_int = myTZ.dateTime("i").toInt();
  seconds_int = myTZ.dateTime("s").toInt();

  

  showTimeFvB();
  
  delay(50);
}

int playAlarmFunction()
{ 
  int pause = 10;
  playAlarm =  true;
  //Rondraaien
  fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
  FastLED.show(); // Zend de gegevens naar de ledstrip
  for (int y = 0; y <= 20; y++) 
  {  
    for (int i = 1; i <= NUM_LEDS; i++)
    {
      int vorigeLed= i - 1;
      int voorgaandeLed = i+1;
      if (i == 0)
      {
        vorigeLed= NUM_LEDS -1;
      }
      if (i == 59)
      {
        voorgaandeLed= 0;
      }
      fill_solid(leds, NUM_LEDS, CRGB(0,0,0));
      leds[vorigeLed] = CRGB(128,0,0);
      leds[i] = CRGB(255,0,0);
      leds[voorgaandeLed] = CRGB(128,0,0);
      FastLED.show(); // Zend de gegevens naar de ledstrip
      delay(pause);
      //leds[i] = CRGB(0,0,0);
    }
  }
  
  //Knipperen
  for (int i = 0; i <= 60; i++) 
  {
    for (int x = 5; x <= 255; x=x+5)
    {
      fill_solid( leds, NUM_LEDS, CRGB(x,0,0));
      FastLED.show(); // Zend de gegevens naar de ledstrip
      delay(pause);
    }
    for (int x = 255; x >= 5; x=x-5)
    {
      fill_solid( leds, NUM_LEDS, CRGB(x,0,0));
      FastLED.show(); // Zend de gegevens naar de ledstrip
      delay(pause);
    }
  }
  fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
  FastLED.show(); // Zend de gegevens naar de ledstri
  playAlarm = false;
}

int countDown(unsigned long Time)
{
  int deltaTime = Time/NUM_LEDS;
  fill_solid( leds, NUM_LEDS, CRGB(255,255,255));
  FastLED.show(); // Zend de gegevens naar de ledstrip
  for (int i = 1; i <= NUM_LEDS; i++)
  {
    int ledUit = NUM_LEDS - i;
    leds[ledUit] = CRGB::Black;
    delay(deltaTime);
    FastLED.show(); // Zend de gegevens naar de ledstrip
    
  }
}

int showTimeFvB()
{
  // omrekenen naar een array 0 t/m 59 van de uren
  // Kleine wijzer langzaam laten draaien, elke $correction minuten 1 ledje opschuiven
  correction = minutes_int / 12;
  if (hours_int == 0 || hours_int == 12)
  {
    hours_int_60 = correction ;
  }
  else
  {    
    hours_int_60 = (hours_int * 5) + correction ; //omrekenen van 12 uur naar 60 ledjes    
  }
  Serial.println((String)hours_int+":"+minutes_int+":"+seconds_int+"  Hours_60: "+hours_int_60+" Correctie: "+correction);
  //Serial.println((String)"startTime: "+startTime+" timeLeft: "+timeLeft+" timerDelay: "+timerDelay+" playAlarm: "+playAlarm);

  // "Oude" Ledjes uitzetten
  fill_solid(leds, NUM_LEDS, CRGB(0,0,0));
  // Ledjes van de nieuwe tijd aanzetten
  leds[hours_int_60].red = lightIntensity;
  leds[minutes_int].green = lightIntensity;
  leds[seconds_int].blue = lightIntensity;
  FastLED.show(); // Zend de gegevens naar de ledstrip
}
