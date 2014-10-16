#include <JeeLib.h>
#include <avr/sleep.h>

typedef struct {
	long lp;
	int bat;
} Payload;
Payload pomiar;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

MilliTimer sendTimer;
void setup () {
    rf12_initialize(12, RF12_433MHZ, 202);
    //pinMode(9, OUTPUT);
    pomiar.lp=0;
    rf12_sleep(RF12_SLEEP);
}
/*
long readVcc2() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
*/

long readVcc() {
   long result;
   // Read 1.1V reference against Vcc
   ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   return result;
}

void loop () {

  bitClear(PRR, PRADC); // power up the ADC
  ADCSRA |= bit(ADEN); // enable the ADC
  Sleepy::loseSomeTime(2);
  pomiar.bat = map(analogRead(3),0,1023,0,6600);
  //pomiar.bat = readVcc();
  ADCSRA &= ~ bit(ADEN); // disable the ADC
  bitSet(PRR, PRADC); // power down the ADC

  //digitalWrite(9, HIGH);
    rf12_sleep(RF12_WAKEUP);
    rf12_recvDone();
    if (rf12_canSend()) {
      rf12_sendStart(0, &pomiar, sizeof pomiar,1);
      pomiar.lp += 1;
      //digitalWrite(9, LOW);
    }
  rf12_sleep(RF12_SLEEP);

for(byte j = 0; j <2; j++) {    // Sleep for j minutes
    Sleepy::loseSomeTime(60000); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
  }

}
