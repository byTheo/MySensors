/**
   For most IoT sensors there's no need to send their data to the controller everytime they change their value. Hence sometimes the
   value changes are being caused because we like to use cheap sensors. The ThesholdUtil.h will help you monitor sensors who's values
   only will be reported whenever their value is above or below a threshold. If however a sensor isn't going over a threshold, the library
   will help you report values on a interval. E.g. if the value of a sensor doesn't change for an hour, a forced transmit interval of 5 minutes
   will cause that sensors to report it's values 12 times.
   
   Created by Theo
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.
 ******************************* */

#include "ThresholdUtil.h"

#include "DHT.h"

DHT dht;


#define DHT 0

#define CHILD_ID_TEMP 0
#define CHILD_ID_HUMIDITY 1
#define CHILD_ID_LIGHT_SENSOR 2
#define DHT22_PIN 14

void setup() {
  Serial.begin( 115200 );

  registerThresholdedSensor( CHILD_ID_TEMP, DHT, TEMPERATURE_SENSOR, 0.5, 30, 20 ); // every 30 seconds report at least every 10 minutes
  registerThresholdedSensor( CHILD_ID_HUMIDITY, DHT, HUMIDTY_SENSOR, 1.0, 60, 10 ); // every 60 seconds

  dht.setup( DHT22_PIN ); // data pin 2

  delay( dht.getMinimumSamplingPeriod() );

  checkThresholdedSensors( checkThrsedSensor, reportSensorValue ); // Necessary call for intializing ThresholdUtil
}


float dhtHumidity; 

void checkThrsedSensor( uint8_t aSensorId, ThreshHoldedSensorType aType, float *value ) {
  if ( aSensorId == DHT  ) {
    if ( aType == TEMPERATURE_SENSOR ) {
      dhtHumidity = dht.getHumidity();
      *value = dht.getTemperature();
    }
    else {
      *value = dhtHumidity;
    }
  }
  if ( aSensorId == LUX_SENSOR && aType == LIGHTLEVEL_SENSOR ) {
    *value = random( 30, 40 );
  }
}

void reportSensorValue( uint8_t child_id, uint8_t sensor_id, ThreshHoldedSensorType sensor_type, float value ) {
  Serial.print( millis() ); Serial.print( ": new sensor value " ); Serial.print( value ); Serial.print( child_id == CHILD_ID_TEMP ? " C" : " %" ); Serial.print( " for child " ); Serial.println( child_id == CHILD_ID_TEMP ? "temp" : ( child_id == CHILD_ID_HUMIDITY ? "hum" : "light" ) );
}

void loop() {
  delay( 5000 );
  checkThresholdedSensors( checkThrsedSensor, reportSensorValue );
}
