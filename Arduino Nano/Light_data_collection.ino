#include <SoftwareSerial.h>
#include <string.h>
#include <Wire.h>
#include <BH1750FVI.h>

#define SSerialTxControl 2   //Kiểm soát hướng cho rs485
//SoftwareSerial RS485Serial(17, 16);

#define ADDRESSPIN 13
BH1750FVI::eDeviceAddress_t DEVICEADDRESS = BH1750FVI::k_DevAddress_L;
BH1750FVI::eDeviceMode_t DEVICEMODE = BH1750FVI::k_DevModeContHighRes;

// Create the Lightsensor instance
BH1750FVI LightSensor(ADDRESSPIN, DEVICEADDRESS, DEVICEMODE);


void setup()
{
  Serial.begin(9600);
  while(!Serial){
    ;
  }
  LightSensor.begin();
  pinMode(SSerialTxControl, OUTPUT);  
  digitalWrite(SSerialTxControl, HIGH);  // Trans mode
}

void loop()
{
    uint16_t lux = LightSensor.GetLightIntensity();
    Serial.println(lux); //Write the serial data
    delay(500);
}
