#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <RemoteDebug.h>

// Update these with values suitable for your network.
const char* ssid = "PIRATA4_plus";
const char* password = "191215F98F";
const char* mqtt_server = "192.168.1.85";
const char* mqtt_user = "mqtt";
const char* mqtt_passwd = "mmqqtttt";

const float MaxDistance = 200;
const float threshold = 58;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// defines pins numbers trigger and echo
const int trigPin = D6;
const int echoPin = D7;

// defines variables
long duration;
float distance;
float lastDistance;
float distancesArray[10] = {100,100,100,100,100,100,100,100,100,100};
int distancesArrayCount = 0;

bool RemoteSerial = true; //True = Remote Serial, False = local serial
RemoteDebug RSerial;

void setup_wifi() {

	delay(10);
	// We start by connecting to a WiFi network
	RSerial.println();
	RSerial.print("Connecting to ");
	RSerial.println(ssid);

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	randomSeed(micros());

	RSerial.println("");
	RSerial.println("WiFi connected");
	RSerial.println("IP address: ");
	RSerial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
	RSerial.print("Message arrived [");
	RSerial.print(topic);
	RSerial.print("] ");
	for (int i = 0; i < length; i++) {
		RSerial.print((char)payload[i]);
	}
	RSerial.println();

	// Switch on the LED if an 1 was received as first character
	if ((char)payload[0] == '1') {
		digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
										  // but actually the LED is on; this is because
										  // it is acive low on the ESP-01)
	}
	else {
		digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
	}
}

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		RSerial.print("Attempting MQTT connection...");
		// Create a random client ID
		String clientId = "Arduino-Bomba";
		//clientId += String(random(0xffff), HEX);
		// Attempt to connect
		if (client.connect(clientId.c_str(), mqtt_user, mqtt_passwd)) {
			RSerial.println("connected");
			// Once connected, publish an announcement...
			client.publish("outTopic", "hello world");
			// ... and resubscribe
			client.subscribe("inTopic");
		}
		else {
			RSerial.print("failed, rc=");
			RSerial.print(client.state());
			RSerial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void distanceData() {
	digitalWrite(trigPin, LOW);
	delayMicroseconds(2);

	// Sets the trigPin on HIGH state for 10 micro seconds
	digitalWrite(trigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);

	// Reads the echoPin, returns the sound wave travel time in microseconds
	duration = pulseIn(echoPin, HIGH);

	// Calculating the distance
	distance = (duration*0.034) / 2;

	//Serial.println("Distance=" + String(distance, 2));
}

void distanceDataErrorFree() {
	digitalWrite(trigPin, LOW);
	delayMicroseconds(2);

	// Sets the trigPin on HIGH state for 10 micro seconds
	digitalWrite(trigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);

	// Reads the echoPin, returns the sound wave travel time in microseconds
	duration = pulseIn(echoPin, HIGH);

	// Calculating the distance
	distance = (duration*0.034) / 2;

	RSerial.print("Real distance: ");
	RSerial.println(distance);

	//Eliminate measure errors
	if (distance > MaxDistance || distance <= 0) {
		distance = lastDistance;
		RSerial.println("ERROR");
	}
	else {
		lastDistance = distance;
	}

	//RSerial.print("Array[");
	//RSerial.print(distancesArrayCount);
	//RSerial.print("]: ");
	//RSerial.println(distancesArray[distancesArrayCount]);

	distancesArray[distancesArrayCount] = distance;
	distancesArrayCount = distancesArrayCount + 1;
	distancesArrayCount = distancesArrayCount % 10;
}

float average(float * array, int len)  // assuming array is int.
{
	float sum = 0L;  // sum will be larger than an item, long for safety.

	//RSerial.print("Array average Length: ");
	//RSerial.println(len);
	for (int i = 0; i < len; i++)
		if (array[i] > 0) {
			sum += array[i];
		}
	//RSerial.print("Sum: ");
	//RSerial.println(sum);
	return  ((float)sum) / len;  // average will be fractional, so float may be appropriate.
}

void setup() {
	pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
	pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
	pinMode(echoPin, INPUT); // Sets the echoPin as an Input
	Serial.begin(115200);
	setup_wifi();
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);

	RSerial.begin("192.168.1.113");
	RSerial.setSerialEnabled(true);
}

void loop() {

	if (!client.connected()) {
		reconnect();
	}
	client.loop();

	long now = millis();
	if (now - lastMsg > 2000) {
		lastMsg = now;
		++value;

		StaticJsonBuffer<200> jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		int waterAlarm;

		distanceDataErrorFree();
		float averageData = average(distancesArray, 10);
		RSerial.print("Average distance: ");
		RSerial.println(averageData);

		if (averageData < threshold) {
			root["water"] = "1";
			waterAlarm = 1;

		}
		else {
			root["water"] = "0";
			waterAlarm = 0;
		}

		// INFO: the data must be converted into a string; a problem occurs when using floats...
		root["current"] = String(distance, 1);

		char data[200];
		root.printTo(data, root.measureLength() + 1);

		snprintf(msg, 75, "Distance to Top #%ld", distance);
		RSerial.print("Publish distance: ");
		RSerial.println(distance);

		snprintf(msg, 75, "Water Alarm #%ld", distance);
		RSerial.print("Publish water alarm: ");
		RSerial.println(waterAlarm);

		client.publish("basement/sump", data);
	}
	if (RemoteSerial) RSerial.handle();
}
