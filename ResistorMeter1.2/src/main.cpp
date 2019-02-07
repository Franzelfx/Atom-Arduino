/*
 * @autor Andre Spielvogel, FabianFranz, Stephen Zailer
 */

 #include <Arduino.h>
 #include <Wire.h>
 #include <SPI.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_SSD1306.h>
 #include <EEPROM.h>
/*
 * Betrieb mit Arduino UNO in Verbindung mit
 * dem Shield "Mikroprzessortechnik 2018"
 * => #define UNO
 * Betrieb mit Arduino NANO in Verbindung mit
 * dem Shield "Bauteileprüfer OLED-MK1"
 * => #define NANO
 */

 #define RES1 466.0F
 #define RES2 46900.0F
 #define RES3 100700.0F
 #define V_REF 5.0F
 #define DIGITS 1023
 #define DEBOUNCE 10
 #define NANO
 #ifdef UNO // => Arduino UNO
 #define BUTTON 9
 #define T_PIN1 10
 #define T_PIN2 11
 #define T_PIN3 12
 #define T_PIN4 13
 #define M_PIN1 A4
 #define M_PIN2 A5
 #endif
 #ifdef NANO // => Arduino NANO
 #define BUTTON 8
 #define T_PIN1 9
 #define T_PIN2 10
 #define T_PIN3 11
 #define T_PIN4 12
 #define M_PIN1 A3
 #define M_PIN2 A2
 #endif
 #define TAU_1 63
 #define TAU_5 99
 #define LOADSWITCH 1000000
 #define TRASHHOLD 200
 #define CLOCKLOAD 100000
 #define WRONGPOLARITY 0
 #define RIGHTPOLARITY 1
// #define OPENCONNECTION 42900.00
 #define OLED_ADDR 0x3C
 #define SCREEN_WIDTH 128
 #define SCREEN_HEIGHT 64
 #define OLED_RESET -1
 #define OMEGA 8
 #define MYCRO 6
 #define FONTSIZE 6
 #define BORDERSPACE 2
 #define PROCESSINGMODE 1
 #define MEASUREREPEATS 2
 #define SHOWMEASURES 3
 #define EXITMENU 4
 #define BUFFSIZE 64
 #define FLOATBUFF 6
 #define INPUTTENS 3
 #define MEASUREDELAY 10
 #define MEASUREMINBORDER 10.F
 #define DEBUG true
 #define DEBUGTRASHHOLD 5
 #define MAXREPEATS 10
 #define PM_ADDR 0
 #define MR_ADDR 1

/*
 * Funktionen für die Werteermittlung:
 * Verantwortlicher: Fabian Fabian
 * Matrikel Nummer: 644414
 */
void trigger(uint8_t source, uint8_t state);
boolean load(uint8_t polarity, uint8_t percent);
boolean loadSlow(uint8_t percent);
boolean unload(uint8_t polarity);
int32_t getLoadDuration(uint8_t polarity, uint8_t percent);
uint32_t getSlowLoadDuration(uint8_t percent);
float getResistorValue(uint8_t source);
float getCapacitorValue(uint8_t percent);
int8_t getPolarity();
uint8_t getConnection();

/*
 * Funktionen für die Werteverarbeitung
 * Verantwortlicher: Andre Spielvogel
 * Matrikel Nummer: 644949
 */
void sort(float *values, uint8_t length);
void resetArray(char *values);
float median(float *value, uint8_t length);
float average(float *values, uint8_t length);
float deviation(float *values, uint8_t length, float average);

/*
 * Funktionen für die LCD-Ausgabe
 * Verantwortlicher: Fabian Franz
 * Matrikel Nummer: 644414
 */
void resistorCircuit(uint8_t y);
void capacitorCircuit(uint8_t y);
void showResistorScreen(float value);
void showCapacitorScreen(float value);
void showStringInCentral(String value);

/*
 * Funktionen für die Terminal-Ausgabe
 * Verantwortlicher: Fabian Franz
 * Matrikel Nummer: 644414
 */
void bootDevice();
void showResistorTerminal(float value);
void showCapacitorTerminal(float value);
void showLastMeasurements(uint32_t *values, uint8_t length);
void showLastMeasurements(float *values, uint8_t length, uint8_t count);

/*
 * Funktionen für die Menüführung
 * Verantwortlicher: Stephen Zailer
 * Matrikel Nummer: 641904
 */
uint8_t getInt();
boolean Marker(char *text);
uint8_t mainMenu();
uint8_t getMeasureRepeats();
uint8_t getProcessingMode();

