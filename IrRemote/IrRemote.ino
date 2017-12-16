/*
   IRrecord: record and play back IR signals as a minimal
   An IR detector/demodulator must be connected to the input RECV_PIN.
   An IR LED must be connected to the output PWM pin 9(3).
   A button must be connected to the input BUTTON_PIN; this is the
   send button.
   A visible LED can be connected to STATUS_PIN to provide status.

   The logic is:
   If the button is pressed, send the IR code.
   If an IR code is received, record it.

   Version 0.11 September, 2009
   Copyright 2009 Ken Shirriff
   http://arcfn.com
*/

#include <IRremote.h>
/*
   В общем делаете так - находите файл arduino\libraries\IRremote\boarddefs.h, открываете его, находите строки:
   // Arduino Duemilanove, Diecimila, LilyPad, Mini, Fio, Nano, etc
   // ATmega48, ATmega88, ATmega168, ATmega328
	 //#define IR_USE_TIMER1   // tx = pin 9
	 #define IR_USE_TIMER2     // tx = pin 3
   и делаете так:
   // Arduino Duemilanove, Diecimila, LilyPad, Mini, Fio, Nano, etc
   // ATmega48, ATmega88, ATmega168, ATmega328
	 #define IR_USE_TIMER1   // tx = pin 9
	 //#define IR_USE_TIMER2     // tx = pin 3
   После данных манипуляций перезапускаете ArduinoIDE и пробуете, теперь IRremote должна использовать TIMER1
*/
#include <EEPROM.h>
#include "pitches.h"

//#define SERIALOUT Serial // Comment this line to disable Serial output

#define SOUND_PIN  4 // Пин пищалки
#define RECORD_PIN 12 // Пин для записи кнопок

#define MAXBUTTON  10  // Количество кнопок на пульте
int NOTES[MAXBUTTON] = { NOTE_D6, NOTE_C6, NOTE_G5, NOTE_D5, NOTE_B5, NOTE_A5, NOTE_E5, NOTE_F5, NOTE_C5, NOTE_E6 }; // Ноты кнопок

int melody[] = { NOTE_C4, NOTE_G3,NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4 }; // ноты для мелодии
//int noteDurations[] = { 4, 8, 8, 4,4,4,4,4 }; // продолжительность ноты: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = { 8, 16, 16, 8, 8, 8, 8, 8 }; // продолжительность ноты: 4 = quarter note, 8 = eighth note, etc.:

IRrecv irrecv(2);    // Пин, к которому подключен IR модуль
IRsend irsend;


// Storage for the recorded code
typedef struct ir_code {
	int codeType = UNKNOWN; // The type of code
	unsigned long codeValue; // The code value if not raw
	//unsigned int rawCodes[RAWBUF]; // The durations if raw
	int codeLen; // The length of the code
};

ir_code storedCodes[MAXBUTTON];

decode_results results;
int codeType; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

int recordButton = -1;

/*
Ejemplo de utilización de una función de lectura del Teclado analógico
Lee el estado de un teclado compuesto por un divisor de tensión,
cuyas resistencias son puenteadas a masa o a 5 voltios.
El programa mostrará mediante pulsaciones de un led colocado entre el pin
13 y masa, la tecla pulsada.

El circuito:
* Salida de "PaperTecladoAnalógico" (cable azul) conectada a la entrada analógica 0
* Cable rojo de "PaperTecladoAnalógico" a 5v
* Cable negro de "Teclado analógico" a masa. (GND)
* the other side pin to +5V
* Anodo del LED (pata larga) conectada a la salida digital 13
* Cátodo del LED (pata corta) conectada a masa

* Nota: como la mayoría de Aduinos ya incorporan un LED conectado
al pin 13 en la placa, este led es opcional.


Creado por Iñigo Zuluaga
20-Jul-2010

http://txapuzas.blogspot.com

*/

