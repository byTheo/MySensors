/**
 * Library ThresholdUtil.h 
 * 
 * Copyright (c) ByTheo
 * 
 * Description:
 *   This library handles all interval based reading of simpels who's values you want to send on a periodical basis. This means that at least every given interval number of readings
 *   the callback handler for that sensor is being called. If howver the value of the sensors goes past the defined threshold value of that sensor, it will call the callbackhandler
 *   the soon as it detects the threshold. This library was based upon the Sensbender Micro example Sketch.
 * 
 * Useage:
 *   1. register each individual sensor to the library with the registerThresholdedSensor() function in your Sketchs setup() function. See comments in code.
 *   2. implement a SensorValueRequestedCallback() function in your Sketch
 *   3. Implement a SensorValueTransmission() function in your sketch 
 *   4. Call checkThresholdedSensors from within your loop as much as you can.
 * 
 * History
 *   August 1st 2016 - Initial version
 * 
 * Design decisions:
 * - don't put sensor specific code into this library (separation of concern). The main sketch is responsible for the retrieval of sensor values.
 * - Don't use C++ is absorps too much resources. PLain old C is more resource efficient
 * - Don't implement memory management. Freeing up is unnecessary because we don't need to support it. Sensors shouldn't be dynamic in a Sketch
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
*/

#ifndef THRESHHOLD_UTIL_H
#define THRESHHOLD_UTIL_H

/**
 * Declartion of the type definition used by the library
 */
typedef enum { TEMPERATURE_SENSOR, HUMIDTY_SENSOR, LIGHTLEVEL_SENSOR, CUSTOM_SENSOR } ThreshHoldedSensorType; // The different kind of sensor type for which you can store values. You can extend this enum to your likings or do a pus request on GITHUB
typedef void (*SensorValueRequestedCallback)( uint8_t aSensorId, ThreshHoldedSensorType aType, float *value ); // Callback handler which is called whenever the library needs a new value for a specific sensor
typedef void (*SensorValueTransmission)( uint8_t child_id, uint8_t sensor_id, ThreshHoldedSensorType sensor_type, float value ); // Callback handler called whenever the library notices that the value of a specific sensor has to be send to the gateway

/**
 * Structure used by the library for administrating the variabels for a specific sensor. The library uses a linked list of structures.
 */
typedef struct ThresholdedSensorStruct {
  uint8_t child_id;
  uint8_t sensor_id; // unique id for the sensors for which we hold the values
  ThreshHoldedSensorType sensor_type;
  float      threshHold;
  uint8_t    forcedTransmissionInterval;
  uint8_t    measureCounter;
  float      lastValue;
  unsigned long lastChecked;
  uint8_t    readingInterval;
  ThresholdedSensorStruct *nextNode;
} *thssPtr;

/**
 * The root node of the linked list of ThresholdedSensorStructs
 */
thssPtr thresHoldedSensorsrootNode = NULL;

/**
 * Mehthod for adding a sensor who's value needs to be monitored.
 * child_id: The id of child for which the sensor is monitored. It's only for the convenience of the main sketch and isn't used by the library 
 * sensor_id: An id of a sensor. You can use this to group different sensor values if they come from the same sensor. Some sensors like the HDT11 or the BMP180 report multiple sensor types
 *            The main sketch should use this to request the values of such kind of sensors just once and not multiple times for each request for value made by the library
 * sensor_type: Indicates what kind of values are being monitored. Use this to determine what kind of value is required when dealing with multi purpose sensors
 * theshHold: The value of threshold is indicates how much the new value needs to differ from the last send value before it triggers a resend.
 * readingInterval: The amount of seconds the library waits before it request a new value for this sensor
 * forcedTransmissionInterval: The amount of measurements without retransmissions, before a retransmission is triggered. In other words of the values of a sensor don't extend the threshold, a retransmission
 *            is triggered of this given amount of readings.
 */
void registerThresholdedSensor(   uint8_t child_id, uint8_t sensor_id, ThreshHoldedSensorType sensor_type, float threshHold, uint8_t readingInterval, uint8_t forcedTransmissionInterval ) { // single sensor
  thssPtr aSensor = (ThresholdedSensorStruct*)malloc(sizeof(struct ThresholdedSensorStruct));
  aSensor->sensor_id = sensor_id;
  aSensor->child_id = child_id;
  aSensor->sensor_type = sensor_type;
  aSensor->threshHold = threshHold;
  aSensor->forcedTransmissionInterval = forcedTransmissionInterval;
  aSensor->measureCounter = forcedTransmissionInterval - 1; // correct initialzation
  aSensor->lastValue = 0;
  aSensor->readingInterval = readingInterval;
  aSensor->lastChecked = millis(); // not checked
  aSensor->nextNode = NULL;
  if ( thresHoldedSensorsrootNode == NULL ) {
    thresHoldedSensorsrootNode = aSensor;
  }
  else {
    thssPtr node = thresHoldedSensorsrootNode;
    while ( node->nextNode != NULL ) {
      node = node->nextNode;
    }
    node->nextNode = aSensor;
  }
}

/**
 * Supportive method for the checkThresholdedSensors routine. It checks if the value of the given sensors needs to be read according to the given timeStamp.
 * The given SensorValueRequestedCallback is used to query the values of a sensor. The given SensorValueTransmission is called when a (re)transmisson for the value
 * of the given Sensor is required.
 * This method contains the actual logic of the library.
 */
void checkIndividualThreshHoldedSensor( thssPtr aSensor, unsigned long timeStamp, SensorValueRequestedCallback aCallBack, SensorValueTransmission transmissionCallback ) {
  // determine whether node should be checked in first place!!!
  if ( aSensor->lastChecked <= timeStamp ) {
    aSensor->measureCounter++;
    float value;
    aCallBack( aSensor->sensor_id, aSensor->sensor_type, &value );
    aSensor->lastChecked = timeStamp + ( (unsigned long)aSensor->readingInterval * 1000 );

    float diffMeasurement = abs( aSensor->lastValue - value );

    bool sendValues = false;

    if ( diffMeasurement >= aSensor->threshHold || aSensor->measureCounter == aSensor->forcedTransmissionInterval ) { // transmit value
      aSensor->lastValue = value;
      aSensor->measureCounter = 0;
      transmissionCallback(  aSensor->child_id,  aSensor->sensor_id, aSensor->sensor_type, aSensor->lastValue );
    }
  }
}

/**
 * Checks all defined sensors on: if they need to be checked and if their value needs to be send to the gateway:
 * aCallBack: pointer to a function that can determine the sensors current value (logic should be implemented in the main sketch, separation of concern)
 * transmissionCallback: pointer to the function the deals with sending a value of a sensor to the gateway (logic should be implemnted by the main sketch, separation of concern)
 */
void checkThresholdedSensors( SensorValueRequestedCallback aCallBack, SensorValueTransmission transmissionCallback ) {
  unsigned long timeStamp = millis();
  if ( thresHoldedSensorsrootNode != NULL ) {
    thssPtr node = thresHoldedSensorsrootNode;
    checkIndividualThreshHoldedSensor( node, timeStamp, aCallBack, transmissionCallback );
    while ( node->nextNode != NULL ) {
      node = node->nextNode;
      checkIndividualThreshHoldedSensor( node, timeStamp, aCallBack, transmissionCallback );
    }
  }
}

#endif
