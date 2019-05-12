#include <Arduino.h>
//------------------------
// Impulszähler
// Gibt die Impulse pro Sekunde
// des Spannungsignals an Pin 7 aus
//-------------------------
int pin = 2;
unsigned long N;          //Anzahl der Impulse
unsigned long T;          //Zeitintervall in us
unsigned long time;       //Startzeit

void setup(){
	Serial.begin(9600);
	pinMode(pin, INPUT);
	T = 1e6;
}

void loop(){
	N = 0;
	time = micros();
	do {
		if (pulseIn(pin, HIGH)>0) N++;
	}
	while( micros() < (time+T) );
	Serial.println(N);
}