int TecladoPin = 0;     // Selecciona la entrada para el teclado
int Pulsador = 99;      // Variable que indica el pulsador accionado
int LastPulsador = 99;

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(RECORD_PIN, INPUT_PULLUP);
	EEPROM.get(0, storedCodes);
#ifdef SERIALOUT
	SERIALOUT.begin(9600);
	SERIALOUT.print("Send pin: ");
	SERIALOUT.println(SEND_PIN);
#endif // #ifdef SERIALOUT
}

/* Devuelve el valor de la tecla pulsada
Entradas=0
Salidas=Pulsador(1-10,99)
Cuando no se acciona ninguna tecla devuelve 99.
*/
int LeerTeclado() {
	int _tecladoValor = 0;
	int _pulsador = 0;
	_tecladoValor = analogRead(TecladoPin);
	if (_tecladoValor > 823) {
		_pulsador = 4;
	}
	else if (_tecladoValor > 725) {
		_pulsador = 3;
	}
	else if (_tecladoValor > 649) {
		_pulsador = 2;
	}
	else if (_tecladoValor > 586) {
		_pulsador = 1;
	}
	else if (_tecladoValor > 535) {
		_pulsador = 10;
	}
	else if (_tecladoValor > 489) {
		_pulsador = 99;
	}
	else if (_tecladoValor > 438) {
		_pulsador = 5;
	}
	else if (_tecladoValor > 375) {
		_pulsador = 6;
	}
	else if (_tecladoValor > 299) {
		_pulsador = 7;
	}
	else if (_tecladoValor > 201) {
		_pulsador = 8;
	}
	else if (_tecladoValor > 0) {
		_pulsador = 9;
	}
	return _pulsador;
}

// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results) {
	codeType = results->decode_type;
	if (codeType == UNKNOWN) {
#ifdef SERIALOUT
		SERIALOUT.println("Received unknown code, saving as raw");
#endif // #ifdef SERIALOUT
		codeLen = results->rawlen - 1;
		// To store raw codes:
		// Drop first value (gap)
		// Convert from ticks to microseconds
		// Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
		for (int i = 1; i <= codeLen; i++) {
			if (i % 2) {
				// Mark
				rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK - MARK_EXCESS;
#ifdef SERIALOUT
				SERIALOUT.print(" m");
#endif // #ifdef SERIALOUT
			}
			else {
				// Space
				rawCodes[i - 1] = results->rawbuf[i] * USECPERTICK + MARK_EXCESS;
#ifdef SERIALOUT
				SERIALOUT.print(" s");
#endif // #ifdef SERIALOUT
			}
#ifdef SERIALOUT
			SERIALOUT.print(rawCodes[i - 1], DEC);
#endif // #ifdef SERIALOUT
		}
#ifdef SERIALOUT
		SERIALOUT.println("");
#endif // #ifdef SERIALOUT
	}
	else {
		codeValue = results->value;
		if (codeType == NEC) {
#ifdef SERIALOUT
			SERIALOUT.print("Received NEC: ");
#endif // #ifdef SERIALOUT
			if (codeValue == REPEAT) {
				// Don't record a NEC repeat value as that's useless.
#ifdef SERIALOUT
				SERIALOUT.println("repeat; ignoring.");
#endif // #ifdef SERIALOUT
				return;
			}
		}
		else if (codeType == SONY) {
#ifdef SERIALOUT
			SERIALOUT.print("Received SONY: ");
#endif // #ifdef SERIALOUT
		}
		else if (codeType == PANASONIC) {
#ifdef SERIALOUT
			SERIALOUT.print("Received PANASONIC: ");
#endif // #ifdef SERIALOUT
		}
		else if (codeType == JVC) {
#ifdef SERIALOUT
			SERIALOUT.print("Received JVC: ");
#endif // #ifdef SERIALOUT
		}
		else if (codeType == RC5) {
#ifdef SERIALOUT
			SERIALOUT.print("Received RC5: ");
#endif // #ifdef SERIALOUT
		}
		else if (codeType == RC6) {
#ifdef SERIALOUT
			SERIALOUT.print("Received RC6: ");
#endif // #ifdef SERIALOUT
		}
		else {
#ifdef SERIALOUT
			SERIALOUT.print("Unexpected codeType ");
			SERIALOUT.print(codeType, DEC);
			SERIALOUT.println("");
#endif // #ifdef SERIALOUT
			return;
		}
#ifdef SERIALOUT
		SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
		codeLen = results->bits;
	}
}

