/*Code to convert a 3x4 button matrix to I2C Meshtastic keyboard.
  
*/

// Rows
const int Row_0 = 2;
const int Row_1 = 4;
const int Row_2 = 5;
const int Row_3 = 10;

// Columns
const int Col_0 = 3;
const int Col_1 = 11;
const int Col_2 = 12;

void setup() {
// Init pins
// Rows
pinMode(Row_0, OUTPUT);
pinMode(Row_1, OUTPUT);
pinMode(Row_2, OUTPUT);
pinMode(Row_3, OUTPUT);
// Columns
pinMode(Col_0, INPUT_PULLUP);
pinMode(Col_1, INPUT_PULLUP);
pinMode(Col_2, INPUT_PULLUP);

}

void loop() {

}