/*
 * Sonstige Funktionen
 * Verantwortlicher: Fabian Franz
 * Matrikel Nummer: 644414
 */
void setParts();
boolean buttonPressed();
void copyArrays(float *firstArr, uint8_t length1, float *secondArr, uint8_t length2);

// Konstruktor für das Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup(){

}
void loop(){
// Variablen für Werteverarbeitung
	float currentValue = 0;
	float previousValue = 0;
	uint8_t processingmode = 1; // Einfache Messung als Standard
	processingmode = EEPROM.read(PM_ADDR); // hole Wert aus EEPROM Adresse 0
	uint8_t measureRepeats = 1; // Eine Messwiederholung als Standard
	measureRepeats = EEPROM.read(MR_ADDR); // hole Wert aus EEPROM Adresse 1
	int8_t polarity = -1;
	float originalValues[MAXREPEATS];
	float copyValues[MAXREPEATS];
	float currentMedian = 0;
	float currentAverage = 0;
	float stdDeviation = 0;
	for(int i=0; i<MAXREPEATS; i++) originalValues[i] = 0;  // hole ab Adresse 2
	for(int i=0; i<MAXREPEATS; i++) copyValues[i] = 0;

// Hilfsvariablen
	uint8_t countR = 0;
	uint8_t countC = 0;
	boolean showMenue = false;
	uint8_t component = false;
	boolean displayConnected = false;

	// Voreinstellungen
	Serial.begin(9600);
	setParts();
	bootDevice();
	if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
		Serial.println("[ERROR]: SSD1306 Oled- Display nicht erkannt");
		displayConnected = false;
	}else{
		display.setTextSize(1); // 6 Pixel
		display.setTextColor(WHITE);
		display.clearDisplay();
		display.setCursor(1, 1);
		display.print("Bauteilemesser");
		display.display();
		displayConnected = true;
	}

	/*
	 * Main Loop des Messgerätes:
	 * => Der Betrieb ist Headless über Display, sowie über das Terminal möglich.
	 * => Auf dem Terminal ist es möglich weitere Auswertungen der Messungen vorzunehmen,
	 *    wie den Durchschnitt, Median und Standardabweichung mehererer Messdurchläufe.
	 * => Wird das Messgerät Headless betrieben, so wird nur eine Messung pro Durchlauf
	 *    ausgeführt und angezeigt.
	 */

	while(true) {
		if(buttonPressed()) showMenue = true;
		if(showMenue && Serial.availableForWrite()) {  // wenn Taster gedrückt zeige Hauptmenü
			switch(mainMenu()) {
			case PROCESSINGMODE:
				processingmode = getProcessingMode();  // schreibe Messmodus in EEPROM
				EEPROM.write(PM_ADDR, processingmode);
				break;
			case MEASUREREPEATS:
				measureRepeats = getMeasureRepeats();
				EEPROM.write(MR_ADDR, measureRepeats);  // schreibe Messwiederholungen in EEPROM
				break;
			case SHOWMEASURES:
				showLastMeasurements(originalValues, sizeof(originalValues) / sizeof(float), measureRepeats);
				currentAverage = average(originalValues, measureRepeats);
				if(currentAverage) {
					Serial.print("Standardabweichung: ");
					stdDeviation = deviation(originalValues, measureRepeats, currentAverage);
					Serial.println(stdDeviation);
				}
				break;
			case EXITMENU: showMenue = false; break;
			default: Serial.println("[ERROR]: Menüfehler"); break;
			}
		}else{
			/*
			 * Messablauf für
			 * => 1 = Wiederstand
			 * => 2 = Kondensator
			 */
			component = getConnection();  // prüfe welches Bauteil angeschlossen ist
			switch(component) {
			case 0: break;
			case 1:
				while(countR < measureRepeats) {
					currentValue = getResistorValue(T_PIN2);  // hole Wiederstandswert
					if(currentValue == -1) break; // Bei Messfehler Springe zum Anfang
					if(currentValue > 0.0) {
						originalValues[countR] = currentValue;  // schreibe Wiederstandswerte in Array
						// Serial.println(currentValue);
						countR++;
					}
				} countR = 0;
				break;
			case 2:
				while(countC < measureRepeats) {
					if(buttonPressed()) {  // Springe aus Abfrage wenn Knopf gedrückt
						showMenue = true;
						break;
					}
					currentValue = getCapacitorValue(TAU_1); // hole Kondensatorwert
					if(currentValue == -1) break; // Bei Messfehler Springe aus Abfrage
					if(currentValue > 0.0) {
						originalValues[countC] = currentValue; // schreibe Kondensatorwerte in Array
						// Serial.println(currentValue);
						countC++;
					}
				} countC = 0;
				if(showMenue) break;  // Springe zum Menü
				do {
					if(buttonPressed()) {  // Springe aus Abfrage wenn Knopf gedrückt
						showMenue = true;
						break;
					}
					polarity = getPolarity();
					if(!polarity) Serial.println("!!!Falsch gepolt!!!");
				} while(polarity == -1);
				break;
			default: Serial.println("[ERROR]: Bauteileerkennungsfehler"); break;
			}

			/*
			 * Auswertung der aufgeneommenen Messwerte
			 * => 1 = Keine Auswertung, lediglich Anzeige des letzen Messwertes
			 * => 2 = Ausgabe des Durchschnittswertes
			 * => 3 = Ausgabe des Medianwertes
			 */
			if(!showMenue) {  // gehe nur in die Ausgabe falls nicht das Menü gezeigt werden soll
				switch(processingmode) {
				case 1:
					if(currentValue != previousValue) { // Prüfen ob Wert sich geändert hat
						if(component == 1) { // Ausgabe je nach Bauteil
							if(displayConnected) showResistorScreen(currentValue);
							showResistorTerminal(currentValue);
						}
						if(component == 2) {
							if(displayConnected) showCapacitorScreen(currentValue);
							showCapacitorTerminal(currentValue);
						}
						previousValue = currentValue;
					}
					break;
				case 2:
					currentAverage = average(originalValues, measureRepeats);
					if(component == 1) { // Ausgabe je nach Bauteil
						if(displayConnected) showResistorScreen(currentAverage);
						showResistorTerminal(currentAverage);
					}
					if(component == 2) {
						if(displayConnected) showCapacitorScreen(currentAverage);
						showCapacitorTerminal(currentAverage);
					}
					break;
				case 3:
					copyArrays(originalValues, measureRepeats, copyValues, measureRepeats);
					currentMedian = median(copyValues, measureRepeats);
					if(component == 1) { // Ausgabe je nach Bauteil
						if(displayConnected) showResistorScreen(currentMedian);
						showResistorTerminal(currentMedian);
					}
					if(component == 2) {
						if(displayConnected) showCapacitorScreen(currentMedian);
						showCapacitorTerminal(currentMedian);
					}
					break;
				default: Serial.println("[ERROR]: Ausgabefehler"); break;
				}
			}
		}
	}
}

