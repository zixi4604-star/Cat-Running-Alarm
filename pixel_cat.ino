/*
 * Pixel Cat - 会跑的闹钟 work-like prototype
 * Board: Arduino UNO R4 WiFi (onboard 12x8 LED matrix)
 *
 * Serial (115200, one command per line, from the alarm code or from Processing):
 *   ON_TIME  alarm turned off on time         -> affinity +2, "> <" face
 *   LATE     turned off after more than 5 min -> affinity -1, upset face
 *   MISSED   never turned off                 -> affinity -1, upset face
 *   RING     alarm starts ringing             -> wakes the cat up
 *   RESET    affinity back to 0 (debug)
 *   GET      print the current state
 *   FACE <NAME>  (debug) force a face: NORMAL HAPPY CRY SUCCESS UPSET SLEEP.
 *                Affinity is untouched; the face stays until the next real
 *                event (ON_TIME/LATE/MISSED/RING/RESET) or FACE AUTO.
 *   FACE AUTO    (debug) release the forced face
 *
 * The cat replies "STATE,<FACE>,<affinity>" on boot, whenever face or
 * affinity changes, and on GET. FACE is one of:
 *   NORMAL HAPPY CRY SUCCESS UPSET SLEEP
 */
#include "Arduino_LED_Matrix.h"
#include <EEPROM.h>

ArduinoLEDMatrix matrix;

// ---------------- tunables ----------------
#define DEBUG_COMMANDS 1   // set to 0 in the final build to disable FACE
const int AFFINITY_MAX = 30;
const int AFFINITY_MIN = -99;   // safety floor, change if you want
const int GAIN_ON_TIME = 2;
const int LOSS_LATE    = 1;

const unsigned long SUCCESS_MS    = 2000;                // "> <" face duration
const unsigned long UPSET_MS      = 3000;                // upset face duration
const unsigned long IDLE_MS       = 5UL * 60UL * 1000UL; // 5 min -> sleep. Demo: 10000UL
const unsigned long SLEEP_ANIM_MS = 700;                 // ZZZ animation step

// ---------------- sprites (12 cols x 8 rows) ----------------
// '#', 'b', 'h', 'z' = LED on. 'p' (blush) and '.' = LED off.
const char* const FRAME_NORMAL[8] = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#..#..#..#.",
  ".#.p....p.#.",
  "..########.."
};

const char* const FRAME_HAPPY[8] = {
  "...##..##...",
  "....####....",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#..#..#..#.",
  ".#.p....p.#.",
  "..########.."
};

const char* const FRAME_SUCCESS[8] = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#.#....#.#.",
  ".#..#..#..#.",
  ".#.#....#.#.",
  "..########.."
};

const char* const FRAME_UPSET[8] = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#...##...#.",
  ".#..#..#..#.",
  ".#.p.##.p.#.",
  "..########.."
};

const char* const FRAME_CRY[8] = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.b....b.#.",
  "..########.."
};

