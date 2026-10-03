/*
 * Pixel Cat viewer / simulator (Processing 4)
 * - Shows the same 12x8 cat as the LED matrix, in colour, plus state and affinity.
 * - Keyboard sends the same Serial commands the real alarm code will send.
 *
 *   1 = ON_TIME   2 = LATE   3 = MISSED   4 = RING   r = RESET   g = GET
 *
 * Demo shortcuts: force a face directly (affinity is not changed; the face stays
 * until the next real event 1-4 / r, or until you press a to release it):
 *   5 = NORMAL  6 = HAPPY  7 = CRY  8 = SUCCESS  9 = UPSET  0 = SLEEP   a = AUTO
 *
 * Set PORT_INDEX to the Arduino's position in the printed port list.
 */
import processing.serial.*;

final int PORT_INDEX = 0;
final int BAUD = 115200;
final int CELL = 40;
final int AFFINITY_MAX = 30;
final int SLEEP_ANIM_MS = 700;

Serial port;
String face = "NORMAL";
int affinity = 0;
String status = "";

color OUTLINE = #7A6A78;
color CREAM   = #F7F3EE;
color PINK    = #F2A6B3;
color HEART   = #EE9AAE;
color TEAR    = #8ECAE6;
color BG      = #FDF8F4;

String[] FRAME_NORMAL = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#..#..#..#.",
  ".#.p....p.#.",
  "..########.."
};

String[] FRAME_HAPPY = {
  "...##..##...",
  "....####....",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#..#..#..#.",
  ".#.p....p.#.",
  "..########.."
};

String[] FRAME_SUCCESS = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#.#....#.#.",
  ".#..#..#..#.",
  ".#.#....#.#.",
  "..########.."
};

String[] FRAME_UPSET = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#...##...#.",
  ".#..#..#..#.",
  ".#.p.##.p.#.",
  "..########.."
};

String[] FRAME_CRY = {
  "............",
  "............",
  "..#......#..",
  ".#.######.#.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.b....b.#.",
  "..########.."
};

String[] FRAME_SLEEP_1 = {
  ".##.........",
  "..##........",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

String[] FRAME_SLEEP_2 = {
  ".##..##.....",
  "..##..##....",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

String[] FRAME_SLEEP_3 = {
  ".##..##..##.",
  "..##..##..##",
  "............",
  ".##########.",
  ".#........#.",
  ".#.##..##.#.",
  ".#.p....p.#.",
  "..########.."
};

String[] HEART_ICON = {
  ".##.##.",
  "#######",
  "#######",
  ".#####.",
  "..###..",
  "...#..."
};

void setup() {
  size(560, 680);
  surface.setTitle("Pixel Cat");
  String[] ports = Serial.list();
  printArray(ports);
  if (ports.length > PORT_INDEX) {
    port = new Serial(this, ports[PORT_INDEX], BAUD);
    port.bufferUntil('\n');
    status = "Connected: " + ports[PORT_INDEX];
  } else {
    status = "No serial port found";
  }
}

String[] frameFor(String name) {
  if (name.equals("HAPPY"))   return FRAME_HAPPY;
  if (name.equals("CRY"))     return FRAME_CRY;
  if (name.equals("SUCCESS")) return FRAME_SUCCESS;
  if (name.equals("UPSET"))   return FRAME_UPSET;
  if (name.equals("SLEEP")) {
    int step = (millis() / SLEEP_ANIM_MS) % 3;
    if (step == 0) return FRAME_SLEEP_1;
    if (step == 1) return FRAME_SLEEP_2;
    return FRAME_SLEEP_3;
  }
  return FRAME_NORMAL;
}

void drawSprite(String[] rows, int x0, int y0, int cell) {
  noStroke();
  for (int r = 0; r < rows.length; r++) {
    String row = rows[r];
    // fill the face interior (rows 3..6 of the 12x8 canvas) with cream
    int first = -1, last = -1;
    for (int c = 0; c < row.length(); c++) {
      if (row.charAt(c) != '.') { if (first < 0) first = c; last = c; }
    }
    for (int c = 0; c < row.length(); c++) {
      char ch = row.charAt(c);
      color col = 0;
      boolean on = true;
      if (ch == '#' || ch == 'z') col = OUTLINE;
      else if (ch == 'p') col = PINK;
      else if (ch == 'h') col = HEART;
      else if (ch == 'b') col = TEAR;
      else if (ch == '.' && r >= 3 && r <= 6 && c > first && c < last) col = CREAM;
      else on = false;
      if (on) {
        fill(col);
        rect(x0 + c * cell, y0 + r * cell, cell, cell);
      }
    }
  }
}

void draw() {
  background(BG);

  // cat
  drawSprite(frameFor(face), 40, 30, CELL);

  // state
  fill(OUTLINE);
  textSize(22);
  textAlign(LEFT, BASELINE);
  text("State: " + face, 40, 400);

  // heart + affinity number
  drawSprite(HEART_ICON, 40, 425, 8);
  textSize(40);
  text(affinity + " / " + AFFINITY_MAX, 115, 470);

  // affinity bar (0..30; negative shows as empty)
  noFill();
  stroke(OUTLINE);
  rect(40, 495, 480, 18);
  noStroke();
  fill(HEART);
  rect(40, 495, 480.0 * constrain(affinity, 0, AFFINITY_MAX) / AFFINITY_MAX, 18);

  // help
  fill(OUTLINE);
  textSize(15);
  text("1 ON_TIME   2 LATE   3 MISSED   4 RING   r RESET   g GET", 40, 555);
  text("5 NORMAL  6 HAPPY  7 CRY  8 SUCCESS  9 UPSET  0 SLEEP  a AUTO", 40, 580);
  text(status, 40, 610);
}

void send(String cmd) {
  if (port != null) port.write(cmd + "\n");
}

void keyPressed() {
  if (key == '1') send("ON_TIME");
  else if (key == '2') send("LATE");
  else if (key == '3') send("MISSED");
  else if (key == '4') send("RING");
  else if (key == 'r') send("RESET");
  else if (key == 'g') send("GET");
  else if (key == '5') send("FACE NORMAL");
  else if (key == '6') send("FACE HAPPY");
  else if (key == '7') send("FACE CRY");
  else if (key == '8') send("FACE SUCCESS");
  else if (key == '9') send("FACE UPSET");
  else if (key == '0') send("FACE SLEEP");
  else if (key == 'a') send("FACE AUTO");
}

void serialEvent(Serial p) {
  String s = p.readStringUntil('\n');
  if (s == null) return;
  String[] parts = split(trim(s), ',');
  if (parts.length == 3 && parts[0].equals("STATE")) {
    face = parts[1];
    affinity = int(parts[2]);
  }
}