// Funktionen für die Werteermittlung:
void trigger(uint8_t source, uint8_t state){
	/*
	 * Schaltet die Pins je nach gebrauch,
	 * Erforderliche Parameter:
	 *  => source = zu schaltender Pin
	 *  => state = Pin an oder aus
	 */
	if(source == T_PIN1) {
		pinMode(M_PIN1, OUTPUT);
		pinMode(M_PIN2, INPUT);
		pinMode(T_PIN2, INPUT);
		pinMode(T_PIN3, INPUT);
		pinMode(T_PIN4, INPUT);
		pinMode(source, OUTPUT);
		digitalWrite(M_PIN1, LOW);
		digitalWrite(source, state);
	}
	if(source == T_PIN2 || source == T_PIN3 || source == T_PIN4) {
		switch(source) {
		case T_PIN2:
			pinMode(T_PIN1, INPUT);
			pinMode(T_PIN3, INPUT);
			pinMode(T_PIN4, INPUT);
			break;
		case T_PIN3:
			pinMode(T_PIN1, INPUT);
			pinMode(T_PIN2, INPUT);
			pinMode(T_PIN4, INPUT);
			break;
		case T_PIN4:
			pinMode(T_PIN1, INPUT);
			pinMode(T_PIN2, INPUT);
			pinMode(T_PIN3, INPUT);
			break;
		}
		pinMode(M_PIN2, OUTPUT);
		pinMode(M_PIN1, INPUT);
		pinMode(source, OUTPUT);
		digitalWrite(M_PIN2, LOW);
		digitalWrite(source, state);
	}
}
boolean load(uint8_t polarity, uint8_t percent){
	/*
	 * Lädt einen Kondensator nach der Polung,
	 * Erforderliche Parameter:
	 *  => richtige Polung = 1
	 *  => false Polung = 0
	 *  => Prozent der gewünschten Ladung
	 *  Rückgabeparameter:
	 *  => Beendigung des Ladenzustandes als Wahrheitswert
	 */
	uint16_t loadDigits = 0;

	if(polarity) {
		trigger(T_PIN2, HIGH);
		loadDigits = analogRead(M_PIN1);
	}
	else {
		trigger(T_PIN1, HIGH);
		loadDigits = analogRead(M_PIN2);
	}
	if(loadDigits >= (int16_t)(1023.0 / 100.0 * float(percent))) return true;
	else return false;
}
boolean loadSlow(uint8_t percent){
	/*
	 * Lädt einen Kondensator langsamer,
	 * Erforderliche Parameter:
	 *  => richtige Polung = 1
	 *  => false Polung = 0
	 *  => Prozent der gewünschten Ladung
	 *  Rückgabeparameter:
	 *  => Beendigung des Ladenzustandes als Wahrheitswert
	 */
	uint16_t loadDigits = 0;

	trigger(T_PIN4, HIGH);
	loadDigits = analogRead(M_PIN1);
	if(loadDigits >= (int16_t)(1023.0 / 100.0 * float(percent))) return true;
	else return false;
}
boolean unload(uint8_t polarity){
	/*
	 * Entlädt einen Kondensator nach der Polung:
	 * Erforderliche Parameter:
	 *  => richtige Polung = 1
	 *  => false Polung = 0
	 *  Rückgabeparameter:
	 *  => Beendigung des Ladenzustandes als Wahrheitswert
	 */
	uint16_t loadDigits = 0;

	if(polarity) {
		trigger(T_PIN2, LOW);
		loadDigits = analogRead(M_PIN1);
	}
	else {
		trigger(T_PIN1, LOW);
		loadDigits = analogRead(M_PIN2);
	}
	if(loadDigits) return false;
	else return true;
}
int32_t getLoadDuration(uint8_t polarity, uint8_t percent){
	/*
	 * Gibt die Ladezeit eines Kondensators zurück.
	 * Erforderliche Parameter:
	 *  => Prozent der gewünschten Ladung
	 *  => gewünschte Polarität der Aufladung
	 *  Rückgabeparameter:
	 *  => Zeit der Ladung in Mikrosekunden
	 *  Hinweis: Gibt -1 zurück falls Kondensator in
	 *  sehr kurzer Zeit geladen oder nicht angeschlossen
	 *  Gibt -2 zurück im Entladevorgang
	 */
	static uint_fast64_t previousMicros = 0;
	static uint8_t mode = 0;
	int32_t deltaT = 0;

	if(!mode) {
		if(unload(polarity)) mode++; // wenn entladen
		deltaT = -2;
	} else {
		if(mode == 1) {
			previousMicros = micros();
			deltaT = -2;
			mode++;
		}
		if(load(polarity, percent)) {
			deltaT = (micros() - previousMicros);
			if(deltaT < TRASHHOLD) {
				deltaT = -1;
			}
			mode = 0;
		}
	}
	return deltaT;
}
uint32_t getSlowLoadDuration(uint8_t percent){
	/*
	 * Gibt die Ladezeit eines Kondensators zurück.
	 * Wenn noch kein Messwert vorhanden Rückgabe von -2.
	 * Wenn noch kein Messwert fehlerhaft Rückgabe von -1.
	 * Erforderliche Parameter:
	 *  => Prozent der gewünschten Ladung
	 *  Rückgabeparameter:
	 *  => Zeit der Ladung in Mikrosekunden
	 */
	static uint_fast64_t previousMicros = 0;
	static uint8_t mode = 0;
	uint32_t deltaT = 0;

	if(!mode) {
		if(unload(RIGHTPOLARITY)) mode++; // wenn entladen
		deltaT = -2;
	} else {
		if(mode == 1) {
			previousMicros = micros();
			deltaT = -2;
			mode++;
		}
		if(loadSlow(percent)) {
			deltaT = (micros() - previousMicros);
			if(deltaT < TRASHHOLD) {
				deltaT = -1;
			}
			mode = 0;
		}
	}
	if(deltaT < TRASHHOLD) deltaT = 0;
	return deltaT;
}
float getResistorValue(uint8_t source){
	/*
	 * Gibt die Größe eines Widerstandes zurück
	 * und erhöt den Messbereich bei großen Wiederständen.
	 * Wenn noch kein Messwert vorhanden Rückgabe von -1;
	 * Erforderliche Parameter:
	 * => Messpin, an welchem gemessen werden soll
	 * Rückgabeparameter:
	 * => Wiederstndsgrösse in Ohm
	 */
	float voltage = 0;
	float toMeasure = 0;
	float measureResistor;

	trigger(source, HIGH);
	voltage = float(analogRead(M_PIN1)) * (V_REF / float(DIGITS));
	trigger(source, LOW);
	switch(source) {  // Je nach Spannungsquelle
	case T_PIN2:
		measureResistor = RES1;
		if(voltage == V_REF-0.01) return -1; // Bei unterschreiten der spezifischen Referenzspannung gebe Fehler aus
		if(voltage > V_REF/2) toMeasure = getResistorValue(T_PIN3); // Messbereich Umschalten, falls über halbe Refernzspannung
		else toMeasure = measureResistor * (voltage / (V_REF - voltage));
		break;
	case T_PIN3: measureResistor = RES2;
		if(voltage > V_REF-0.5) return -1;
		if(voltage > V_REF/2) toMeasure = getResistorValue(T_PIN4);
		else toMeasure = measureResistor * (voltage / (V_REF - voltage));
		break;
	case T_PIN4: measureResistor = RES3;
		if(voltage > V_REF-1.0) return -1;
		toMeasure = measureResistor * (voltage / (V_REF - voltage));
		break;
	}
	// Serial.println(toMeasure);
	return toMeasure;
}
float getCapacitorValue(uint8_t percent){
	/*
	 * Gibt die Größe eines Kondensators zurück
	 * und erhöt den Messbereich bei kleinen Kapazitäten.
	 * Wenn noch kein Messwert vorhanden Rückgabe von -2.
	 * Wenn noch kein Messwert fehlerhaft Rückgabe von -1.
	 * Erforderliche Parameter:
	 * => Prozent: wie weit soll der Kondensator geladen werden
	 * Rückgabeparameter:
	 * => Kondensatorgrösse in Mikrofarad
	 */
	int32_t deltaT = 0;
	float measuredFarad = 0;
	static uint8_t mode = 0;

	if(!mode) {
		deltaT = getLoadDuration(RIGHTPOLARITY, TAU_1);
		measuredFarad = float(deltaT) / RES1;
	}else{
		deltaT = getSlowLoadDuration(TAU_1);
		measuredFarad = float(deltaT) / RES3;
	}
	return measuredFarad;
}
int8_t getPolarity(){
	/*
	 * Gibt die Polungsrichtung eines Kondensators zurück.
	 *  Rückgabeparameter:
	 *  => falsch gepolt = 0
	 *  => richtig gepolt = 1
	 *  => default = 0
	 */
	int8_t polarity = -1;
	static uint8_t passing = 0;
	static int32_t loadC = 0;
	static int32_t _loadC = 0;

	switch(passing) {
	case 0:
		loadC = getLoadDuration(RIGHTPOLARITY, TAU_5);
		if(loadC > 0) passing++;
		break;
	case 1:
		if(unload(RIGHTPOLARITY)) passing++;
		break;
	case 2:
		_loadC = getLoadDuration(WRONGPOLARITY, TAU_5);
		if(_loadC > 0) passing++;
		break;
	case 3:
		if(unload(WRONGPOLARITY)) {
			passing = 0;
			if(loadC > _loadC) polarity = 0;
			else polarity = 1;
			loadC = 0;
			loadC = 0;
		}
		break;
	}
	return polarity;
}
uint8_t getConnection(){
	/*
	 * Gibt die das angeschlossenen Bauteil zurück.
	 *  Rückgabeparameter:
	 *  => Wiederstand: 1
	 *  => Kondenator: 2
	 *  => default = 0
	 */
	float capacitor = 0;
	uint8_t connection = 0;

	capacitor = getLoadDuration(RIGHTPOLARITY, 10);
	if(capacitor > -2) {
		if(capacitor != -1) connection = 2; // Kondensator angeschlossen
		else{
			if(getResistorValue(T_PIN2) != -1) connection = 1; // Wiederstand angeschlossen
		}
	}
	return connection;
}

