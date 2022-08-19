#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <string.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include "SPI.h"
#include <OneWire.h>

//define mcp
#define CS_PIN 15     // ESP32 default SPI pins
#define CLOCK_PIN 14  // Should work with any other GPIO pins, since the library does not formally
#define MOSI_PIN 13   // use SPI, but rather performs pin bit banging to emulate SPI communication.
#define MISO_PIN 12   //
#define MCP3204 4     // (Generally "#define MCP320X X", where X is the last model digit/number of inputs)

//define t.,h.
#define DHTPIN 32
#define DHTTYPE DHT22
DHT dht(DHTPIN,DHTTYPE);

//define PH
#define samplingInterval 20
#define print_Interval 800
#define ArrayLenth  40      //times of collection
#define Offset 0.783051
int pHArray[ArrayLenth];    //Store the average value of the sensor feedback
int pHArrayIndex=0;

//define EC
#define StartConvert 0
#define ReadTemperature 1
#define AnalogSampleInterval 25 //  analog sample interval
#define printInterval 800       //  serial print interval
#define tempSampleInterval 850  //  temperature sample interval

byte DS18B20_Pin = 2;           //  DS18B20 signal, pin on digital 2
byte Index = 0;                           // the index of the current reading
unsigned long AnalogValueTotal = 0;                 // the running total
unsigned int AnalogAverage = 0,averageVoltage=0;    // the average
float temperature,ECcurrent;

float RES2 = 820.0;
float ECRF = 200.0;
float Kvalue = 1.0;
float KvalueLow = 1.1403;
float KvalueHigh = 1.160;

//Temperature chip i/o
OneWire ds(DS18B20_Pin);

float PH_current;
float EC_current;
int Lux_current;
float Temp_current;
float Hum_current;
float Temp_water_current;

// define two tasks 
void TaskReadAllData( void *pvParameters );
void TaskReadSerial( void *pvParameters );

void setup() { // Start the serial so we can see some output

  Serial.begin(9600, SERIAL_8N1, 3, 1); 
  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  
  xTaskCreatePinnedToCore(
    TaskReadAllData
    ,  "ReadAllData"   // A name just for humans
    ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskReadSerial
    ,  "ReadSerial"
    ,  1024  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);
}
void loop() {
}

