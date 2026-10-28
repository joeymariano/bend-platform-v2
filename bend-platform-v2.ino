// Bend Platform v2
// Adds footswitches to a guitar fuzz/distortion that open & close circuit "bends"
// in a musical way, from a Seeeduino XIAO SAMD21 on the Bend Platform v2 PCB.
//
// 3 buttons:     MODE (cycles the bend behavior) + 2 bend buttons.
// 3 MOSFETs:     Q1 interrupts the pedal's main LED; Q2 & Q3 drive the CD4066 switches
//                that open/close the two circuit bends.
//
// Mode indication is on the pedal's main LED via Q1 (the "4LED rail"): pressing MODE
// blinks it the mode number (1..4) as brief dark pulses against its normally-on state.
//
// Bend modes cycled by MODE:
//   1 momentary       - bend engaged while the button is held
//   2 momentary-trem  - bend stutters while the button is held
//   3 latching        - press toggles the bend on/off
//   4 latching-trem   - press toggles a stuttering bend on/off

// ---------- pins (per manufactured PCB) ----------
const uint8_t PEDAL_LED = 1;   // Q1/INTRPT gate (D1) - pedal main LED, blinked for mode
const uint8_t MODE_BTN  = 9;   // D9

// Bend gate polarity. v2: gate HIGH -> AO3400A pulls the CD4066 control low -> bend OPEN,
// so the bends are active-LOW. Set BOARD_V2 0 for the v1 board (2N7000 directly across the bend).
#define BOARD_V2 1
#if BOARD_V2
const uint8_t BEND_ON = LOW,  BEND_OFF = HIGH;
#else
const uint8_t BEND_ON = HIGH, BEND_OFF = LOW;
#endif

// Q1 drain on the LED anode (resistor side): Q1 on shorts the LED dark, Q1 off lets it
// follow the 3PDT. Rest LOW (LED on), each mode blink pulses HIGH (dark).
const uint8_t LED_IDLE = LOW,  LED_PULSE = HIGH;

// ---------- tunables ----------
const unsigned long DEBOUNCE_MS   = 50;
const unsigned long BLINK_DARK_MS = 223;  // pulse time per mode-blink (the visible pulse)
const unsigned long BLINK_GAP_MS  = 223;  // gap (LED back at rest) between blinks
const unsigned long TREM_MS       = 60;   // momentary-tremolo interval
const unsigned long LATCH_TREM_MS = 100;  // latching-tremolo interval
const int NUM_MODES = 4;

#define LOG 1                             // 1 = print an event log over Serial @ 9600
#define T1_TEST 0                         // 1 = ignore normal op, just toggle T1 1s on/1s off
                                          //     to watch the pedal LED interrupt. Set 0 when done.
const char* MODE_NAMES[] = { "", "momentary", "momentary-trem", "latching", "latching-trem" };

// ---------- types (all structs before any function; Arduino prototypes need them) ----------
struct Button {              // debounced button, shared by all 3 buttons
  uint8_t pin;
  int lastRead;
  unsigned long lastChange;
  bool down;                 // debounced level: true = pressed
};

struct Bend {               // one circuit bend: button + transistor + latch/tremolo state
  Button button;
  uint8_t sw;                // transistor gate pin (OUTPUT)
  bool latched;              // latching modes
  bool tremPhase;            // tremolo square-wave phase
  unsigned long lastTrem;
};

// ---------- state ----------
Bend bends[2] = {
  { { 2, HIGH, 0, false }, 5, false, false, 0 },  // BTN-1 (D2) -> Q3 (D5) -> CD4066 A -> BND-1  bend1
  { { 3, HIGH, 0, false }, 4, false, false, 0 },  // BTN-2 (D3) -> Q2 (D4) -> CD4066 B -> BND-2  bend2
};

Button modeBtn = { MODE_BTN, HIGH, 0, false };
int mode = 1;

// mode indicator: blink the pedal LED (T1) N times = mode number.
// T1 rests at LED_IDLE; each blink is a pulse to LED_PULSE. Non-blocking.
int blinkRemaining = 0;      // pulses still to emit
bool blinkDark = false;      // true while a pulse is being held
unsigned long blinkStamp = 0;

// ---------- shared helpers ----------
// returns true once per debounced press (HIGH->LOW edge); also refreshes b.down
bool pressed(Button &b) {
  int r = digitalRead(b.pin);
  if (r != b.lastRead) { b.lastRead = r; b.lastChange = millis(); }
  bool edge = false;
  if (millis() - b.lastChange > DEBOUNCE_MS) {
    bool nowDown = (r == LOW);           // INPUT_PULLUP: LOW = pressed
    if (nowDown && !b.down) edge = true;
    b.down = nowDown;
  }
  return edge;
}

