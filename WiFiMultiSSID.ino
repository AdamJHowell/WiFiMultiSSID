/*
 * This program enables the microcontroller to connect to multiple WiFi networks, and then to an associated MQTT broker.
 * This is useful when you need to take the device to multiple locations.
 * This serves the same functionality as the Arduino_MultiWiFi library (https://github.com/arduino-libraries/Arduino_MultiWiFi),
 * but incorporates MQTT broker credentials and settings.
 */
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h> // PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient


/* 
 * These ficticious credentials attempt to connect to my home network first, then a neighbor's WiFi, then finally a work WiFi.
 * In this scenario, when connected at home, it would use my home MQTT broker.
 * When connected to the neighbor's WiFi, it would use a public broker.
 * When connected to my work WiFi, it will use a broker on that network.
 */
const char* wifiSsidList[3] = { "MyHomeWifi", "NeighborWifi", "MyWorkWifi" };
const char* wifiPassArray[3] = { "password1", "password2", "password3" };
const char* mqttBrokerArray[3] = { "192.168.42.2", "publicBroker.com", "10.0.0.128" };
int const mqttPortArray[3] = { 1337, 1883, 1883 };
const char* mqttBrokerUserArray[3] = { "homeUser", "publicBrokerUser", "a.howell" };
const char* mqttBrokerPassArray[3] = { "homePass", "publicBrokerPass", "ultraMegaSecret" };

const char* mqttStatsTopic = "espStats";
const char* mqttCommandTopic = "espCommands";
const int BUFFER_SIZE = 65535;						// The maximum packet size MQTT should transfer.
const int FAILURE_BLINK_RATE = 300;					// The blink delay used when buffer allocation fail.
char ipAddress[16];										// Holds the IP address.
char macAddress[18];										// Holds the MAC address.
const int LED_PIN = 2;									// This LED for the Lolin devkit is on the ESP8266 module itself (next to the antenna).
String sketchName = "WiFiMultiSSID";				// A string to hold this sketch name.
unsigned long loopCount = 0;							// An unsigned long can hold 8,171 years worth of one-minute loops.  A word can hold only 45.5 days worth.
unsigned long publishDelay = 20000;					// How long to wait between publishes.
unsigned long wifiConnectionTimeout = 10000;		// The maximum amount of time to wait for a WiFi connection before trying a different SSID.
unsigned long mqttReconnectDelay = 5000;			// How long to wait between MQTT reconnect attempts.
unsigned long lastPublish = 0;						// The time of the last MQTT publish.
unsigned long lastLoop = 0;							// The time of the last loop.
size_t networkIndex = 2112;							// Used to hold the correct index for the network arrays: wifiSsidList[], wifiPassArray[], mqttBrokerArray[], and mqttPortArray[].


// Class objects
WiFiClient espClient;
PubSubClient mqttClient( espClient );


void setup()
{
	Serial.begin( 115200 );
	if( !Serial )
		delay( 1000 );
	Serial.println();
	Serial.println( "Running setup() in " + sketchName );

	// Get the MAC address.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );
	Serial.print( "MAC address: " );
	Serial.println( macAddress );

	// Try to connect to the configured WiFi networks.
	wifiMultiConnect();

	// The networkIndex variable is initialized to 2112.  If it is still 2112 at this point, then WiFi failed to connect.
	if( networkIndex != 2112 )
	{
		const char* mqttBroker = mqttBrokerArray[networkIndex];
		const int mqttPort = mqttPortArray[networkIndex];
		// Set the MQTT client parameters.
		mqttClient.setServer( mqttBroker, mqttPort );
		Serial.print( "MQTT broker: " );
		Serial.println( mqttBroker );
		Serial.print( "MQTT port: " );
		Serial.println( mqttPort );
	}

	Serial.println( "Completed setup()\n" );
} // End of setup() function.


