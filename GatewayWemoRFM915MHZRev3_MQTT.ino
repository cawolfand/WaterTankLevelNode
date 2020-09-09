// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption, and Automatic Transmission Control
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// RFM69 library and sample code by Felix Rusu - http://LowPowerLab.com/contact
// Copyright Felix Rusu (2015)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <PubSubClient.h>   //still good for wemo??

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>      //comes with Arduino IDE (www.arduino.cc)
//#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        1    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HCW    true //uncomment only for RFM69HW! Leave out if you have RFM69W!
//#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************
#define RFM69_CS      15  // GPIO15/HCS/D8
#define RFM69_IRQ     4   // GPIO04/D2
#define RFM69_IRQN     digitalPinToInterrupt(RFM69_IRQ)
#define RFM69_RST     2   // GPIO02/D4
#define LED           0   // GPIO00/D3, onboard blinky for Adafruit Huzzah

#define SERIAL_BAUD   115200

RFM69 radio = RFM69(RFM69_CS, RFM69_IRQ, IS_RFM69HCW, RFM69_IRQN);

bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

MDNSResponder mdns;
WiFiServer server(80);
//char emailServer[] = "mail.smtp2go.com";

const char* ssid = "BUBBLES";
String st;
//char email[64];
char nodeName[20];
int lastState;

//RPI mqtt server
byte mqttServer[] = { 
  192, 168, 1, 180 };

//MQTT stuff
WiFiClient wifiClient;
PubSubClient client(wifiClient);
#define MQTT_CLIENT_ID "Gateway"
#define MQTT_RETRY 500
#define DHCP_RETRY 500
unsigned long keepalivetime=0;
unsigned long MQTT_reconnect=0;
const byte TARGET_ADDRESS = 42;

char  buff_topic[30];       // MQTT publish topic string
bool  mqttCon = false;      // MQTT broker connection flag
char  *clientName = "RFM_gateway";    // MQTT system name of gateway
long  lastMinute = -1;      // timestamp last minute
long  loopUpTime = 0;       // uptime in minutes


ADC_MODE(ADC_VCC);

typedef struct __attribute__((packed)){
  short int           nodeId; //store this nodeId
  unsigned long      uptime; //uptime in ms
  float         sensorData1;   //
  float         sensorData2;   //
  float         sensorData3;   //temperature maybe?
  float         sensorData4;   //temperature maybe?
  float         sensorData5;   //temperature maybe?
} Payload ;
Payload theData;

//typedef struct {    
//  int                   nodeId; 
//  unsigned long         uptime;
//  float                 sensor1Data;
//  float                 sensor2Data;
//  float                 sensor3Data;
//  float                 sensor4Data;
//  float                 sensor5Data;
//   
////  float     var3_float;
////  int                   var4_int;
//} itoc_Send;
//itoc_Send theDataI2C;

// Handing of Mosquitto messages
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
 // DEBUGLN1(F("Mosquitto Callback"));
  Serial.println("in callback");
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
     if (client.connect(MQTT_CLIENT_ID)) {
   // if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println();
  Serial.println("Starting GatewayWemoRfm915MHZRev3_MQTT.ino");
  delay(10);
  //EEPROM.begin(512);
  if (!readEprom()) setupAP();  //eprom read unsuccessful in connecting o setup AP
  
    // Hard Reset the RFM module
pinMode(RFM69_RST, OUTPUT);
digitalWrite(RFM69_RST, HIGH);
delay(100);
digitalWrite(RFM69_RST, LOW);
delay(100);

  if (!radio.initialize(FREQUENCY,NODEID,NETWORKID)) {
    Serial.println("radio.initialize failed!:");
  }
  
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
radio.setPowerLevel(31);
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  
#ifdef ENABLE_ATC
  Serial.println("RFM69_ATC Enabled (Auto Transmission Control)");
#endif


  
  client.setServer(mqttServer,1883);
  client.setCallback(callback);
  
 MQTT_reconnect = millis();
  client.publish("gatewayClient", " connected");
  Serial.println("setup complete");
  while (client.connect("gatewayClient") != 1)
  {
    Serial.println("Error connecting to MQTT");
    delay(3000);
    
  }

}
int testWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");  
  while ( c < 20 ) {
    Serial.print(c);
    if (WiFi.status() == WL_CONNECTED) { return(20); } 
    delay(500);
    Serial.print(WiFi.status());    
    c++;
  }
  Serial.println("Connect timed out, opening AP");
  return(10);
} 
int readEprom()
{
  Serial.println("reading Eprom");
   EEPROM.begin(512);

  

  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("Startup");
  // read eeprom for ssid and pass
  Serial.println("Reading EEPROM ssid");
  String esid;
  for (int i = 0; i < 32; ++i)
    {
      esid += char(EEPROM.read(i));
    }
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
  String epass = "";
  for (int i = 32; i < 96; ++i)
    {
      epass += char(EEPROM.read(i));
    }
   Serial.print("PASS: ");
  Serial.println(epass);    
   Serial.println("Reading EEPROM email");
   String emails = "";
   char c;
   int esize;
   for (int i = 96; i < 160; ++i)
     {
      c = char(EEPROM.read(i));
      if ( c != 0)
       emails += c;
      else 
       {
        esize = i; 
        i=160;  ///fix this!
       }
      }
    Serial.print("Email"); Serial.println(emails);
  //  emails.toCharArray(email,esize);
 Serial.println("Readint EEPROM nodeName");
 String snodeName="";
 for (int i=161 ; i<181; ++i)
   {
    snodeName += char(EEPROM.read(i));
     
   }
   snodeName.toCharArray(nodeName,20);
   
    
  if ( esid.length() > 1 ) {
      // test esid 
      WiFi.begin(esid.c_str(), epass.c_str());
      if ( testWifi() == 20 ) { 
          
           //byte ret = sendEmail();
           //launchWeb(0);
         Serial.println("connected");
          return 1;
      }
    return 0;
  }
}