void sendCode(int repeat) {
	if (codeType == NEC) {
		if (repeat) {
			irsend.sendNEC(REPEAT, codeLen);
#ifdef SERIALOUT
			SERIALOUT.println("Sent NEC repeat");
#endif // #ifdef SERIALOUT
		}
		else {
			irsend.sendNEC(codeValue, codeLen);
#ifdef SERIALOUT
			SERIALOUT.print("Sent NEC ");
			SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
		}
	}
	else if (codeType == SONY) {
		irsend.sendSony(codeValue, codeLen);
#ifdef SERIALOUT
		SERIALOUT.print("Sent Sony ");
		SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
	}
	else if (codeType == PANASONIC) {
		irsend.sendPanasonic(codeValue, codeLen);
#ifdef SERIALOUT
		SERIALOUT.print("Sent Panasonic");
		SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
	}
	else if (codeType == JVC) {
		irsend.sendJVC(codeValue, codeLen, false);
#ifdef SERIALOUT
		SERIALOUT.print("Sent JVC");
		SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
	}
	else if (codeType == RC5 || codeType == RC6) {
		if (!repeat) {
			// Flip the toggle bit for a new button press
			toggle = 1 - toggle;
		}
		// Put the toggle bit into the code to send
		codeValue = codeValue & ~(1 << (codeLen - 1));
		codeValue = codeValue | (toggle << (codeLen - 1));
		if (codeType == RC5) {
			irsend.sendRC5(codeValue, codeLen);
#ifdef SERIALOUT
			SERIALOUT.print("Sent RC5 ");
			SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
		}
		else {
			irsend.sendRC6(codeValue, codeLen);
#ifdef SERIALOUT
			SERIALOUT.print("Sent RC6 ");
			SERIALOUT.println(codeValue, HEX);
#endif // #ifdef SERIALOUT
		}
	}
	else if (codeType == UNKNOWN /* i.e. raw */) {
		// Assume 38 KHz
		irsend.sendRaw(rawCodes, codeLen, 38);
#ifdef SERIALOUT
		SERIALOUT.println("Sent raw");
#endif // #ifdef SERIALOUT
	}
}

void PlayNote(int ind) {
	// to calculate the note duration, take one second
	// divided by the note type.
	//e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
	int noteDuration = 1000 / 16;
	tone(SOUND_PIN, NOTES[ind], noteDuration);

	// to distinguish the notes, set a minimum time between them.
	// the note's duration + 30% seems to work well:
	int pauseBetweenNotes = noteDuration * 1.30;
	delay(pauseBetweenNotes);
	// stop the tone playing:
	noTone(SOUND_PIN);
}

void PlayMelody() {
	// проходим через все ноты мелодии:
	for (int thisNote = 0; thisNote < 8; thisNote++) {
		// to calculate the note duration, take one second 
		// divided by the note type.
		//e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
		int noteDuration = 1000 / noteDurations[thisNote];
		tone(SOUND_PIN, melody[thisNote], noteDuration);

		// to distinguish the notes, set a minimum time between them.
		// the note's duration + 30% seems to work well:
		int pauseBetweenNotes = noteDuration * 1.30;
		delay(pauseBetweenNotes);
		// stop the tone playing:
		noTone(SOUND_PIN);
	}
}

