/*
 * This program is an attempt to make a device-agnostic WiFi test tool.
 */
#include "WiFi.h"			  // This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#include <base64.hpp>	  // The Base64 encode/decode library. Author: Kazuki Ota (dojyorin) https://github.com/dojyorin/arduino_base64
#include "privateInfo.h"  // This file stores sensitive information.
#include <PubSubClient.h> // PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient


//const char* wifiSsid = "<change or use privateInfo.h>";
//const char* wifiPassword = "<change or use privateInfo.h>";
//char* const wifiSsidList[3] = { "<change or use privateInfo.h>", "<change or use privateInfo.h>", "<change or use privateInfo.h>" };
//char* const wifiSsidPassArray[3] = { "<change or use privateInfo.h>", "<change or use privateInfo.h>", "<change or use privateInfo.h>" };
//char* const mqttBrokerArray[3] = { "<change or use privateInfo.h>", "<change or use privateInfo.h>", "<change or use privateInfo.h>" };
//int const mqttPortArray[3] = { 1883, 1883, 1883 };


const char* mqttStatsTopic = "espStats";
const int buttonGPIO = 0;
char ipAddress[16];
char macAddress[18];
const int LED_PIN = 2;							// This LED for the Lolin devkit is on the ESP8266 module itself (next to the antenna).
String sketchName = "WiFiScratchPad.ino";	// A string to hold this sketch name.
String ipString = "127.0.0.1";				// A String to hold the IP address.
String macString = "00:00:00:00:00:00";	// A String to hold the MAC address.
char ipCharArray[16];							// A character array to hold the IP address.
char macCharArray[18];							// A character array to hold the MAC address.
byte macByteArray[6];							// A byte array to hold the MAC address.
unsigned long loopCount = 0;					// An unsigned long can hold 8,171 years worth of one-minute loops.  A word can hold only 45.5 days worth.
unsigned long publishDelay = 20000;			// An unsigned long can hold values from 0-4,294,967,295.  In milliseconds, this sets a limit at 49.7 days of time.
unsigned long mqttReconnectDelay = 5000;	// An unsigned long can hold values from 0-4,294,967,295.  In milliseconds, this sets a limit at 49.7 days of time.
unsigned long lastPublish = 0;				// This is used to determine the time since last MQTT publish.
unsigned long lastLoop = 0;					// An unsigned long can hold values from 0-4,294,967,295.  In milliseconds, this sets a limit at 49.7 days of time.
size_t networkIndex = 99;						// An unsigned integer to hold the correct index for the network arrays: wifiSsidList[], wifiSsidPassArray[], mqttBrokerArray[], and mqttPortArray[].


WiFiClient espClient;
PubSubClient mqttClient( espClient );


// The IRAM_ATTR causes the function to operate from RAM, which is faster.
void IRAM_ATTR takePicture()
{
	Serial.println( "\n\n\nThe takePicture() function is only stub!" );
	Serial.println( "Please finish it and try again!\n\n\n" );
}


void setup()
{
	Serial.begin( 115200 );
	if( !Serial )
		delay( 1000 );
//	Serial.setDebugOutput( true );
	Serial.println();
	Serial.println( "Running setup() in " + sketchName );

	// Get the MAC address.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );
	Serial.print( "MAC address: " );
	Serial.println( macAddress );
	snprintf( macCharArray, 18, "%s", WiFi.macAddress().c_str() );		// Read the MAC address into a character array.
	macString = WiFi.macAddress().c_str();										// Read the MAC address into a String.
	WiFi.macAddress( macByteArray );

	Serial.println( "" );
	Serial.print( "MAC char array: " );
	Serial.println( macCharArray );
	Serial.print( "MAC byte array: " );
	Serial.printf( "%02X:%02X:%02X:%02X:%02X:%02X\n", macByteArray[0], macByteArray[1], macByteArray[2], macByteArray[3], macByteArray[4], macByteArray[5] );
	Serial.println( "MAC String: " + macString );

	// Set the ipCharArray char array to a default value.
	snprintf( ipCharArray, 16, "127.0.0.1" );

	Serial.print( "Auto-reconnect = " );
	Serial.println( WiFi.getAutoReconnect() );

	// Try to connect to the configured WiFi network, up to 20 times.
	wifiMultiConnect();

	// The networkIndex variable is initialized to 99.  If it is still 99 at this point, then WiFi failed to connect.
	if( networkIndex != 99 )
	{
		const char* mqttBroker = mqttBrokerArray[networkIndex];
		const int mqttPort = mqttPortArray[networkIndex];
		// Set the MQTT client parameters.
		mqttClient.setServer( mqttBroker, mqttPort );
	}

	pinMode( buttonGPIO, INPUT );
	attachInterrupt( digitalPinToInterrupt( buttonGPIO ), takePicture, RISING );

	Serial.println( "Completed setup()\n" );
} // End of setup() function.