// Funktionen für die Werteverarbeitung:
void sort(float *values, uint8_t length){
	/*
	 * Sortiert ein Array aus Messwerten.
	 * Erforderliche Parameter:
	 * => Messwerte
	 * => Länge des Array an Messwerten
	 */
	for (uint8_t i = 1; i < length; i++) {
		for (uint8_t k = 0; k < length - i; k++) {
			if (values[k] > values[k + 1]) {
				float buf = values[k];
				values[k] = values[k + 1];
				values[k + 1] = buf;
			}
		}
	}
}
void resetArray(char *values){
	/*
	 * Löscht ein Array aus Char Werten.
	 * Erforderliche Parameter:
	 * => Char Werte
	 */
	while(*values != '\0') {
		*values = '\0';
		values++;
	}
}
float median(float *values, uint8_t length){
	/*
	 * Gibt aus einem Array an Messwerten den Median zurück.
	 * Erforderliche Parameter:
	 * => Messwerte
	 * => Länge des Array an Messwerten
	 */
	float median = 0;
	uint8_t mid = 0;

	sort(values, length);
	if(length == 1) return values[0];
	if(length % 2 != 0) {  // wenn Rest => Mitte existiert
		mid = (length / 2); // z.B.: Länge 9 => Hälfte = 4.5, Komma wird "weggeworfen" 4 besitz selben Abstand zu 0 wie zu 8
		median = values[mid];
	}else {
		mid = length / 2;  // z.B.: Länge 10 => Hälfte = 5, Abstand zu 0 = 5, Abstand zu 9 = 4 => 1 subtrahieren
		median = (values[mid] + values[mid - 1]);
		median = median / 2;
	}
	return median;
}
float average(float *values, uint8_t length){
	/*
	 * Gibt aus einem Array an Messwerten den Durchschnitt zurück.
	 * Erforderliche Parameter:
	 * => Messwerte
	 * => Länge des Array an Messwerten
	 */
	float average = 0;
	for(uint8_t i=0; i<length; i++) {
		average += values[i];
	}
	average = average/length;
	return average;
}
float deviation(float *values, uint8_t length, float average){
	/*
	 * Gibt aus einem Array an Messwerten die Standardabweichung zurück.
	 * Erforderliche Parameter:
	 * => Messwerte
	 * => Länge des Array an Messwerten
	 * Rückgabeparameter:
	 * => Standardabweichung
	 */
	float deviation = 0;
	for(uint8_t i=0; i<length; i++) {
		deviation = deviation + (values[i]-average) * (values[i]-average);
	}
	deviation = deviation / float(length);
	deviation = sqrt(deviation);
	return deviation;
}