void Beep() {
	// to calculate the note duration, take one second
	// divided by the note type.
	//e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
	int noteDuration = 1000 / 8;
	tone(SOUND_PIN, NOTE_A1, noteDuration);

	// to distinguish the notes, set a minimum time between them.
	// the note's duration + 30% seems to work well:
	int pauseBetweenNotes = noteDuration * 1.30;
	delay(pauseBetweenNotes);
	// stop the tone playing:
	noTone(SOUND_PIN);
	delay(pauseBetweenNotes);

	tone(SOUND_PIN, NOTE_A1, noteDuration);
	delay(pauseBetweenNotes);
	noTone(SOUND_PIN);
}

void loop() {
	// lee el valor analógico del teclado:
	Pulsador = LeerTeclado();          // Va a la función de lectura del teclado

	int recordState = digitalRead(RECORD_PIN);
	if (recordState == LOW) {
		// Recording
		digitalWrite(LED_BUILTIN, HIGH);

		// Si se ha pulsado una tecla ...
		if (LastPulsador != 99 && Pulsador == 99) {
			PlayNote(LastPulsador - 1);
			irrecv.enableIRIn(); // Start the receiver
			recordButton = LastPulsador;
#ifdef SERIALOUT
			SERIALOUT.print("Released ");
			SERIALOUT.print(LastPulsador);
			SERIALOUT.println(", start the reciever");
#endif // #ifdef SERIALOUT
		}
		if (LastPulsador == 99 && Pulsador != 99) {
			recordButton = 99;
#ifdef SERIALOUT
			SERIALOUT.print("Pressed ");
			SERIALOUT.print(Pulsador);
			SERIALOUT.println(", wait for release");
#endif // #ifdef SERIALOUT
		}
		if (irrecv.decode(&results)) {
			irrecv.resume(); // resume receiver
			if (recordButton != 99) {
				storeCode(&results);
				if ((codeType == NEC && codeValue != REPEAT) || codeType == SONY || codeType == PANASONIC || codeType == JVC || codeType == RC5 || codeType == RC6) {
					ir_code* code = &storedCodes[recordButton - 1];
					code->codeType = codeType;
					code->codeValue = codeValue;
					code->codeLen = codeLen;
					EEPROM.put(0, storedCodes);
					PlayMelody();
					recordButton = 99;
#ifdef SERIALOUT
					SERIALOUT.print("Button ");
					SERIALOUT.print(recordButton);
					SERIALOUT.println(" stored");
#endif // #ifdef SERIALOUT
				}
				else {
					Beep();
				}
			}
		}
	}
	else {
		// Playing
		digitalWrite(LED_BUILTIN, LOW);

		if (LastPulsador != 99 && Pulsador == 99) {
#ifdef SERIALOUT
			SERIALOUT.print("Released ");
			SERIALOUT.println(LastPulsador);
#endif // #ifdef SERIALOUT
		}
		else if (LastPulsador == 99 && Pulsador != 99) {
			ir_code code = storedCodes[Pulsador - 1];
			codeType = code.codeType;
			codeValue = code.codeValue;
			codeLen = code.codeLen;
			if (codeType == NEC || codeType == SONY || codeType == PANASONIC || codeType == JVC || codeType == RC5 || codeType == RC6) {
				PlayNote(Pulsador - 1);
				sendCode(0);
			}
			else {
				Beep();
			}
#ifdef SERIALOUT
			SERIALOUT.print("Pressed ");
			SERIALOUT.print(Pulsador);
			SERIALOUT.println(", sending");
#endif // #ifdef SERIALOUT
		}
		else if (Pulsador != 99) {
			ir_code code = storedCodes[Pulsador - 1];
			codeType = code.codeType;
			codeValue = code.codeValue;
			codeLen = code.codeLen;
			if (codeType == NEC || codeType == SONY || codeType == PANASONIC || codeType == JVC || codeType == RC5 || codeType == RC6) {
				sendCode(1);
			}
#ifdef SERIALOUT
			SERIALOUT.print("Pressed ");
			SERIALOUT.print(Pulsador);
			SERIALOUT.println(", repeating");
#endif // #ifdef SERIALOUT
		}
	}
	LastPulsador = Pulsador;
	delay(50); // Wait a bit between retransmissions
}
