#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

#define freq 2000
#define res 8
#define led_in 5 //spinker

const char* get_url = "https://ecourse.cpe.ku.ac.th/exceed07/api/hard_get/";

char str[100];
const char* ssid = "ZisZtR";
const char* password = "asd123456";
const int _size = 2*JSON_OBJECT_SIZE(8);
StaticJsonDocument<_size> JSONGet;

void WiFi_Connect(){
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(".");  
  }
  Serial.println("Connected to network"); 
  Serial.print("IP Adress : "); 
  Serial.println(WiFi.localIP());   
}

//Flame, Water, Shock
int LED_input[3][2] = {{15, 14}, {0, 2}, {33, 32}}; //Red, Green pin  
int LED_Mode[3][2] = {{0, 255}, {255, 255}, {255, 0}}; //we'll use Green, Yellow and Red color, which neither of them use Blue pin, 
                                                       //therefore, we won't attach it 

int buzzer[3] = {19, 18, 21}; //Flame, Water, Shock

int Flame, Water, Gas, Smoke;
int Shake =1;
Servo myservo;

String serial = "01";

TaskHandle_t FreeTask=NULL;

void _Get(){
  if(WiFi.status()==WL_CONNECTED){
    HTTPClient http;
    http.begin(get_url + serial);

    int hgcode = http.GET();
    if(hgcode==HTTP_CODE_OK){ //success
      String load = http.getString();
      Serial.println(load);
      DeserializationError err = deserializeJson(JSONGet, load);
      if(err){
        Serial.println(hgcode);
        Serial.println("Error deserialization");
      } else {
        Water = (int)JSONGet["water_level"];  
        Gas = (int)JSONGet["gas"];
        Smoke = (int)JSONGet["smoke"];
        Flame = (int)JSONGet["flame"];
        Shake = (int)JSONGet["shake"];
      }
    } else { //fail
      Serial.println(hgcode);
      Serial.println("Fail to GET()");
    }
  }  
}

void Flood(void *parameter){ //Flood LED&Buzzer Control
  while(1){
    //water LED2
    if(Water < 1600){
      for(int i=0;i<2;i++) ledcWrite(2+i, LED_Mode[0][i]); //green
      digitalWrite(buzzer[1], LOW);
    } else if(Water < 1900){
      for(int i=0;i<2;i++) ledcWrite(2+i, LED_Mode[1][i]); //yellow
      digitalWrite(buzzer[1], LOW);
    } else {
      for(int i=0;i<2;i++) ledcWrite(2+i, LED_Mode[2][i]); //red
      digitalWrite(buzzer[1], HIGH);
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

void Shock(void *parameter){ //Earthquake LED&Buzzer Control
  while(1){
    //shake LED3
    if(Shake){
      for(int i=0;i<2;i++) ledcWrite(4+i, LED_Mode[0][i]); //green
      digitalWrite(buzzer[2], LOW);
    } else {
      for(int i=0;i<2;i++) ledcWrite(4+i, LED_Mode[2][i]); //red
      digitalWrite(buzzer[2], HIGH);
    }
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

void setup(){
  Serial.begin(9600);
  for(int i=0;i<3;i++){
    for(int j=0;j<2;j++){
      pinMode(LED_input[i][j], OUTPUT);
      ledcSetup(2*i+j, freq, res);
      ledcAttachPin(LED_input[i][j], 2*i+j);
      ledcWrite(2*i+j, 0);
    }
    pinMode(buzzer[i], OUTPUT);
    digitalWrite(buzzer[i], LOW);
  }

  pinMode(13, OUTPUT);
  myservo.attach(13);
  myservo.write(0);

  pinMode(led_in, OUTPUT);
  digitalWrite(led_in, LOW);

  WiFi_Connect();
  xTaskCreatePinnedToCore(Flood, "Flood", 1024, NULL, 1, &FreeTask, 0);
  xTaskCreatePinnedToCore(Shock, "Shock", 1024, NULL, 1, &FreeTask, 0);
}

void loop(){
  int stateFlame;
  int stateSmoke;
 // myservo.write(0);
  bool gas_check=true;
  _Get();
  
  for(int i=0;i<3;i++) digitalWrite(buzzer[i], LOW); //reset

  if(Gas>1050){ //gas off limit
    for(int i=0;i<2;i++) ledcWrite(i, LED_Mode[2][i]); //red
    digitalWrite(buzzer[0], HIGH); //Warning
    gas_check=false;
    myservo.write(90); //turn val off
    Serial.print("Servo done : ");
    Serial.print(myservo.read());
  }

  if(Flame>2700) stateFlame=0;
  else if(Flame>400) stateFlame=1;
  else stateFlame=2;

  if(Smoke>2000) stateSmoke=2;
  else if(Smoke>1800) stateSmoke=1;
  else stateSmoke=0;
  
  if(!stateFlame&&!stateSmoke){ //0&0
    if(gas_check) for(int i=0;i<2;i++) ledcWrite(i, LED_Mode[0][i]); //green
    digitalWrite(led_in, HIGH); //spinker off
  } else if(stateFlame+stateSmoke < 3){ //middle
    if(gas_check) for(int i=0;i<2;i++) ledcWrite(i, LED_Mode[1][i]); //yellow
    digitalWrite(led_in, HIGH); //spinker off
  } else {
    for(int i=0;i<2;i++) ledcWrite(i, LED_Mode[2][i]); //red
    digitalWrite(buzzer[0], HIGH); //Warning
    digitalWrite(led_in, LOW); //spinker on
  }
  delay(500);
}
