/*
 * Kapazitiver Füllstandsmesser für Elektronikwettbewerb 2019
 * @autor Fabian Franz
 */

#include <Arduino.h>
#include <EEPROM.h>

#define GREEN 15
#define YELLOW 16
#define RED 17
#define HIGHBUTTON 3
#define LOWBUTTON 4
#define OSZI 2
#define PERIOD 1e5

uint64_t getPulse(uint8_t pin, uint64_t interval);
void showLeds(uint8_t humidity);
void blinkLed(uint8_t pin, uint16_t period);
boolean buttonPressed(uint8_t pin);
void writeEEPROM(uint8_t adress, uint16_t value);
uint16_t readEEPROM(uint8_t adress);

void setup(){
}

void loop(){
	Serial.begin(9600);
	uint16_t dryVal = readEEPROM(0);
	uint16_t wetVal = readEEPROM(6);

	pinMode(GREEN, OUTPUT);
	pinMode(YELLOW, OUTPUT);
	pinMode(RED, OUTPUT);
	pinMode(HIGHBUTTON, INPUT);
	pinMode(LOWBUTTON, INPUT);
	pinMode(OSZI, INPUT);


	while(true) {
		volatile uint16_t pulse = getPulse(OSZI, PERIOD);
		int16_t humidity = 0;

		if(!digitalRead(LOWBUTTON)) {
			while(!digitalRead(LOWBUTTON)) {
				dryVal = pulse;
				digitalWrite(GREEN, LOW);
				digitalWrite(YELLOW, LOW);
				blinkLed(RED, 200);
			}
			Serial.print("Setze untere Schranke auf:");
			Serial.println(dryVal);
			writeEEPROM(0, dryVal);
			digitalWrite(RED, LOW);
		}
		if(!digitalRead(HIGHBUTTON)) {
			while(!digitalRead(HIGHBUTTON)) {
				wetVal = pulse;
				digitalWrite(YELLOW, LOW);
				digitalWrite(RED, LOW);
				blinkLed(GREEN, 200);
			}
			Serial.print("Setze obere Schranke auf:");
			Serial.println(wetVal);
			writeEEPROM(6, wetVal);
			digitalWrite(GREEN,LOW);
		}

		// Fehler abfangen
		if(wetVal > dryVal) {
			Serial.println("Fehler bei der Konfiguration");
			blinkLed(YELLOW, 500);
		}else{
			humidity = dryVal - pulse;
			if(humidity < 0) humidity = 0;
			humidity = ((float)humidity / (dryVal - wetVal)) * 255;
			if(humidity > 255) humidity = 255;
			showLeds(humidity);
			humidity = (humidity / 255.0) * 100;
			Serial.print("Feuchtigkeit: ");
			Serial.print(humidity);
			Serial.println("%");
		}
	}
}

uint64_t getPulse(uint8_t pin, uint64_t interval){
	uint64_t count = 0;
	uint64_t time = micros();

	do {
		if (pulseIn(pin, HIGH) > 0) count++;
	} while( micros() < (time + interval));
	return count;
}
void showLeds(uint8_t humidity){
	uint8_t dim = 0;

	if(humidity <= 15) {
		digitalWrite(GREEN, LOW);
		digitalWrite(YELLOW, LOW);
		blinkLed(RED, 1e3);
	}
	// rote LED dimmen, gelb und rot aus
	if(humidity > 15 && humidity <= 70) {
		digitalWrite(GREEN, LOW);
		digitalWrite(YELLOW, LOW);
		dim = (humidity / 70.0) * 255;
		analogWrite(RED, dim);
	}
	// gelbe LED dimmen, grün aus, rot an
	if(humidity > 70 && humidity <= 140) {
		digitalWrite(GREEN, LOW);
		digitalWrite(RED, HIGH);
		dim = humidity - 70;
		dim = (dim / 70.0) * 255;
		analogWrite(YELLOW, dim);
	}
	// grüne LED dimmen, gelb und rot an
	if(humidity > 140 && humidity <= 255) {
		digitalWrite(YELLOW, HIGH);
		digitalWrite(RED, HIGH);
		dim = humidity - 140;
		dim = (dim / 115.0) * 255;
		analogWrite(GREEN, dim);
	}
}
void blinkLed(uint8_t pin, uint16_t period){
	uint64_t currentMillis = millis();
	static uint64_t previousMillis = 0;

	if(digitalRead(pin) == LOW) {
		if(currentMillis > previousMillis + period) {
			previousMillis = currentMillis;
			digitalWrite(pin, HIGH);
		}
	} else {
		if(currentMillis > previousMillis + period) {
			previousMillis = currentMillis;
			digitalWrite(pin, LOW);
		}
	}
}
void writeEEPROM(uint8_t adress, uint16_t value){
	char buf[6];
	itoa(value, buf, 10);
	for(int i=0; i<6; i++) {
		EEPROM.write(adress+i, buf[i]);
	}
}
uint16_t readEEPROM(uint8_t adress){
	char buf[6];
	for(int i=0; i<6; i++) {
		buf[i] = EEPROM.read(adress+i);
	}
	return atoi(buf);
}