void launchWeb(int webtype) {
          Serial.println("");
          Serial.println("WiFi connected");
          Serial.println(WiFi.localIP());
          Serial.println(WiFi.softAPIP());
          
          if (!mdns.begin("esp8266", WiFi.localIP())) {
            Serial.println("Error setting up MDNS responder!");
            while(1) { 
              delay(1000);
            }
          }
          Serial.println("mDNS responder started");
          // Start the server
          server.begin();
          Serial.println("Server started");   
          int b = 20;
          int c = 0;
          while(b == 20) { 
             b = mdns1(webtype);
           }
}

void setupAP(void) {
  Serial.println("setting up AP");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
     {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
     }
  }
  Serial.println(""); 
  st = "<ul>";
  for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<li>";
      st +=i + 1;
      st += ": ";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      st += "</li>";
    }
  st += "</ul>";
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  Serial.println("softap");
  Serial.println("");
  launchWeb(1);
  Serial.println("over");
}

int mdns1(int webtype)
{
  // Check for any mDNS queries and send responses
  mdns.update();
  
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return(20);
  }
  Serial.println("");
  Serial.println("New client");

  // Wait for data from client to become available
  while(client.connected() && !client.available()){
    delay(1);
   }
  
  // Read the first line of HTTP request
  String req = client.readStringUntil('\r');
  
  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    Serial.print("Invalid request: ");
    Serial.println(req);
    return(20);
   }
  req = req.substring(addr_start + 1, addr_end);
  Serial.print("Request: ");
  Serial.println(req);
  client.flush(); 
  String s;
  if ( webtype == 1 ) {
      if (req == "/")
      {
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
        s += ipStr;
        s += "<p>";
        s += st;
        s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><br>";
        s += "<label>Password: </label><input name='pass' length=64><br>";
        s += "<label>Email: </label><input name='email' length=64><br>";
        s += "<label>Name: </label><input name='nodeName' length=20><br><input type='submit'></form>";
        s += "</html>\r\n\r\n";
        Serial.println("Sending 200");
      }
      else if ( req.startsWith("/a?ssid=") ) {
        // /a?ssid=blahhhh&pass=poooo
        Serial.println("clearing eeprom");
        for (int i = 0; i < 185; ++i) { EEPROM.write(i, 0); }
        String qsid; 
        int j,k;
        qsid = req.substring(8,req.indexOf('&'));
        Serial.println(qsid);
        Serial.println("");
        
        String qpass;
        qpass = req.substring(req.indexOf("pass=")+5,req.indexOf("&email"));
        Serial.println(qpass);
        Serial.println("");
        int buffer_size = qpass.length()+1;
        char pass_encoded[buffer_size];
        char pass_decoded[buffer_size];
        qpass.toCharArray(pass_encoded, buffer_size);
        int decoded_size = urldecode(pass_decoded,pass_encoded);
        Serial.print("decoded pass ");Serial.println(pass_decoded);
        
        String qemail;
        //qemail = req.substring(req.indexOf("email="+7),req.indexOf("&"));
        qemail = req.substring(req.indexOf("email=")+6,req.indexOf("&nodeName"));
        Serial.print("Email: "); Serial.println(qemail);
        buffer_size = qemail.length()+1;
        char email_encoded[buffer_size];
        char email_decoded[buffer_size];
        qemail.toCharArray(email_encoded, buffer_size);
        int email_decode_size = urldecode(email_decoded,email_encoded);
        Serial.print("Decoded email: "); Serial.println(email_decoded);

        String qnodename;
        qnodename = req.substring(req.lastIndexOf('=')+1);
        
        
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
          {
            EEPROM.write(i, qsid[i]);
            Serial.print("Wrote: ");
            Serial.println(qsid[i]); 
          }
        Serial.println("writing eeprom pass:"); 
        for (int i = 0; i < decoded_size; ++i)
          {
            EEPROM.write(32+i, qpass[i]);
            Serial.print("Wrote: ");
            Serial.println(pass_decoded[i]); 
          }    
        Serial.println("writing eeprom email:");
        for (int i = 0; i < email_decode_size; ++i)
          {
            EEPROM.write(96+i, email_decoded[i]);
            Serial.print("Wrote:");
            Serial.println(email_decoded[i]);
          }    
        Serial.println("writing node name");
        for (int i = 0; i<20; ++i)
          {
            EEPROM.write(161+i, qnodename[i] );
            Serial.print("Wrote:");
            Serial.println(qnodename[i]);
          }

          EEPROM.write(200,1);  //hardcode a reset bit
          
        EEPROM.commit();
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 ";
        s += "Found ";
        s += req;
        s += "<p> saved to eeprom... reset to boot into new wifi</html>\r\n\r\n";
      }
      else
      {
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
        Serial.println("Sending 404");
      }
  } 
  else  //webtype 0
  {
      if (req == "/")
      {
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266";
        s += "<p>";
        s += "</html>\r\n\r\n";
        Serial.println("Sending 200");
      }
      else if ( req.startsWith("/cleareeprom") ) {
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Hello from ESP8266";
        s += "<p>Clearing the EEPROM<p>";
        s += "</html>\r\n\r\n";
        Serial.println("Sending 200");  
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
        EEPROM.commit();
      }
      else
      {
        s = "HTTP/1.1 404 Not Found\r\n\r\n";
        Serial.println("Sending 404");
      }       
  }
  client.print(s);
  Serial.println("Done with client");
  return(20);
}


