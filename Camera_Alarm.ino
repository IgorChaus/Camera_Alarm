#define ESP_BUILTIN_LED 2
#define ESP_RELAY 4
#define ESP_INPUT 5 //D1

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

char auth[] = "XXXXXXXXXXXXXXXXXXXXXXX";
char ssid[] = "XXXXXXXXXXXXXXX";
char pass[] = "XXXXXXXXXX";
char server[] = "XXX.XXX.XXX.XXX";
int port = 8080;

char host[][14] = {"192.168.3.208", "192.168.3.209", "192.168.3.210", "192.168.3.211", "192.168.3.212"};

int val, wifisignal;

WiFiClient client;

BlynkTimer timer;

int camera = 0;

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

//'{"EncryptType": "MD5", "LoginType": "DVRIP-Web", "PassWord": "00000000", "UserName": "admin"}'
char login_packet_bytes[] = {
  0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x03,
  0x5f, 0x00, 0x00, 0x00, 0x7b, 0x22, 0x45, 0x6e,
  0x63, 0x72, 0x79, 0x70, 0x74, 0x54, 0x79, 0x70,
  0x65, 0x22, 0x3a, 0x20, 0x22, 0x4d, 0x44, 0x35,
  0x22, 0x2c, 0x20, 0x22, 0x4c, 0x6f, 0x67, 0x69,
  0x6e, 0x54, 0x79, 0x70, 0x65, 0x22, 0x3a, 0x20,
  0x22, 0x44, 0x56, 0x52, 0x49, 0x50, 0x2d, 0x57,
  0x65, 0x62, 0x22, 0x2c, 0x20, 0x22, 0x50, 0x61,
  0x73, 0x73, 0x57, 0x6f, 0x72, 0x64, 0x22, 0x3a,
  0x20, 0x22, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
  0x30, 0x30, 0x22, 0x2c, 0x20, 0x22, 0x55, 0x73,
  0x65, 0x72, 0x4e, 0x61, 0x6d, 0x65, 0x22, 0x3a,
  0x20, 0x22, 0x61, 0x64, 0x6d, 0x69, 0x6e, 0x22,
  0x7d, 0x0a, 0x00
};

//cam.get_info('Detect.MotionDetect')
//'{"Name": "Detect.MotionDetect", "SessionID": "0x00000006"}'
char get_info_packet_bytes[] = {
  0xff, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x04,
  0x3c, 0x00, 0x00, 0x00, 0x7b, 0x22, 0x4e, 0x61,
  0x6d, 0x65, 0x22, 0x3a, 0x20, 0x22, 0x44, 0x65,
  0x74, 0x65, 0x63, 0x74, 0x2e, 0x4d, 0x6f, 0x74,
  0x69, 0x6f, 0x6e, 0x44, 0x65, 0x74, 0x65, 0x63,
  0x74, 0x22, 0x2c, 0x20, 0x22, 0x53, 0x65, 0x73,
  0x73, 0x69, 0x6f, 0x6e, 0x49, 0x44, 0x22, 0x3a,
  0x20, 0x22, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30,
  0x30, 0x30, 0x30, 0x36, 0x22, 0x7d, 0x0a, 0x00
};

struct {
  byte head;                  //0xff
  byte vers;                  //0x00 - send, 0x01 - receive
  byte byte_zero1;            //0x00
  byte byte_zero2;            //0x00
  unsigned long session;
  unsigned long packet_count;
  byte byte_zero3;            //0x00
  byte byte_zero4;            //0x00  
  word msg;
  unsigned long len_data;
  char data[4000];           
} pack;

byte* ptr = (byte*)&pack;

String data_str;



int read_data(){
    int size_replay = 0;
    while (client.connected()){
      delay(1);
      if (client.available()){
         *((byte*)&pack + size_replay) = client.read();
        size_replay++;        
      }else{
        break;
      }
    }
    return size_replay;
}

boolean login(char host[]){
  memcpy( &login_packet_bytes[82], "tlJwpbo6", 8 );//set password hash(83..88)
  Serial.printf("\n[Connecting to %s ... ", host);
  if (client.connect(host, 34567)){
    Serial.println("connected]");
    client.write(login_packet_bytes, sizeof(login_packet_bytes));
    delay(500);
    if(read_data() > 0)
      return true;
    else{
      Serial.println("athrorezation failed!]");
      client.stop();
      return false;
    }    
  }else{
    Serial.println("connection failed!]");
    client.stop();
    return false;
  }
}


int get_info(){
  memcpy(&get_info_packet_bytes[4], &pack + 5, 4);     //4...7 - set session id
  client.write(get_info_packet_bytes, sizeof(get_info_packet_bytes));
  delay(500);  
  return read_data();
}