bool elapsed(unsigned long &since, unsigned long interval) {
  if (millis() - since >= interval) { since = millis(); return true; }
  return false;
}

void setSwitch(Bend &b, bool on) {
  digitalWrite(b.sw, on ? BEND_ON : BEND_OFF);
}

void tremolo(Bend &b, unsigned long interval) {
  if (elapsed(b.lastTrem, interval)) b.tremPhase = !b.tremPhase;
  setSwitch(b, b.tremPhase);
}

// ---------- mode-indicator blink (pedal LED / T1: LED_IDLE = rest, LED_PULSE = pulse) ----------
void startModeBlink(int count) {
  blinkRemaining = count;
  blinkDark = true;
  blinkStamp = millis();
  digitalWrite(PEDAL_LED, LED_PULSE);    // begin first pulse
}

// advance the blink sequence without blocking; leaves the LED back at rest when done
void serviceModeBlink() {
  if (!blinkDark && blinkRemaining == 0) return;   // idle
  if (blinkDark) {
    if (millis() - blinkStamp >= BLINK_DARK_MS) {
      digitalWrite(PEDAL_LED, LED_IDLE);          // LED back to rest -> gap
      blinkDark = false;
      blinkStamp = millis();
      blinkRemaining--;
    }
  } else if (blinkRemaining > 0) {
    if (millis() - blinkStamp >= BLINK_GAP_MS) {
      digitalWrite(PEDAL_LED, LED_PULSE);         // next pulse
      blinkDark = true;
      blinkStamp = millis();
    }
  }
}

// ---------- bend + mode servicing ----------
void serviceBend(Bend &b, int idx) {
  bool justPressed = pressed(b.button);

  switch (mode) {
    case 1: setSwitch(b, b.button.down); break;                                 // momentary
    case 2: b.button.down ? tremolo(b, TREM_MS) : setSwitch(b, false); break;   // momentary-trem
    case 3: if (justPressed) b.latched = !b.latched;                            // latching
            setSwitch(b, b.latched); break;
    case 4: if (justPressed) b.latched = !b.latched;                            // latching-trem
            b.latched ? tremolo(b, LATCH_TREM_MS) : setSwitch(b, false); break;
  }

#if LOG
  if (justPressed) {
    Serial.print("BTN-"); Serial.print(idx + 1);
    Serial.print(" -> bend"); Serial.print(idx + 1);
    if (mode == 3 || mode == 4) Serial.println(b.latched ? " latched ON" : " latched OFF");
    else { Serial.print(' '); Serial.println(MODE_NAMES[mode]); }
  }
#endif
}

void serviceMode() {
  if (pressed(modeBtn)) {
    mode = (mode % NUM_MODES) + 1;   // cycle 1..4
    startModeBlink(mode);            // blink the pedal LED `mode` times
#if LOG
    Serial.print("MODE -> mode "); Serial.print(mode);
    Serial.print(" ("); Serial.print(MODE_NAMES[mode]); Serial.println(")");
#endif
  }
}

// Q1 interrupt test (T1_TEST): toggle the pedal LED gate (D1) slowly so the interrupt is obvious.
void t1BlinkTest() {
  static unsigned long last = 0;
  static bool on = true;
  if (millis() - last >= 1000) {
    last = millis();
    on = !on;
    digitalWrite(PEDAL_LED, on ? LED_IDLE : LED_PULSE);
#if LOG
    Serial.println(on ? "T1 idle (rest)" : "T1 pulse");
#endif
  }
}

// ---------- setup / loop ----------
void setup() {
  // Gates first: no external pull-downs, so they float until configured here.
  // Set the level before switching to OUTPUT so the pin never drives the wrong state.
  for (int i = 0; i < 2; i++) {
    digitalWrite(bends[i].sw, BEND_OFF);
    pinMode(bends[i].sw, OUTPUT);
  }
  digitalWrite(PEDAL_LED, LED_IDLE);   // LED at rest
  pinMode(PEDAL_LED, OUTPUT);

  Serial.begin(9600);

  for (int i = 0; i < 2; i++) pinMode(bends[i].button.pin, INPUT_PULLUP);
  pinMode(modeBtn.pin, INPUT_PULLUP);

#if LOG
  Serial.println("bend-platform-v2 ready");
  Serial.print("mode 1 ("); Serial.print(MODE_NAMES[1]); Serial.println(")");
#endif

  startModeBlink(mode);            // blink current mode once at boot
}

void loop() {
#if T1_TEST
  t1BlinkTest();   // watch the pedal LED interrupt; set T1_TEST 0 for normal operation
  return;
#endif

  serviceBend(bends[0], 0);
  serviceBend(bends[1], 1);
  serviceMode();
  serviceModeBlink();
}