// Funktionen für die LCD-Ausgabe:
void resistorCircuit(uint8_t y){
	for(uint8_t i=0; i<35; i++) display.drawPixel(i, y, WHITE);
	for(uint8_t i=y-5; i<=y+5; i++) {
		for(int k=35; k<=93; k++) {
			display.drawPixel(k, i, WHITE);
		}
	}
	for(uint16_t i=94; i<=SCREEN_WIDTH; i++) display.drawPixel(i, y, WHITE);
	display.display();
}
void capacitorCircuit(uint8_t y){
	for(uint8_t i=0; i<=50; i++) display.drawPixel(i, y, WHITE);
	for(uint8_t i=y-10; i<=y+10; i++) display.drawPixel(50, i, WHITE);
	for(uint8_t i=y-10; i<=y+10; i++) display.drawPixel(78, i, WHITE);
	for(uint8_t i=78; i<=SCREEN_WIDTH; i++) display.drawPixel(i, y, WHITE);
	display.display();
}
void showResistorScreen(float value){
	int fontConst = SCREEN_HEIGHT-FONTSIZE-BORDERSPACE;
	static float oldValue;
	const uint8_t omegaSize = 8;
	byte omega[omegaSize][omegaSize]{
		{0,0,1,1,1,1,0,0},
		{0,1,0,0,0,0,1,0},
		{1,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,1},
		{0,1,0,0,0,0,1,0},
		{0,0,1,0,0,1,0,0},
		{0,0,1,0,0,1,0,0},
		{1,1,1,0,0,1,1,1}
	};

	if(value!=oldValue) {
		display.clearDisplay();
		display.setTextColor(WHITE);
		display.setCursor(BORDERSPACE,FONTSIZE);
		display.print("Wiederstand");
		resistorCircuit((uint16_t)SCREEN_HEIGHT/2);
		display.setCursor(BORDERSPACE,fontConst);
		display.print("Wert: ");
		display.setCursor(display.getCursorX(), fontConst);
		display.print((uint32_t)value);
		for(int i=0; i<omegaSize; i++) {
			for(int k=0; k<omegaSize; k++) {
				if(omega[k][i]==1) display.drawPixel(i+display.getCursorX()+2, k+fontConst, WHITE);
			}
		}
		oldValue = value;
		display.display();
	}
}
void showCapacitorScreen(float value){
	int fontConst = SCREEN_HEIGHT-FONTSIZE-BORDERSPACE;
	static float oldValue;
	const uint8_t mycroSize = 6;
	byte mycro[mycroSize ][mycroSize]{
		{1,1,0,0,1,0},
		{1,1,0,0,1,0},
		{1,1,0,0,1,0},
		{1,0,1,1,0,0},
		{1,0,0,0,0,0},
		{1,0,0,0,0,0}
	};

	if(value!=oldValue) {
		display.clearDisplay();
		display.setTextColor(WHITE);
		display.setCursor(BORDERSPACE,FONTSIZE);
		display.print("Kondensator");
		capacitorCircuit((uint8_t)SCREEN_HEIGHT/2);
		display.setCursor(BORDERSPACE,fontConst);
		display.print("Wert: ");
		display.setCursor(display.getCursorX(), fontConst);
		display.print(value);
		for(int i=0; i<mycroSize; i++) {
			for(int k=0; k<mycroSize; k++) {
				if(mycro[k][i]==1) display.drawPixel(i+display.getCursorX()+FONTSIZE, k+fontConst, WHITE);
			}
		}
		for(int i=SCREEN_WIDTH; i>0; i--) {
			if(display.getPixel(i, fontConst)) {
				display.setCursor(i+2, fontConst);
				break;
			}
		}
		display.print('F');
		oldValue = value;
		display.display();
	}
}
void showStringInCentral(String value){
	display.clearDisplay();
	display.setTextColor(WHITE);
	display.setCursor((uint8_t) SCREEN_WIDTH, (uint8_t)SCREEN_HEIGHT/2-FONTSIZE);
	display.print(value);
	display.display();
}

