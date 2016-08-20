/**
   Room conditions node. Measure temperature, humidity, lightlevel and pressure.

   Copyright (c) ByTheo

   Description:
   This Node demonstrates the useage of the ThresholdUtil library, by combining a temperature, humidity, lightlevel and barometric sensors
   It also utilizes the Forecast algorithm from the MySensors build page, which for this purpose has been put in a separate library

   Developped on a ProMini 3.3V. Sensors used:
   - BMP180 Pressure sensor (can also read temperature but not used by this Node)
   - SI7021 Humidity and temperature sensor
   - Photo Diode LM393 sensor (bought an Aliexpress

   History
     August 20th 2016 - Initial version

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

 *******************************
*/

#include <SPI.h>
#include <MySensor.h>#include <Wire.h>
#include <SI7021.h>
#include <Adafruit_BMP085.h>
#include "ThresholdUtil.h"
#include "WeatherForecast.h"

// Uncomment when debugging
// #define RC_DEBUG

// Define Arduino Pins used by sensors A4 and A5 are also used for i2c bus
#define LIGHT_SENSOR_ANALOG_PIN A0
#define LIGHT_SENSOR_DIGITAL_PIN 3

// The altitude of the location where the sensor is being placed. Adjust to your location
#define ALTITUDE 33.0

// Define constants for children used by Node
#define TEMPEARTURE_SENSOR_CHILD_ID  0
#define HUMIDITY_SENSOR_CHILD_ID     1
#define LIGHTLEVEL_SENSOR_CHILD_ID   2
#define PRESSURE_SENSOR_CHILD_ID     3

// Declare Sensor ID's used for ThresholdUtil library
#define TEMPEARTURE_SENSOR_ID 1
#define HUMIDITY_SENSOR_ID    2
#define LIGHTLEVEL_SENSOR_ID  3
#define PRESSURE_SENSOR_ID    4

// Declare Sketch Name and Version
#define MYS_SKETCH_NAME "Improved room conditions"
#define MYS_SKETCH_VERSION "1.0"

// Variabe used for remembering lastForeCast send to the gateway
int lastForecast = -1;

// Create objecs for i2c sensors
Adafruit_BMP085 bmp = Adafruit_BMP085();
SI7021 sensor;

// Declare MySensors class and messages
MySensor gw;
MyMessage tempMsg( TEMPEARTURE_SENSOR_CHILD_ID, V_TEMP );
MyMessage pressureMsg( PRESSURE_SENSOR_CHILD_ID, V_PRESSURE);
MyMessage forecastMsg( PRESSURE_SENSOR_CHILD_ID, V_FORECAST);
MyMessage msgHum(HUMIDITY_SENSOR_CHILD_ID, V_HUM);
MyMessage lightLevelMsg(LIGHTLEVEL_SENSOR_CHILD_ID, V_LIGHT_LEVEL);

/**
 * Setup method. This will initialize the i2c sensors, register Thresholds. Initializes MySensors and presents the children of the node. Finally the initial values are being send
 * to the gateway
 */
void setup() {
#ifdef RC_DEBUG
  Serial.begin( 115200 );
#endif
  // initialize I2C sensors
  sensor.begin();
  bmp.begin();

  // register sensors and their thresholds
  registerThresholdedSensor( TEMPEARTURE_SENSOR_CHILD_ID, TEMPEARTURE_SENSOR_ID, TEMPERATURE_SENSOR, 0.5,  5, 60  ); // read every 5 sec and report at least every 5 minute
  registerThresholdedSensor( HUMIDITY_SENSOR_CHILD_ID,    HUMIDITY_SENSOR_ID,    HUMIDTY_SENSOR,     1.0, 10, 30  ); // read every 5 sec and report at least every 5 minute
  registerThresholdedSensor( LIGHTLEVEL_SENSOR_CHILD_ID,  LIGHTLEVEL_SENSOR_ID,  LIGHTLEVEL_SENSOR,  2.0,  1, 300 ); // read every 5 sec and report at least every 5 minute
  registerThresholdedSensor( PRESSURE_SENSOR_CHILD_ID,    PRESSURE_SENSOR_ID,    PRESSURE_SENSOR,    0.5, 60, 10  ); // every minute. report at least every 10. Forecast algorithm wants new value every minute

  // Register the node to the gateway
  gw.begin();

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo( MYS_SKETCH_NAME, MYS_SKETCH_VERSION );
  gw.wait( 50 ); // give radio the time to register the child

  gw.sendBatteryLevel( 100, true );
  gw.wait( 50 ); // give radio the time to register the child

  // Register sensors to gw (they will be created as child devices)
  gw.present( PRESSURE_SENSOR_CHILD_ID, S_BARO, "Barometer", true );
  gw.wait( 50 ); // give radio the time to register the child
  gw.present( TEMPEARTURE_SENSOR_CHILD_ID, S_TEMP, "Temperature", true );
  gw.wait( 50 ); // give radio the time to register the child
  gw.present( HUMIDITY_SENSOR_CHILD_ID, S_HUM, "Humidity", true );
  gw.wait( 50 ); // give radio the time to register the child
  gw.present( LIGHTLEVEL_SENSOR_CHILD_ID, S_LIGHT_LEVEL, "Light level", true );
  gw.wait( 50 ); // give radio the time to register the child

  checkThresholdedSensors( readSensorValue, reportSensorValue ); // Send initial values to the gateway
}

