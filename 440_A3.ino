
#include <ESP8266WiFi.h>    
#include <PubSubClient.h>   
#include <ESP8266HTTPClient.h>                            
#include <ArduinoJson.h>                                  
#include <Adafruit_Sensor.h>                              
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <Adafruit_MPL115A2.h>    //Include these libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// pin connected to DH22 data line
#define DATA_PIN 12

// create DHT22 instance
DHT_Unified dht(DATA_PIN, DHT22);

//Create an instance
Adafruit_MPL115A2 mpl115a2;

//////////
//So to clarify, we are connecting to and MQTT server
//that has a login and password authentication
//I hope you remember the user and password
//////////

#define mqtt_server "mediatedspaces.net"  //this is its address, unique to the server
#define mqtt_user "hcdeiot"               //this is its server login, unique to the server
#define mqtt_password "esp8266"           //this is it server password, unique to the server

//////////
//We also need to publish and subscribe to topics, for this sketch are going
//to adopt a topic/subtopic addressing scheme: topic/subtopic
//////////

WiFiClient espClient;             //blah blah blah, espClient
PubSubClient mqtt(espClient);     //blah blah blah, tie PubSub (mqtt) client to WiFi client

//////////
//We need a 'truly' unique client ID for our esp8266, all client names on the server must be unique.
//Every device, app, other MQTT server, etc that connects to an MQTT server must have a unique client ID.
//This is the only way the server can keep every device separate and deal with them as individual devices/apps.
//The client ID is unique to the device.
//////////

char mac[6]; //A MAC address is a 'truly' unique ID for each device, lets use that as our 'truly' unique user ID!!!

//////////
//In our loop(), we are going to create a c-string that will be our message to the MQTT server, we will
//be generous and give ourselves 200 characters in our array, if we need more, just change this number
//////////

char message[201]; //201, as last character in the array is the NULL character, denoting the end of the array

/////SETUP/////
void setup() {
  
  Serial.begin(115200);
  
  // wait for serial monitor to open
  while(! Serial);
  
  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));                            //These four lines give description of of file name and date 
  Serial.print("Complied: ");
  Serial.println(F(__DATE__ " " __TIME__));

 pinMode(16,INPUT_PULLUP);

  // initialize dht22
  dht.begin();

  // initialize MPL
  mpl115a2.begin();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();                     // Clear any existing image
  display.setTextSize(1);                     // Set text size
  display.setTextColor(WHITE);                // Set text color
  display.setCursor(0, 0);                    // Put cursor back on top left
  display.println("Starting up.");            // Test and write up
  display.display();                          // Display the display
  
  setup_wifi();
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); //register the callback function
}

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}                                     //5C:CF:7F:F0:B0:C1 for example

/////CONNECT/RECONNECT/////Monitor the connection to MQTT server, if down, reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("itsForMeeee/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/////LOOP/////
void loop() {

  Serial.println();
  Serial.println("//////////////////");                 //To seperate the sends
  Serial.println();
  
  sensors_event_t event;                                //An event is a grabbing of data, so prep a instance
  dht.temperature().getEvent(&event);                   //Actually grab the data
  
  float celsius = event.temperature;                    //Take temperature from the event
  float fahrenheit = (celsius * 1.8) + 32;              //Convert to Fahrenheit
  Serial.print("Temperature from DHT is: ");            //Print all this
  Serial.print(celsius);
  Serial.println(" *C");

  float pressureKPA = mpl115a2.getPressure();           // Get and print pressure from MPL
  Serial.print("Pressure (kPa): ");
  Serial.print(pressureKPA, 4);
  Serial.println(" kPa");

  float temperatureC = mpl115a2.getTemperature();       // Get and print temperature from MPK
  Serial.print("Temp (*C): ");
  Serial.print(temperatureC, 1);
  Serial.println(" *C");

  float diff = abs(celsius - temperatureC);
  
  display.clearDisplay();                                       //Clear test display off

  display.setCursor(0, 0);                                      //Reset cursor
  display.println("Difference in instruments: ");               //Print that to display
  display.println(diff);                                        //Print that to display too
  display.display();                                            //Display the display

  if (!mqtt.connected()) {                                      //Reconnect if lost connection
    reconnect();
  }

  mqtt.loop();                                                  //this keeps the mqtt connection 'active'

  Serial.print("Diff is ");                                     //Check the difference
  Serial.println(diff);
  Serial.println(digitalRead(16));

  
  if(digitalRead(16) == 0) {
    if (diff < 2) {                                               //If values are close
  
      char str_diff[5];                                           //format diff to send
      dtostrf(diff, 4, 2, str_diff);
      //Serial.print("str_diff is ");                            
      //Serial.println(str_diff);
      
      sprintf(message, "{\"Both instruments are getting the same data, with only a difference of\":\"%s%\"}", str_diff); // Squish these together to form the message
      mqtt.publish("itsForMeeee/weather", message);
    
    }
  
    else if (diff < 10) {                                   //If values are far apart
  
      char str_diff[5];                                     //same as above
      dtostrf(diff, 4, 2, str_diff);
      //Serial.print("str_diff is ");
      //Serial.println(str_diff);
      
      sprintf(message, "{\"Your instruments are getting different data, with a difference of\":\"%s%\"}", str_diff); // Squish these together to form the message
      mqtt.publish("itsForMeeee/weather", message);
    
    }
    
    else {                                              //unexpected values, errors, or crazy far aoart
  
      char str_diff[5];                                 //same
      dtostrf(diff, 4, 2, str_diff);
      Serial.print("str_diff is ");
      Serial.println(str_diff);
      
      sprintf(message, "{\"Check your instruments, theres a difference of\":\"%s%\"}", str_diff); // Squish these together to form the message
      mqtt.publish("itsForMeeee/testing", message);
    
    }
  
  }
  
  delay(3000);          //dont spam messages
}//end Loop


/////CALLBACK/////
//The callback is where we attacch a listener to the incoming messages from the server.
//By subscribing to a specific channel or topic, we can listen to those topics we wish to hear.
//We place the callback in a separate tab so we can edit it easier . . . (will not appear in separate
//tab on github!)
/////

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic); //'topic' refers to the incoming topic name, the 1st argument of the callback function
  Serial.println("] ");


  DynamicJsonBuffer  jsonBuffer; //blah blah blah a DJB
  JsonObject& root = jsonBuffer.parseObject(payload); //parse it!

  if (!root.success()) { //well?
    Serial.println("parseObject() failed, are you sure this message is JSON formatted.");
    return;
  }
}
