/********* Thingspeak *******/
String statusChWriteKey = "3IQC1SEW7337DY9E";  // Status Channel id: 592388
#include <SoftwareSerial.h>
SoftwareSerial EspSerial(6, 7); // TX ESP = 6 (RX UNO) , RX ESP = 7 (TX UNO)
#define HARDWARE_RESET 8


/********* DS18B20 (Sensor Suhu) *******/
#include <DallasTemperature.h>
#include <OneWire.h>
#define ONE_WIRE_BUS 2  // sensor diletakkan di pin 2
OneWire oneWire(ONE_WIRE_BUS);  // setup sensor
DallasTemperature sensorSuhu(&oneWire);  // berikan nama variabel,masukkan ke pustaka Dallas
float suhuSekarang;


/********* Sensor pH *******/
const int analogInPin = A0;  //sensor diletakkan di pin A0 
int sensorValue = 0; 
unsigned long int avgValue; 
float b;
float pHSekarang;
int buf[10],temp;


// Note that we must not write on ThingSpeak channel on intervals lower than 16 seconds
long writeTimingSeconds = 17; // ==> Define Sample time in seconds to send data
long startWriteTiming = 0;
long elapsedWriteTime = 0;


int spare = 0;
boolean error;


void setup() {
  Serial.begin(9600);
  pinMode(HARDWARE_RESET,OUTPUT);
  digitalWrite(HARDWARE_RESET, HIGH);
  sensorSuhu.begin();
  EspSerial.begin(9600); // ESP telah di set ke baud 9600
  EspHardwareReset(); //Reset setiap akan dipakai
  startWriteTiming = millis(); // starting the "program clock"
}


void loop() {
  start: //label 
  error=0;
  
  elapsedWriteTime = millis()-startWriteTiming; 
  
  if (elapsedWriteTime > (writeTimingSeconds*1000)) 
  {
    readSensors();
    writeThingSpeak();
    startWriteTiming = millis();   
  }
  
  if (error==1) //Resend if transmission is not completed, bisa aja kalau jaringan putus
  {       
    Serial.println(" <<<< ERROR >>>>");
    delay (2000);  
    goto start; //go to label "start"
  }
}


/********* Penghitungan Sensor Suhu *************/
float ambilSuhu ()
{
   sensorSuhu.requestTemperatures();
   float suhu = sensorSuhu.getTempCByIndex(0);
   return suhu;  
}


/********* Penghitungan Sensor pH *************/
float ambilpH ()
{
    for(int i=0;i<10;i++) 
    { 
      buf[i]=analogRead(analogInPin);
      delay(10);
    }
    for(int i=0;i<9;i++)
    {
      for(int j=i+1;j<10;j++)
      {
        if(buf[i]>buf[j])
        {
          temp=buf[i];
          buf[i]=buf[j];
          buf[j]=temp;
        }
      }
    }
 avgValue=0;
 for(int i=2;i<8;i++)
 avgValue+=buf[i];
 float pHVol=(float)avgValue*5.0/1024/6; //1024 karna arduino 10 bit 
 float phValue = -5.70 * pHVol + 21.34;
 return phValue;
}


/********* Read dan Display Sensors Value *************/
void readSensors(void)
{
  suhuSekarang = ambilSuhu();
  pHSekarang = ambilpH();
  Serial.println(suhuSekarang);
  Serial.println(pHSekarang);
  delay(2000);
}


/********* Koneksi ke Thingspeak *******/
void writeThingSpeak(void)
{

  startThingSpeakCmd();

  // preparacao da string GET
  String getStr = "GET /update?api_key=";
  getStr += statusChWriteKey;
  getStr +="&field1=";
  getStr += String(suhuSekarang);
  getStr +="&field2=";
  getStr += String(pHSekarang);
  getStr += "\r\n\r\n";

  sendThingSpeakGetCmd(getStr); 
}


/********* Reset ESP *************/
void EspHardwareReset(void)
{
  Serial.println("Reseting......."); 
  digitalWrite(HARDWARE_RESET, LOW); 
  delay(500);
  digitalWrite(HARDWARE_RESET, HIGH);
  delay(8000);//Tempo necessário para começar a ler 
  Serial.println("RESET"); 
}


/********* Start communication with ThingSpeak*************/
void startThingSpeakCmd(void)
{
  EspSerial.flush();//limpa o buffer antes de começar a gravar
  
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += "184.106.153.149"; // IP dari thingspeak.com
  cmd += "\",80";
  EspSerial.println(cmd);
  Serial.print("enviado ==> Start cmd: ");
  Serial.println(cmd);

  if(EspSerial.find("Error"))
  {
    Serial.println("AT+CIPSTART error");
    return;
  }
}


/********* send a GET cmd to ThingSpeak *************/
String sendThingSpeakGetCmd(String getStr)
{
  String cmd = "AT+CIPSEND=";
  cmd += String(getStr.length());
  EspSerial.println(cmd);
  Serial.print("enviado ==> lenght cmd: ");
  Serial.println(cmd);

  if(EspSerial.find((char *)">"))
  {
    EspSerial.print(getStr);
    Serial.print("enviado ==> getStr: ");
    Serial.println(getStr);
    delay(500);//tempo para processar o GET, sem este delay apresenta busy no próximo comando

    String messageBody = "";
    while (EspSerial.available()) 
    {
      String line = EspSerial.readStringUntil('\n');
      if (line.length() == 1) 
      { //actual content starts after empty line (that has length 1)
        messageBody = EspSerial.readStringUntil('\n');
      }
    }
    Serial.print("MessageBody received: ");
    Serial.println(messageBody);
    return messageBody;
  }
  else
  {
    EspSerial.println("AT+CIPCLOSE");     // alert user
    Serial.println("ESP8266 CIPSEND ERROR: RESENDING"); //Resend...
    spare = spare + 1;
    error=1;
    return "error";
  } 
}