/* 
 * wifiMultiConnect() will attempt to connect to multiple SSIDs.
 * It expects the first SSID in wifiSsidList will match the first password in wifiSsidPassArray, the second to second, etc.
 * When a connection is made, the index in use will be saved to 'networkIndex'.
 * The mqttConnect() function will use networkIndex to know which broker and port to connect to.
 */
void wifiMultiConnect()
{
	digitalWrite( LED_PIN, LOW ); // Turn the LED off.

	Serial.println( "\nEntering wifiMultiConnect()" );
	for( size_t networkArrayIndex = 0; networkArrayIndex < sizeof( wifiSsidList ); networkArrayIndex++ )
	{
		// Get the details for this connection attempt.
		const char* wifiSsid = wifiSsidList[networkArrayIndex];
		const char* wifiPassword = wifiPassArray[networkArrayIndex];

		// Announce the WiFi parameters for this connection attempt.
		Serial.print( "Attempting to connec to to SSID \"" );
		Serial.print( wifiSsid );
		Serial.println( "\"" );

		// Attempt to connect to this WiFi network.
		Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : "Failed!" );
		WiFi.begin( wifiSsid, wifiPassword );

		unsigned long wifiConnectionStartTime = millis();
		// Wait up to 10 seconds for a connection.
		Serial.print( "Waiting up to " );
		Serial.print( wifiConnectionTimeout / 1000 );
		Serial.print( " seconds for a connection" );
		/*
			WiFi.status() return values:
			0 : WL_IDLE_STATUS when WiFi is in process of changing between statuses
			1 : WL_NO_SSID_AVAIL in case configured SSID cannot be reached
			3 : WL_CONNECTED after successful connection is established
			4 : WL_CONNECT_FAILED if wifiPassword is incorrect
			6 : WL_DISCONNECTED if module is not configured in station mode
		*/
		while( WiFi.status() != WL_CONNECTED && ( millis() - wifiConnectionStartTime < wifiConnectionTimeout ) )
		{
			Serial.print( "." );
			delay( 1000 );
		}
		Serial.println( "" );

		if( WiFi.status() == WL_CONNECTED )
		{
			digitalWrite( LED_PIN, HIGH ); // Turn the LED on.
			Serial.print( "IP address: " );
			snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
			Serial.println( ipAddress );
			networkIndex = networkArrayIndex;
			// Print that WiFi has connected.
			Serial.println( "\nWiFi connection established!" );
			return;
		}
		else
		{
			Serial.println( "Unable to connect to WiFi!" );
		}
	}

	Serial.println( "Exiting wifiMultiConnect()\n" );
} // End of wifiMultiConnect() function.


/* 
 *  mqttConnect() will attempt to (re)connect the MQTT client.
 */