// Funktionen für die Terminal-Ausgabe:
void bootDevice(){
	/*
	 * Zeigt einen Anfangsbildschirm auf dem terminal
	 */
	Serial.println("------------------------------------------------------------------------------------");
	Serial.println("Dies ist ein digitales Messgerät für Widerstände von 500 Ohm bis 500kOhm,");
	Serial.println("sowie für Kondensatoren von 0,5 µF bis 100 µF");
	Serial.println("------------------------------------------------------------------------------------");
}
void showCapacitorTerminal(float value){
	/*
	 * Zeigt die Den Wert eines angeschlossenen
	 * Kondensators auf dem terminal
	 * Übergabeparameter:
	 * => value = Grösse des Kondensators
	 */
	Serial.println("------------------------------------------------------------------------------------");
	Serial.println("Kondensator: --||--");
	Serial.print("Wert:");
	Serial.print(value);
	Serial.println(" µF");
}
void showResistorTerminal(float value){
	/*
	 * Zeigt die Den Wert eines angeschlossenen
	 * Widerstandes auf dem terminal
	 * Übergabeparameter:
	 * => value = Grösse des Wiederstandes
	 */
	Serial.println("------------------------------------------------------------------------------------");
	Serial.print("Wiederstand: ");
	Serial.print("--");
	Serial.print("==");
	Serial.println("--");
	Serial.print("Wert:");
	Serial.print(value);
	Serial.println(" Ω");
}
void showLastMeasurements(float *values, uint8_t length, uint8_t count){
	/*
	 * Zeigt die letzten Messwerte auf dem Terminal
	 * an getrennt durch ein Komma
	 * Übergabeparameter:
	 * => values = Letze Messwerte als Pointer auf ein Array
	 * => length = Länge des Array
	 * => count = Anzuzeigende Zahl von Werten
	 */
	char buf[BUFFSIZE];

	Serial.println("------------------------------------------------------------------------------------");
	sprintf(buf, "Die letzten \%d Messwerte sind:", count);
	Serial.println(buf);
	for(uint8_t i=0; i<count; i++) {
		Serial.print(values[i]);
		if(i != count -1) Serial.print(",");
	}
	Serial.println();
}