void TaskReadAllData(void *pvParameters)
{
  (void) pvParameters;
  SPI.begin(CLOCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  TempProcess(StartConvert);   //let the DS18B20 start the convert
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(23,LOW);

  for (;;) // A Task shall never return or exit.
  {
    String stringPrint = String(""); 
    read_dht22();
    read_ph();
    read_EC_config2();

    stringPrint +="{ \"temp\" : \"";
    stringPrint +=Temp_current;
    stringPrint +="\", \"humidity\" : \"";
    stringPrint +=Hum_current;
    stringPrint +="\", \"light\" : \"";
    stringPrint +=Lux_current;
    stringPrint +="\", \"ec\" : \"";
    stringPrint +=EC_current;
    stringPrint +="\", \"ph\" : \"";
    stringPrint +=PH_current;
    stringPrint +="\", \"waterTemp\" : \"";
    stringPrint +=Temp_water_current;
    stringPrint +="\"}";
    Serial.println(stringPrint);
    vTaskDelay(15000);
  }
}

void TaskReadSerial(void *pvParameters) 
{
  (void) pvParameters;

  for (;;) // A Task shall never return or exit.
  {
    if (Serial1.available()) {
      Lux_current = Serial1.parseInt();
//      Serial.println(Lux_current);
    }
  }
}

int read_mcp(int channel){
byte data1;
byte data2;
digitalWrite(CS_PIN,HIGH);
digitalWrite(CS_PIN,LOW);
SPI.transfer(0x06);
switch( channel)
{
  case 0:
  {
    data1 = SPI.transfer(0x00);
    break;
  }
  case 1:
  {
    data1 = SPI.transfer(0x40);
    break;
  }
  case 2:
  {
    data1 = SPI.transfer(0x80);
    break;
  }
  case 3:
  {
    data1 = SPI.transfer(0xc0);
    break;
  }
  default: break;
}
data2 = SPI.transfer(0x00);
//digitalWrite(CS_PIN,HIGH);
uint16_t  analogPH = ((data1<<8)|data2) & 0b0000111111111111;
return analogPH;
}

//PH
void read_ph(){
  int sum =0;
  int avg =0;
  static float pHValue,voltage;
//  Serial.println(read_mcp(0));
  for(int i =0; i<10;i++)
  {
    sum += read_mcp(0);
    delay(25);
  }
  avg = sum/10;
  voltage = avg*5.0/4095.0;
//  Serial.println(voltage);
  pHValue = 3.330339*voltage+Offset;
  PH_current = pHValue;
 }

void temperature_water()
{
  Temp_water_current = TempProcess(1);
  TempProcess(StartConvert);
}

void read_EC_config()
{
  int Analogread = 0;
  float ValueTemp = 0.0;
  float Value = 0.0;
  float rawEC =0.0;
  
  Temp_water_current = TempProcess(1);
  TempProcess(StartConvert);
  
  Analogread = read_mcp(1);
  averageVoltage=Analogread*(float)5000/4095;
//  Serial.println(Analogread);
//  Serial.println(averageVoltage);
  float TempCoefficient=1.0+0.0185*(Temp_water_current-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.0185*(fTP-25.0));
  float CoefficientVolatge=(float)averageVoltage/TempCoefficient;
  if(CoefficientVolatge<=805)EC_current=5.03*CoefficientVolatge+481.5113;   //1ms/cm<EC<=4ms/cm
  else if(CoefficientVolatge<=1410)EC_current=3.15*CoefficientVolatge+1987.92;  //4ms/cm<EC<=7ms/cm
  else EC_current=17.426*CoefficientVolatge-18107.218;                           //7ms/cm<EC<14ms/cm
  EC_current/=1000;
}

void read_EC_config2()
{
  int sum =0;
  int Analogread = 0;
  float ValueTemp = 0.0;
  float Value = 0.0;
  float rawEC =0.0;
  
  Temp_water_current = TempProcess(1);
  TempProcess(StartConvert);

  for(int i =0; i<10;i++)
  {
    sum += read_mcp(1);
    delay(25);
  }
  Analogread = sum/10;
  averageVoltage=Analogread*(float)5000/4095;
//  Serial.println(Analogread);
//  Serial.println(averageVoltage);
  
  rawEC = 1000.0*averageVoltage/RES2/ECRF;
  ValueTemp = rawEC;

  if(ValueTemp > 2.0)
    Kvalue = KvalueHigh;
  else if(ValueTemp <= 2.0)
    Kvalue = KvalueLow;
    
  Value = rawEC*Kvalue;
  Value = Value/(1.0 + 0.0185*(Temp_water_current -25.0));
//  Serial.println(rawEC);
//  Serial.println(Kvalue);
//  Serial.println(Temp_water_current);
  
  EC_current = Value;
}

float TempProcess(bool ch)
{
  //returns the temperature from one DS18B20 in DEG Celsius
  static byte data[12];
  static byte addr[8];
  static float TemperatureSum;
  if(!ch){
          if ( !ds.search(addr)) {
//              Serial.println("no more sensors on chain, reset search!");
              ds.reset_search();
              return 0;
          }
          if ( OneWire::crc8( addr, 7) != addr[7]) {
              Serial.println("CRC is not valid!");
              return 0;
          }
          if ( addr[0] != 0x10 && addr[0] != 0x28) {
              Serial.print("Device is not recognized!");
              return 0;
          }
          ds.reset();
          ds.select(addr);
          ds.write(0x44,1); // start conversion, with parasite power on at the end
  }
  else{
          byte present = ds.reset();
          ds.select(addr);
          ds.write(0xBE); // Read Scratchpad
          for (int i = 0; i < 9; i++) { // we need 9 bytes
            data[i] = ds.read();
          }
          ds.reset_search();
          byte MSB = data[1];
          byte LSB = data[0];
          float tempRead = ((MSB << 8) | LSB); //using two's compliment
          TemperatureSum = tempRead / 16;
    }
          return TemperatureSum;
}

void read_dht22(){
  dht.begin();
  delay(100);
  float temp=dht.readTemperature();
  float hum=dht.readHumidity();
  Temp_current = temp;
  Hum_current = hum;
}