bool mqttConnect( int maxAttempts )
{
	if( WiFi.status() != WL_CONNECTED )
		wifiMultiConnect();

	digitalWrite( LED_PIN, LOW ); // Turn the LED off.

	/*
	 * The networkIndex variable is initialized to 2112.
	 * If it is still 2112 at this point, then WiFi failed to connect.
	 * This is only needed to display the name and port of the broker being used.
	 */
	if( networkIndex != 2112 )
	{
		Serial.print( "Attempting to connect to the MQTT broker at '" );
		Serial.print( mqttBrokerArray[networkIndex] );
		Serial.print( ":" );
		Serial.print( mqttPortArray[networkIndex] );
		Serial.print( "' up to " );
		Serial.print( maxAttempts );
		Serial.println( " times." );
	}
	else
	{
		Serial.print( "Attempting to connect to the MQTT broker up to " );
		Serial.print( maxAttempts );
		Serial.println( " times." );
	}

	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempt # " );
		Serial.print( i + 1 );
		Serial.print( "..." );

		// Put the macAddress and randNum into clientId.
		char clientId[22];
		snprintf( clientId, 22, "%s-%03d", macAddress, random( 999 ) );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		Serial.print( "Connecting with client ID '" );
		Serial.print( clientId );
		Serial.print( "' " );
		if( mqttClient.connect( clientId, mqttBrokerUserArray[networkIndex], mqttBrokerPassArray[networkIndex] ) )
		{
			digitalWrite( LED_PIN, HIGH ); // Turn the LED on.
			Serial.println( " connected!" );
			if( !mqttClient.setBufferSize( BUFFER_SIZE ) )
			{
				unableToComply( BUFFER_SIZE );
			}
			publishStats();
			// Subscribe to the command topic.
			mqttClient.subscribe( mqttCommandTopic );
		}
		else
		{
			int mqttState = mqttClient.state();
			/*
				Possible values for client.state():
				#define MQTT_CONNECTION_TIMEOUT     -4		// Note: This also comes up when the clientID is already in use.
				#define MQTT_CONNECTION_LOST        -3
				#define MQTT_CONNECT_FAILED         -2
				#define MQTT_DISCONNECTED           -1
				#define MQTT_CONNECTED               0
				#define MQTT_CONNECT_BAD_PROTOCOL    1
				#define MQTT_CONNECT_BAD_CLIENT_ID   2
				#define MQTT_CONNECT_UNAVAILABLE     3
				#define MQTT_CONNECT_BAD_CREDENTIALS 4
				#define MQTT_CONNECT_UNAUTHORIZED    5
			*/
			Serial.print( " failed!  Return code: " );
			Serial.print( mqttState );
			if( mqttState == -4 )
			{
				Serial.println( " - MQTT_CONNECTION_TIMEOUT" );
			}
			else if( mqttState == 2 )
			{
				Serial.println( " - MQTT_CONNECT_BAD_CLIENT_ID" );
			}
			else
			{
				Serial.println( "" );
			}

			Serial.print( "Trying again in " );
			Serial.print( mqttReconnectDelay / 1000 );
			Serial.println( " seconds." );
			delay( mqttReconnectDelay );
		}
		i++;
	}

	if( !mqttClient.connected() )
	{
		Serial.println( "Unable to connect to the MQTT broker!" );
		return false;
	}

	Serial.println( "Function mqttConnect() has completed." );
	return true;
} // End of mqttConnect() function.


// This function is used when buffer allocations fail.
void unableToComply( int bufferSize )
{
	Serial.print( "\n\n\n\nUnable to set the buffer size to " );
	Serial.println( bufferSize );
	Serial.println( "This device will not be able to run this program!" );
	Serial.println( "Please upload a different sketch." );
	while( true )
	{
		digitalWrite( LED_PIN, LOW );
		delay( FAILURE_BLINK_RATE );
		digitalWrite( LED_PIN, HIGH );
		delay( FAILURE_BLINK_RATE );
	}
}


/*
 * publishStats() will publish device information.
 */
void publishStats()
{
	char mqttStatsString[BUFFER_SIZE];
	long rssi = WiFi.RSSI();
	snprintf( mqttStatsString, BUFFER_SIZE, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"rssi\": \"%ld\"\n}", sketchName, macAddress, ipAddress, rssi );
	if( mqttClient.publish( mqttStatsTopic, mqttStatsString ) )
	{
		Serial.print( "Published this data to '" );
		Serial.print( mqttStatsTopic );
		Serial.print( "':\n" );
		Serial.print( mqttStatsString );
	}
	else
	{
		Serial.print( "\n\nPublish to '" );
		Serial.print( mqttStatsTopic );
		Serial.println( "' failed!\n\n" );
	}
} // End of publishStats() function.


void loop()
{
	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
		mqttConnect( 10 );

	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	unsigned long time = millis();
	// Only process this block every 'publishDelay' milliseconds.
	if( lastPublish == 0 || ( ( time - lastPublish ) > publishDelay ) )
	{
		loopCount++;
		Serial.println( sketchName + " loop # " + loopCount );
		Serial.println( "" );
		Serial.println( sketchName );
		Serial.print( "rssi: " );
		Serial.println( WiFi.RSSI() );

		publishStats();

		Serial.print( "Delaying " );
		Serial.print( publishDelay / 1000 );
		Serial.println( " seconds.\n" );
		lastPublish = millis();
	}
} // End of loop() function.