// Funktionen für die Menüführung:
uint8_t getInt() {
	/*
	 * Ermittelt eine Nummer, welche in der KOnsole eingegeben wurde.
	 * Rückgabeparameter:
	 * => Eingegebene Zahl als 8-Bit Integer Datentyp (Vorzeichenunbehaftet)
	 */
	const uint8_t numChars = 4;
	char *receivedChars = (char*)calloc(numChars, sizeof(char));
	boolean newData = false;
	while (!newData) {
		newData = Marker(receivedChars);
		if(buttonPressed()) return 4;
	}
	return atoi(receivedChars);
}
boolean Marker(char *text) {
	/*
	 * Ermittelt ob ein Enterzeichen in der Konsole eingegeben wird.
	 * Rückgabeparameter:
	 * => Wahrheitswert ob Enter gedrückt wurde
	 */
	const uint8_t numChars = 4;
	static uint8_t Index = 0;
	static boolean once = false;
	char endMarker = '\n';
	char readchar;

	if(!once) {
		Serial.print("Eingabe: ");
		once = true;
	}
	if (Serial.available() > 0) {
		readchar = Serial.read();
		Serial.print(readchar);

		if (readchar != endMarker) {
			text[Index] = readchar;
			Index++;
			if (Index >= numChars) {
				Index = numChars - 1;
			}
		}
		else {
			text[Index] = '\0';
			Index = 0;
			once = false;
			return true;
		}
	}
	return false;
}
uint8_t mainMenu(){
	/*
	 * Zeigt dem Benutzer eine Oberfläche für die Menüführung
	 * und gibt den ausgewählten Menüpunkt zurück.
	 * Rückgabeparameter:
	 * => Menüpunkt:
	 * => 1 = Messwertmittlung
	 * => 2 = Messwertwiederholungen
	 * => 3 = Letzte Messwerte
	 * => 4 = Menü verlassen
	 */
	uint8_t input = 0;

	Serial.println("------------------------------------------------------------------------------------");
	Serial.println("Arduino Kapazität und Wiederstand Messgerät Hauptmenü:");
	Serial.println("1: Messwertmittlung festlegen");
	Serial.println("2: Messwertwiederholungen festegen");
	Serial.println("3: Letzte Messwerte anzeigen");
	Serial.println("4: Menü verlassen");
	input = getInt();
	if(input > 4) {
		Serial.println ("---------------Überprüfe deine Eingabe---------------");
		input = mainMenu();
	}
	return input;
}
uint8_t getMeasureRepeats() {
	/*
	 * Zeigt dem Benutzer eine Oberfläche für die einzugebenen Messwiederholungen
	 * und gibt die durchzuführenden Messwiederholungen zurück.
	 * Rückgabeparameter:
	 * => Anzahl an Messungen, welche durchgeführt werden sollen
	 */
	uint8_t input = 0;

	Serial.println("------------------------------------------------------------------------------------");
	Serial.print ("Gib die Anzahl der Messungen ein. Gültige Eingaben sind 1-");
	Serial.println(MAXREPEATS);
	input = getInt();
	if(input > MAXREPEATS || input < 1) {
		Serial.println("---------------Überprüfe deine Eingabe---------------");
		input = getMeasureRepeats();
	}
	return input;
}
uint8_t getProcessingMode() {
	/*
	 * Zeigt dem Benutzer eine Oberfläche für die Art der Messwertauswertung
	 * und gibt die durchzuführende Messwertauswertung zurück.
	 * Rückgabeparameter:
	 * => Messwertauswertung:
	 * => 1 = Einfache Messung
	 * => 2 = Durchschnitt
	 * => 3 = Median
	 */
	uint8_t input = 0;

	Serial.println("------------------------------------------------------------------------------------");
	Serial.println("Berechnungsmodus: ");
	Serial.println("1: Einfache Messung");
	Serial.println("2: Durchschnitt");
	Serial.println("3: Median");
	input = getInt();
	switch(input) {
	case 1:
		Serial.println("Einfache Messung wurde gewählt");
		break;
	case 2:
		Serial.println("Durchschnitt wurde gewählt");
		break;
	case 3: Serial.println("Median wurde gewählt");
		break;
	default:
		Serial.println ("---------------Überprüfe deine Eingabe---------------");
		input = getProcessingMode();
		break;
	}
	return input;
}