void set_info(int val){
  String action;
  if (val == 0)
    action = "false";
  else
    action = "true";

  Serial.print("Action: ");
  Serial.println(action);
      
  pack.data[pack.len_data - 2] = 0; //Deleted end of package '0x10'
  data_str = String(pack.data);

  int index = data_str.lastIndexOf("SessionID");
  index = index + 14;

//     String data = "{'Name': 'Detect.MotionDetect', 'SessionID': '" + json.substring(index, index + 10) + "', ";
//     data = data + "'Detect.MotionDetect': [{'Enable': " + action + " ,'";
//     data = data + json.substring(json.indexOf("EventHandler"), json.lastIndexOf("]")+1) + " }";  
    
  String data = "{\"Name\": \"Detect.MotionDetect\", \"SessionID\": \"" + data_str.substring(index, index + 10) + "\", ";
  data = data + "\"Detect.MotionDetect\": [{\"Enable\": " + action + ", \"";
  data = data + data_str.substring(data_str.indexOf("EventHandler"), data_str.lastIndexOf("]")+1) + " }";  

  pack.vers = 0;
  pack.packet_count++;
  pack.len_data = data.length() + 2;
  pack.msg = 1040;

  memset((byte*)&pack + 20, 0, 4000);                 //clear data    
  data.toCharArray(pack.data, data.length() + 1); 

  pack.data[data.length()] = 0xA;            // end of package 
  pack.data[data.length() + 1] = 0x0;        // end of package 

  client.write((char*)&pack,  data.length() + 2 + 20);
}

void reconnectBlynk() {
  if (!Blynk.connected()) {
    if (Blynk.connect()) {
      BLYNK_LOG("Reconnected");      
    } else {
      BLYNK_LOG("Not reconnected");    
    }
  }
}

void sendWifi() {
  wifisignal = map(WiFi.RSSI(), -105, -40, 0, 100);
  Blynk.virtualWrite(V0,wifisignal);
}

void setup() {
  timer.setInterval(30000L, reconnectBlynk);  // проверяем каждые 30 секунд, если все еще подключен к серверу 
  timer.setInterval(5000L, sendWifi);  // читаем данные каждые 5 секунд, если все еще подключен к серверу   

  WiFi.begin(ssid, pass); 
  Blynk.config(auth, server, port);
  Blynk.connect();
  
  Blynk.disconnect(); //разорвать соединение
  Blynk.connect();  // а потом пробуем уже конектиться
  
  pinMode(ESP_BUILTIN_LED, OUTPUT);
  digitalWrite(ESP_BUILTIN_LED, HIGH);

  pinMode(ESP_INPUT, INPUT);
   
  pinMode(ESP_RELAY, OUTPUT);

  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // строчка для номера порта по умолчанию
  // можно вписать «8266»:
  // ArduinoOTA.setPort(8266);
 
  // строчка для названия хоста по умолчанию;
  // можно вписать «esp8266-[ID чипа]»:
  ArduinoOTA.setHostname("Esp8266-Security");
 
  // строчка для аутентификации
  // (по умолчанию никакой аутентификации не будет):
  // ArduinoOTA.setPassword((const char *)"123");
 
  ArduinoOTA.onStart([]() {
    Serial.println("Start");  //  "Начало OTA-апдейта"
    digitalWrite(ESP_BUILTIN_LED, HIGH);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");  //  "Завершение OTA-апдейта"
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    //  "Ошибка при аутентификации"
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    //  "Ошибка при начале OTA-апдейта"
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    //  "Ошибка при подключении"
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    //  "Ошибка при получении данных"
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    //  "Ошибка при завершении OTA-апдейта"
  });
  
  ArduinoOTA.begin();  

  val = digitalRead(ESP_INPUT);
  digitalWrite(ESP_RELAY, LOW);
}

void loop() {
    timer.run();
    ArduinoOTA.handle();

    if (Blynk.connected()) {
      Blynk.run();   
      digitalWrite(ESP_BUILTIN_LED, LOW);
      
      if (camera > 0) {
        Serial.println(camera);
        if(login(host[camera-1])){
            if(get_info() > 0){
              set_info(val);
            }          
            client.stop();
            camera--;
        }else{
          Serial.print("Cant connect to host: ");
          Serial.println(camera);
          camera--;
        }                 
      }else{
        if(val != digitalRead(ESP_INPUT)){
          camera = sizeof(host)/14;
          Serial.println("Start");
          delay(100);
          val = digitalRead(ESP_INPUT);
        }
      }
    }else
      digitalWrite(ESP_BUILTIN_LED, HIGH); 
}