/*
 * Call back method for ThresholdUtil library, will ask for sensor values when the library needs them
 * 
 * ps. don't forget to put a * in front of the value inside the method. The library will not receive any
 * values if you forget the *
 */
void readSensorValue( uint8_t aSensorId, ThreshHoldedSensorType aType, float *value ) {
  switch ( aSensorId ) {
    case TEMPEARTURE_SENSOR_ID:
      if ( aType == TEMPERATURE_SENSOR ) {
        *value = sensor.getCelsiusHundredths() / 100.0;
      }
      break;
    case HUMIDITY_SENSOR_ID:
      if ( aType == HUMIDTY_SENSOR ) {
        *value = sensor.getHumidityPercent() * 1.0;
      }
      break;
    case LIGHTLEVEL_SENSOR_ID:
      if ( aType == LIGHTLEVEL_SENSOR ) {
        *value = (float)( 1023 - analogRead( LIGHT_SENSOR_ANALOG_PIN ) ) / 10.23;
      }
      break;
    case PRESSURE_SENSOR_ID:
      if ( aType == PRESSURE_SENSOR ) {
        float pressure = bmp.readSealevelPressure( ALTITUDE ) / 100.0;
        *value = pressure;
        int forecast = sample( pressure );
        if (forecast != lastForecast) {
          gw.send( forecastMsg.set( weather[ forecast ] ), true );
          gw.wait( 50 );
          lastForecast = forecast;
#ifdef RC_DEBUG
          Serial.print( "New forecast " ); Serial.println( forecast );
#endif
        }
      }
      break;
  }
}

/**
 * Call back function being called by the thresholdUtil library whenever a threshold or forced transmit has been detected
 */
void reportSensorValue( uint8_t child_id, uint8_t sensor_id, ThreshHoldedSensorType sensor_type, float value ) {
  switch ( child_id ) {
    case TEMPEARTURE_SENSOR_CHILD_ID:
#ifdef RC_DEBUG
      Serial.print( "Temperature " ); Serial.print( value, 1 ); Serial.println( "C" );
#endif
      gw.send(tempMsg.set(value, 1), true ); // Send temperature with 1 decimal precision
      break;
    case HUMIDITY_SENSOR_CHILD_ID:
#ifdef RC_DEBUG
      Serial.print( "Humidity " ); Serial.print( value, 0 ); Serial.println( "%" );
#endif
      gw.send(msgHum.set( value, 1 ), true ); // Send temperature with 1 decimal precision
      break;
    case LIGHTLEVEL_SENSOR_CHILD_ID:
#ifdef RC_DEBUG
      Serial.print( "Light level " ); Serial.print( value, 0 ); Serial.println( "%" );
#endif
      gw.send( lightLevelMsg.set( value, 0 ), true );
      break;
    case PRESSURE_SENSOR_CHILD_ID:
#ifdef RC_DEBUG
      Serial.print( "Pressure " ); Serial.print( value, 1 ); Serial.println( " hPa" );
#endif
      gw.send( pressureMsg.set( value, 0 ), true );
      break;
  }
  gw.wait( 50 );
}

/**
 * loop method. Basically we only call the checkTresholdedSensor method of the ThresholdUtil library
 */
void loop() {
  checkThresholdedSensors( readSensorValue, reportSensorValue );
  gw.wait( 50 );
}