void	 wifiMultiConnect()
{
	Serial.println( "\nEntering wifiMultiConnect()" );
	for( size_t i = 0; i < 3; i++ )
	{
		// Get the details for this connection attempt.
		const char* wifiSsid = wifiSsidList[i];
		const char* wifiPassword = wifiSsidPassArray[i];

		// Announce the WiFi parameters for this connection attempt.
		Serial.print( "WiFi connecting to SSID \"" );
		Serial.print( wifiSsid );
		Serial.println( "\"" );

		// Attempt to connect to this WiFi network.
		Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : "Failed!" );
		WiFi.begin( wifiSsid, wifiPassword );

		/*
			WiFi.status() return values:
			0 : WL_IDLE_STATUS when WiFi is in process of changing between statuses
			1 : WL_NO_SSID_AVAIL in case configured SSID cannot be reached
			3 : WL_CONNECTED after successful connection is established
			4 : WL_CONNECT_FAILED if wifiPassword is incorrect
			6 : WL_DISCONNECTED if module is not configured in station mode
		*/
		unsigned long wifiConnectionTimeout = 10000;
		unsigned long wifiConnectionStartTime = millis();
		// Wait up to 10 seconds for a connection.
		while( WiFi.status() != WL_CONNECTED || millis() - wifiConnectionStartTime < wifiConnectionTimeout )
		{
			Serial.print( "." );
			delay( 1000 );
		}

		if( WiFi.status() == WL_CONNECTED )
		{
			networkIndex = i;
			// Print that WiFi has connected.
			Serial.println( "\nWiFi connection established!" );
			snprintf( ipCharArray, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] ); // Read the IP address into a character array.
			ipString = WiFi.localIP().toString();																												// Read the IP address into a String.
			Serial.print( "IP address character array: " );
			Serial.println( ipCharArray );
			Serial.println( "IP address String: " + ipString );
			return;
		}
	}

	Serial.println( "Exiting wifiMultiConnect()\n" );
} // End of wifiMultiConnect() function.


// mqttConnect() will attempt to (re)connect the MQTT client.
void mqttConnect( int maxAttempts )
{
	digitalWrite( LED_PIN, LOW ); // Turn the LED off.
	Serial.print( "Attempting to connect to the MQTT broker up to " );
	Serial.print( maxAttempts );
	Serial.println( " times." );

	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempt # " );
		Serial.print( i + 1 );
		Serial.print( "..." );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		if( mqttClient.connect( macAddress ) )
		{
			Serial.println( " connected!" );
			digitalWrite( LED_PIN, HIGH ); // Turn the LED on.
		}
		else
		{
			Serial.print( " failed!  Return code: " );
			Serial.print( mqttClient.state() );
			Serial.print( ".  Trying again in " );
			Serial.print( mqttReconnectDelay / 1000 );
			Serial.println( " seconds." );
			digitalWrite( LED_PIN, HIGH ); // Turn the LED on.
			delay( mqttReconnectDelay / 2 );
			digitalWrite( LED_PIN, LOW ); // Turn the LED off.
			delay( mqttReconnectDelay / 2 );
		}
		i++;
	}
	mqttClient.setBufferSize( 512 );
	char mqttString[512];
	snprintf( mqttString, 512, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"rssi\": \"%s\"\n}", sketchName, macAddress, ipAddress, WiFi.RSSI() );
	if( mqttClient.publish( mqttStatsTopic, mqttString ) )
	{
		Serial.print( "Published this data to '" );
		Serial.print( mqttStatsTopic );
		Serial.print( "':\n" );
		Serial.print( mqttString );
	}
	else
	{
		Serial.print( "\n\nPublish to '" );
		Serial.print( mqttStatsTopic );
		Serial.println( "' failed!\n\n" );
	}

	Serial.println( "Function mqttConnect() has completed." );
} // End of mqttConnect() function.


void loop()
{
	unsigned long time = millis();
	// When time is less than publishDelay, subtracting publishDelay from time causes an overlow which results in a very large number.
	if( lastPublish == 0 || ( ( time > publishDelay ) && ( time - publishDelay ) > lastPublish ) )
	{
		loopCount++;
		Serial.println( sketchName + " loop # " + loopCount );

		if( WiFi.status() != WL_CONNECTED )
			wifiMultiConnect();

		Serial.println();
		Serial.println( sketchName );
		Serial.println( "MAC String: " + macString );
		Serial.println( "IP String: " + ipString );
		Serial.print( "rssi: " );
		Serial.println( WiFi.RSSI() );

		Serial.println( "Delaying " );
		Serial.print( publishDelay / 1000 );
		Serial.println( " seconds.\n" );
		lastPublish = millis();
	}
} // End of loop() function.
