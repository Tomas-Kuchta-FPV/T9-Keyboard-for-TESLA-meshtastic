#include <avr/sleep.h>
#include <avr/interrupt.h>

// Rows
const uint8_t rows[] = {2, 4, 5, 10};
const uint8_t ROW_COUNT = 4;

// Columns
const uint8_t cols[] = {3, 11, 12};
const uint8_t COL_COUNT = 3;

volatile bool wokeUp = false;

// -------- Interrupts --------

// PORTD: D3
ISR(PCINT2_vect) {
  wokeUp = true;
}

// PORTB: D11, D12
ISR(PCINT0_vect) {
  wokeUp = true;
}

// -------- Setup --------

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Rows: outputs, start HIGH (active mode)
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    pinMode(rows[i], OUTPUT);
    digitalWrite(rows[i], HIGH);
  }

  // Columns: inputs with pullups
  for (uint8_t i = 0; i < COL_COUNT; i++) {
    pinMode(cols[i], INPUT_PULLUP);
  }

  setupPinChangeInterrupts();
}

// -------- Loop --------

void loop() {
  enterSleep();

  if (wokeUp) {
    wokeUp = false;

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100); // debounce / visibility
  }
}

// -------- Sleep handling --------

void enterSleep() {
  // Drive rows LOW so key press pulls column LOW
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    digitalWrite(rows[i], LOW);
  }

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();

  // ---- wakes here ----
  sleep_disable();

  // Restore rows HIGH for active mode
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    digitalWrite(rows[i], HIGH);
  }
}

// -------- PCINT setup --------

void setupPinChangeInterrupts() {
  // Enable pin change interrupts
  PCICR |= (1 << PCIE2) | (1 << PCIE0);

  // Columns:
  // D3  -> PD3 -> PCINT19
  PCMSK2 |= (1 << PCINT19);

  // D11 -> PB3 -> PCINT3
  // D12 -> PB4 -> PCINT4
  PCMSK0 |= (1 << PCINT3) | (1 << PCINT4);
}