const char* const FRAME_SLEEP_1[8] = {
  ".##.........",
  "..##........",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

const char* const FRAME_SLEEP_2[8] = {
  ".##..##.....",
  "..##..##....",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

const char* const FRAME_SLEEP_3[8] = {
  ".##..##..##.",
  "..##..##..##",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

typedef const char* const* Sprite;
const Sprite SLEEP_FRAMES[3] = { FRAME_SLEEP_1, FRAME_SLEEP_2, FRAME_SLEEP_3 };

// ---------------- state ----------------
enum Face { F_NORMAL, F_HAPPY, F_CRY, F_SUCCESS, F_UPSET, F_SLEEP };
const char* const FACE_NAMES[] = { "NORMAL", "HAPPY", "CRY", "SUCCESS", "UPSET", "SLEEP" };

int affinity = 0;
bool forced = false;              // debug: a face forced by FACE <NAME>
Face forcedFace = F_NORMAL;
unsigned long lastActivity = 0;   // last time the alarm interacted with the cat
bool tempActive = false;          // a short-lived face (SUCCESS / UPSET) is showing
Face tempFace = F_NORMAL;
unsigned long tempUntil = 0;

int lastDrawKey = -1;
int lastFace = -1;
int lastReportedAffinity = 9999;
bool forceReport = false;

String cmdLine;

// ---------------- persistence ----------------
const int ADDR_MAGIC = 0;
const int ADDR_AFFINITY = 1;
const uint8_t MAGIC = 0xA5;

void saveAffinity() {
  uint8_t v = (uint8_t)(int8_t)affinity;
  if (EEPROM.read(ADDR_AFFINITY) != v) EEPROM.write(ADDR_AFFINITY, v);
}

void loadAffinity() {
  if (EEPROM.read(ADDR_MAGIC) == MAGIC) {
    affinity = constrain((int)(int8_t)EEPROM.read(ADDR_AFFINITY), AFFINITY_MIN, AFFINITY_MAX);
  } else {                        // first run
    affinity = 0;
    EEPROM.write(ADDR_MAGIC, MAGIC);
    saveAffinity();
  }
}

// ---------------- drawing ----------------
void drawFrame(Sprite rows) {
  uint32_t f[3] = {0, 0, 0};
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 12; c++) {
      char ch = rows[r][c];
      if (ch == '#' || ch == 'b' || ch == 'h' || ch == 'z') {
        int i = r * 12 + c;                       // row-major, MSB first
        f[i / 32] |= (1UL << (31 - (i % 32)));
      }
    }
  }
  matrix.loadFrame(f);
}

void draw(Face f, int step) {
  switch (f) {
    case F_NORMAL:  drawFrame(FRAME_NORMAL);  break;
    case F_HAPPY:   drawFrame(FRAME_HAPPY);   break;
    case F_CRY:     drawFrame(FRAME_CRY);     break;
    case F_SUCCESS: drawFrame(FRAME_SUCCESS); break;
    case F_UPSET:   drawFrame(FRAME_UPSET);   break;
    case F_SLEEP:   drawFrame(SLEEP_FRAMES[step]); break;
  }
}

// ---------------- logic ----------------
Face baseFace() {
  if (affinity >= AFFINITY_MAX) return F_HAPPY;
  if (affinity < 0) return F_CRY;
  return F_NORMAL;
}

// priority: forced (debug) > short-lived face > sleeping > base face from affinity
Face currentFace() {
  if (forced) return forcedFace;
  if (tempActive) return tempFace;
  if (millis() - lastActivity >= IDLE_MS) return F_SLEEP;
  return baseFace();
}

void touch() { lastActivity = millis(); }

void showTemp(Face f, unsigned long ms) {
  tempActive = true;
  tempFace = f;
  tempUntil = millis() + ms;
}

void setAffinity(int v) {
  affinity = constrain(v, AFFINITY_MIN, AFFINITY_MAX);
  saveAffinity();
}

void report(Face f) {
  Serial.print("STATE,");
  Serial.print(FACE_NAMES[f]);
  Serial.print(",");
  Serial.println(affinity);
  lastFace = f;
  lastReportedAffinity = affinity;
}

#if DEBUG_COMMANDS
void handleFaceCommand(String name) {
  name.trim();
  if (name == "AUTO") {
    forced = false;
    touch();
    forceReport = true;
    return;
  }
  for (int i = 0; i < (int)(sizeof(FACE_NAMES) / sizeof(FACE_NAMES[0])); i++) {
    if (name == FACE_NAMES[i]) {
      forced = true;
      forcedFace = (Face)i;
      tempActive = false;
      forceReport = true;
      return;
    }
  }
}
#endif

void handleCommand(const String& cmd) {
  if (cmd == "ON_TIME") {
    forced = false;
    setAffinity(affinity + GAIN_ON_TIME);
    showTemp(F_SUCCESS, SUCCESS_MS);
    touch();
  } else if (cmd == "LATE" || cmd == "MISSED") {
    forced = false;
    setAffinity(affinity - LOSS_LATE);
    showTemp(F_UPSET, UPSET_MS);
    touch();
  } else if (cmd == "RING") {
    forced = false;
    tempActive = false;
    touch();
  } else if (cmd == "RESET") {
    forced = false;
    setAffinity(0);
    tempActive = false;
    touch();
  } else if (cmd == "GET") {
    forceReport = true;
#if DEBUG_COMMANDS
  } else if (cmd.startsWith("FACE ")) {
    handleFaceCommand(cmd.substring(5));
#endif
  }
}

void pollSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (cmdLine.length() > 0) {
        cmdLine.trim();
        cmdLine.toUpperCase();
        handleCommand(cmdLine);
        cmdLine = "";
      }
    } else if (cmdLine.length() < 32) {
      cmdLine += ch;
    }
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  matrix.begin();
  loadAffinity();
  touch();
  delay(500);
}

void loop() {
  pollSerial();

  unsigned long now = millis();
  if (tempActive && (long)(now - tempUntil) >= 0) tempActive = false;

  Face f = currentFace();
  int step = (f == F_SLEEP) ? (int)((now / SLEEP_ANIM_MS) % 3) : 0;

  int key = (int)f * 10 + step;
  if (key != lastDrawKey) {
    lastDrawKey = key;
    draw(f, step);
  }

  if (forceReport || (int)f != lastFace || affinity != lastReportedAffinity) {
    forceReport = false;
    report(f);
  }
}
