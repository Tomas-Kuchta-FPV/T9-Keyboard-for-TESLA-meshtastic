#include <Wire.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>

// -------- I2C Configuration --------
#define I2C_SLAVE_ADDRESS 0x34  // TCA8418 keyboard address (Meshtastic uses this)
#define I2C_BUFFER_SIZE 16

// -------- Keypad Configuration --------
const uint8_t ROWS[] = {2, 4, 5, 10};
const uint8_t COLS[] = {3, 11, 12};
const uint8_t ROW_COUNT = 4;
const uint8_t COL_COUNT = 3;
const uint8_t NUM_KEYS = ROW_COUNT * COL_COUNT;

// -------- Interrupt Pins (for wakeup) --------
const uint8_t INT_PIN_1 = 6;   // INT0 or INT1 (check your board)
const uint8_t INT_PIN_2 = 7;   // INT0 or INT1 (check your board)

// -------- State Management --------
volatile bool systemWakeUp = false;
volatile uint8_t i2cBuffer[I2C_BUFFER_SIZE];
volatile uint8_t i2cWriteIndex = 0;
volatile bool newKeyEvent = false;

// Key debounce and timing
#define DEBOUNCE_MS 20
#define SCAN_INTERVAL_MS 10

uint32_t lastScanTime = 0;
uint8_t lastKeyState[NUM_KEYS] = {0};
uint8_t currentKeyState[NUM_KEYS] = {0};

// -------- Interrupt Handlers --------

// INT0 or INT1 external interrupt
ISR(INT1_vect) {
  systemWakeUp = true;
}

ISR(INT0_vect) {
  systemWakeUp = true;
}

// I2C events
volatile uint8_t currentRegister = 0;

void onI2CReceive(int byteCount) {
  if (byteCount > 0) {
    currentRegister = Wire.read();  // First byte is register address
    byteCount--;
    
    // Handle any data written after register address
    while (byteCount > 0 && Wire.available()) {
      uint8_t data = Wire.read();
      if (i2cWriteIndex < I2C_BUFFER_SIZE) {
        i2cBuffer[i2cWriteIndex++] = data;
      }
      byteCount--;
    }
  }
}

void onI2CRequest() {
  // TCA8418 detection: register 0x90 should return 0x00
  if (currentRegister == 0x90) {
    Wire.write(0x00);
  } else if (currentRegister == 0x84) {
    // Keyboard event register - return buffered key events
    Wire.write((const uint8_t*)i2cBuffer, i2cWriteIndex);
    i2cWriteIndex = 0;
  } else {
    // Default register response for other register addresses
    Wire.write(0x00);
  }
}

// -------- Setup --------

void setup() {
  // Configure pins
  configureKeypadPins();
  configureInterruptPins();
  
  // Initialize I2C slave
  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(onI2CReceive);
  Wire.onRequest(onI2CRequest);
  
  // Enable external interrupts for wakeup
  EIMSK |= (1 << INT0) | (1 << INT1);  // Enable both INT0 and INT1
  EICRA |= (1 << ISC00) | (1 << ISC10); // Trigger on any change
}

// -------- Main Loop --------

void loop() {
  // Check if system should wake up
  if (!systemWakeUp) {
    enterSleep();
  }
  
  systemWakeUp = false;
  
  // Scan keypad while awake
  uint32_t now = millis();
  if (now - lastScanTime >= SCAN_INTERVAL_MS) {
    scanKeypad();
    lastScanTime = now;
  }
  
  // Brief debounce/activity window, then return to sleep
  if (millis() - lastScanTime > DEBOUNCE_MS) {
    // Ready for next sleep
  }
}

// -------- Keypad Functions --------

void configureKeypadPins() {
  // Rows as outputs (inactive HIGH)
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    pinMode(ROWS[i], OUTPUT);
    digitalWrite(ROWS[i], HIGH);
  }
  
  // Columns as inputs with pullups
  for (uint8_t i = 0; i < COL_COUNT; i++) {
    pinMode(COLS[i], INPUT_PULLUP);
  }
}

void configureInterruptPins() {
  // Set interrupt pins as inputs with pullups
  pinMode(INT_PIN_1, INPUT_PULLUP);
  pinMode(INT_PIN_2, INPUT_PULLUP);
}

void scanKeypad() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    // Set all rows HIGH
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
      digitalWrite(ROWS[i], HIGH);
    }
    
    // Drive current row LOW
    digitalWrite(ROWS[row], LOW);
    delayMicroseconds(10);  // Settle time
    
    // Read columns
    for (uint8_t col = 0; col < COL_COUNT; col++) {
      uint8_t keyIndex = row * COL_COUNT + col;
      uint8_t isPressed = (digitalRead(COLS[col]) == LOW) ? 1 : 0;
      
      currentKeyState[keyIndex] = isPressed;
      
      // Detect state change
      if (currentKeyState[keyIndex] != lastKeyState[keyIndex]) {
        lastKeyState[keyIndex] = currentKeyState[keyIndex];
        queueKeyEvent(keyIndex, isPressed);
      }
    }
  }
  
  // Return all rows to HIGH (idle)
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    digitalWrite(ROWS[i], HIGH);
  }
}

void queueKeyEvent(uint8_t keyIndex, uint8_t pressed) {
  // Queue key event for I2C transmission
  // Format: [keyIndex] [pressed]
  if (i2cWriteIndex + 2 <= I2C_BUFFER_SIZE) {
    i2cBuffer[i2cWriteIndex++] = keyIndex;
    i2cBuffer[i2cWriteIndex++] = pressed ? 0x80 : 0x00;  // High bit indicates press vs release
    newKeyEvent = true;
  }
}

// -------- Sleep Functions --------

void enterSleep() {
  // Power down unused peripherals
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
  power_spi_disable();
  power_usart0_disable();
  power_adc_disable();
  
  // Prepare keypad for sleep (rows LOW to detect key press)
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    digitalWrite(ROWS[i], LOW);
  }
  
  // Enter power-down sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();  // Ensure interrupts are enabled
  sleep_cpu();
  
  // ---- System wakes here ----
  sleep_disable();
  
  // Re-enable peripherals (only if needed)
  power_timer0_enable();
  power_usart0_enable();
  
  // Restore keypad to active state
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    digitalWrite(ROWS[i], HIGH);
  }
}