long lastMsg = 0;
char message[50];
int sendMQTT = 0;
byte ackCount=0;
long watchdogInterval = 50000;
long watchdog = 0;
char buff_message[12];
char fstr[12],hstr[6];


uint32_t packetCount = 0;
int loopcount =0;
int rssi;
void loop() {
 
  //check if something was received (could be an interrupt from the radio)
  if (radio.receiveDone())
  {
    uint8_t senderId;
    int16_t rssi;
    uint8_t data[RF69_MAX_DATA_LEN];
    unsigned long newMessage;

    //save packet because it may be overwritten
    senderId = radio.SENDERID;
    rssi = radio.RSSI;
   // memcpy(data, (void *)radio.DATA, radio.DATALEN);
   if (radio.DATALEN != sizeof(Payload))
   {
      Serial.println("Invalid payload received, not matching payload struct!");
   }
   else
   {
      sendMQTT = 1;
      theData = *(Payload*)radio.DATA;
      Serial.print("from radio id");
      Serial.print(radio.SENDERID);
      Serial.print(" nodeId=");
      Serial.print(theData.nodeId);
      Serial.print(" uptime=");
      Serial.print(theData.uptime);
      Serial.print(" sensor1=");
      Serial.print(theData.sensorData1);
       Serial.print(" sensor2=");
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.println("]");
   }
 
    //check if sender wanted an ACK
    if (radio.ACKRequested())
    {
      radio.sendACK();
    }
    
  } else {
    radio.receiveDone(); //put radio in RX mode
  }
  if (sendMQTT == 1){
     if (!client.connected()) {
       Serial.println("mqtt not connected");
       reconnect();
     }
   
     else
     {
       if(theData.nodeId != 0.0) {
        sprintf(buff_topic, "home/rfm_gw/node%02d/dev01", theData.nodeId);
        Serial.print(buff_topic);Serial.print(": ");Serial.println(theData.sensorData1);
        dtostrf(theData.sensorData1,2,1,fstr);
        client.publish(buff_topic,fstr);
       }
       sendMQTT = 0; 
     }
  }
  Serial.flush(); 
}//end of loop

int urldecode(char *dst, const char *src)
{
  char a, b;
  int new_size = 0;
  while (*src) {
    if ((*src == '%') &&
      ((a = src[1]) && (b = src[2])) &&
      (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a'-'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a'-'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16*a+b;
      src+=3;
    } 
    else {
      *dst++ = *src++;
    }
    new_size += 1;
  }
  *dst++ = '\0';
  return new_size;
}
void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