// Sonstige Funktionen
void setParts(){
	pinMode(BUTTON, INPUT);
	pinMode(T_PIN1, INPUT);
	pinMode(T_PIN2, INPUT);
	pinMode(T_PIN3, INPUT);
	pinMode(T_PIN4, INPUT);
	pinMode(M_PIN1, INPUT);
	pinMode(M_PIN2, INPUT);
	analogReference(V_REF);
  #ifdef NANO
	// pinMode(A4, OUTPUT);
	// pinMode(A5, OUTPUT);
  #endif
}
boolean buttonPressed(){
	unsigned long currentTime = millis();
	static unsigned long debounceTime;
	static boolean triggerOnce=false;
	static boolean mindLoop=false;

	if(digitalRead(BUTTON)==LOW&&mindLoop) {
		return false;
	}else{
		mindLoop = false;
	}
	if(digitalRead(BUTTON)==LOW&&!triggerOnce) {
		triggerOnce = true;
		debounceTime = millis();
	}
	if(digitalRead(BUTTON)==LOW&&currentTime-debounceTime>DEBOUNCE) {
		mindLoop = true;
		return true;
	}
	return false;
}
void copyArrays(float *firstArr, uint8_t length1, float *secondArr, uint8_t length2){
	for(uint8_t i=0; i<length1; i++) {
		if(i == length2) {
			Serial.println("[ERROR]: Array Kopiervorgang fehlgeschlagen (Überlauf)");
			break;
		}
		secondArr[i] = firstArr[i];
	}
}
