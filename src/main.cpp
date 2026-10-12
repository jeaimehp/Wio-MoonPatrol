#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Every color in reference/atari-2600.png. The playfield is drawn into an
// indexed framebuffer with these entries, then pushed to the panel.
enum : uint8_t {
  C_SKY, C_HILL, C_GROUND, C_BUGGY, C_ROCK, C_UFO, C_HUD, C_YELLOW,
  C_MINT, C_VIOLET, C_LEAF, C_PINK, C_LAVENDER, C_STEEL, C_MOSS, C_SHOT,
  C_WHITE, C_COUNT
};
static const uint16_t PALETTE[C_COUNT] = {
    rgb(0, 0, 0),       rgb(48, 32, 152),   rgb(108, 152, 80),
    rgb(208, 112, 112), rgb(132, 104, 48),  rgb(208, 132, 192),
    rgb(184, 156, 88),  rgb(252, 252, 104), rgb(140, 208, 140),
    rgb(124, 112, 208), rgb(156, 204, 124), rgb(252, 180, 180),
    rgb(188, 180, 252), rgb(104, 136, 204), rgb(92, 156, 92),
    rgb(208, 180, 108), rgb(236, 236, 236)};

static const int W = 320;
static const int GROUND_TOP = 160;
static const int HUD_TOP = 177;
static const int PLAY_H = HUD_TOP;
// Rows that change while driving: hill crest (118 at its highest) to ground.
static const int BAND_TOP = 112;

static uint8_t fb[PLAY_H][W];

static void fbRect(int x, int y, int w, int h, uint8_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > W) w = W - x;
  if (y + h > PLAY_H) h = PLAY_H - y;
  if (w <= 0) return;
  for (int j = 0; j < h; ++j) memset(&fb[y + j][x], c, w);
}

// Sprites are cut from reference/atari-2600.png or traced from the 1080p
// gameplay video. Each row is a bitmask with the most significant used bit
// on the left; one bit is a block of `blockW` panel pixels by 1 row, the
// 2600's wide sprite pixel.
template <typename T>
static void drawBits(int x, int y, const T *rows, uint8_t h, uint8_t cols,
                     uint8_t blockW, uint8_t color) {
  for (uint8_t row = 0; row < h; ++row) {
    for (uint8_t col = 0; col < cols; ++col) {
      if (rows[row] & (1u << (cols - 1 - col))) {
        fbRect(x + col * blockW, y + row, blockW, 1, color);
      }
    }
  }
}

// Stars, 2x1, in their reference colors.
struct Star { uint16_t x; uint8_t y; uint8_t color; };
static const Star STARS[] = {
    {228, 37, C_HUD},     {260, 39, C_YELLOW},   {20, 44, C_MINT},
    {36, 45, C_HUD},      {116, 50, C_VIOLET},   {148, 52, C_LEAF},
    {276, 60, C_PINK},    {68, 67, C_YELLOW},    {148, 72, C_HUD},
    {228, 77, C_VIOLET},  {308, 82, C_LAVENDER}, {84, 88, C_PINK},
    {116, 90, C_STEEL},   {292, 101, C_YELLOW},  {36, 105, C_VIOLET},
    {84, 108, C_MOSS},    {164, 113, C_LEAF}};

// Crest of the blue hill, one sample per 8 pixels. The reference repeats
// every 160 pixels.
static const uint8_t HILL_CREST[] = {
    118, 118, 120, 120, 122, 120, 122, 124, 126, 128,
    130, 128, 128, 126, 128, 126, 124, 124, 122, 120};
static const int HILL_PERIOD = sizeof(HILL_CREST) * 8;

// Magenta attacker, 16x8, 2-pixel blocks.
static const uint8_t ATTACKER_H = 8;
static const uint8_t ATTACKER_SPRITE[ATTACKER_H] = {
    0b00100100, 0b00011000, 0b00111100, 0b00111100,
    0b01111110, 0b11100111, 0b11000011, 0b01000010};

// The 2600 buggy, 28x9, 4-pixel blocks, facing right. Run A is the
// reference frame; run B and the jump frame are traced from the video.
enum BuggyFrame : uint8_t { BUGGY_RUN_A, BUGGY_RUN_B, BUGGY_JUMP };
static const uint8_t BUGGY_W = 28;
static const uint8_t BUGGY_H = 9;
static const uint8_t BUGGY_SPRITE[][BUGGY_H] = {
    // Wheels under columns 1 and 4, as in the reference frame.
    {0b0010000, 0b0111000, 0b1010100, 0b1110010, 0b1111111,
     0b0111110, 0b0000000, 0b0100100, 0b0100100},
    // Wheels one column back, the second rolling frame in the video.
    {0b0010000, 0b0111000, 0b1010100, 0b1110010, 0b1111111,
     0b0111110, 0b0000000, 0b0010010, 0b0010010},
    // In the air the wheels tuck up against the body.
    {0b0010000, 0b0111000, 0b1010100, 0b1110010, 0b1111111,
     0b0111110, 0b0010010, 0b0010010, 0b0000000},
};

// The buggy after it drops into a hole: loose 4-pixel blocks over its
// footprint, flashing salmon and yellow. 32x11, traced from the video.
static const uint8_t DEBRIS_H = 11;
static const uint8_t DEBRIS_SPRITE[DEBRIS_H] = {
    0b00100000, 0b00010100, 0b00010100, 0b01010100, 0b00100000, 0b01011010,
    0b00110101, 0b10000000, 0b01001010, 0b00010000, 0b00100100};

// Holes are notches in the ground that show the hill's blue, 10 rows deep,
// 2-pixel blocks. The small one is traced from the video; the large one is
// the same cup, widened.
static const uint8_t HOLE_H = 10;
static const uint8_t HOLE_SMALL_W = 16;
static const uint8_t HOLE_LARGE_W = 24;
static const uint16_t HOLE_SMALL[HOLE_H] = {
    0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b01111111,
    0b01111110, 0b00111110, 0b00111100, 0b00111000, 0b00010000};
static const uint16_t HOLE_LARGE[HOLE_H] = {
    0b111111111111, 0b111111111111, 0b111111111111, 0b111111111111,
    0b011111111111, 0b011111111110, 0b001111111110, 0b000111111100,
    0b000011111000, 0b000001110000};

// The one-shot rock is the reference rock, 12x6. The two-shot boulder is
// the same shape grown to 16x8. Both use 2-pixel blocks.
static const uint8_t ROCK_SMALL_W = 12, ROCK_SMALL_H = 6;
static const uint8_t ROCK_SMALL[ROCK_SMALL_H] = {
    0b001000, 0b001100, 0b011110, 0b011110, 0b111111, 0b111111};
static const uint8_t ROCK_BIG_W = 16, ROCK_BIG_H = 8;
static const uint8_t ROCK_BIG[ROCK_BIG_H] = {
    0b00011000, 0b00011100, 0b00111110, 0b00111110,
    0b01111111, 0b01111111, 0b11111111, 0b11111111};

// An attacker hit by an air shot breaks into a checkerboard, as in the
// video, before it disappears.
static const uint8_t BURST_H = 8;
static const uint8_t BURST_SPRITE[2][BURST_H] = {
    {0b10101010, 0b01010101, 0b10101010, 0b01010101,
     0b10101010, 0b01010101, 0b10101010, 0b01010101},
    {0b01010101, 0b10101010, 0b01010101, 0b10101010,
     0b01010101, 0b10101010, 0b01010101, 0b10101010}};

// ---- Sound ------------------------------------------------------------
// Cues measured from the 2600 gameplay video (see pickupandgo.md). Driving
// is silent there, so there is no drive cue. Autoplay starts muted, a new
// game unmutes, and the top middle button toggles sound at any time.

enum CueKind : uint8_t { CUE_SWEEP, CUE_NOISE };
struct Cue { CueKind kind; uint16_t f0, f1, ms; uint8_t priority; };
static const Cue CUE_SHOT = {CUE_SWEEP, 1200, 2000, 120, 1};
static const Cue CUE_JUMP = {CUE_SWEEP, 660, 510, 300, 2};
static const Cue CUE_WAVE_DOWN = {CUE_SWEEP, 980, 690, 225, 1};
static const Cue CUE_WAVE_UP = {CUE_SWEEP, 690, 980, 225, 1};
static const Cue CUE_BOMB = {CUE_SWEEP, 870, 625, 200, 1};
static const Cue CUE_THUD = {CUE_NOISE, 80, 110, 300, 2};
static const Cue CUE_CRASH = {CUE_NOISE, 75, 230, 900, 3};

static bool soundOn = false;
static bool toneStarted = false;
static const Cue *cue = nullptr;
static uint32_t cueStart = 0;
static uint32_t cueStep = 0;

static void stopSound() {
  // The core's noTone() waits on the tone timer, whose clock is only
  // enabled by the first tone(). Calling it earlier hangs the board.
  if (toneStarted) noTone(WIO_BUZZER);
  cue = nullptr;
}

static void startTone(uint32_t f) {
  tone(WIO_BUZZER, f);
  toneStarted = true;
}

static void setSound(bool on) {
  if (on == soundOn) return;
  soundOn = on;
  if (!on) stopSound();
  Serial.println(on ? "SOUND ON" : "SOUND OFF");
}

static void playCue(const Cue &c) {
  if (!soundOn) return;
  if (cue && cue->priority > c.priority) return;
  cue = &c;
  cueStart = millis();
  cueStep = 0;
}

static void soundUpdate() {
  if (!cue) return;
  uint32_t t = millis() - cueStart;
  if (t >= cue->ms) {
    // The attacker swoop is two sweeps, down then back up.
    if (cue == &CUE_WAVE_DOWN) {
      cue = &CUE_WAVE_UP;
      cueStart = millis();
      return;
    }
    stopSound();
    return;
  }
  if (cue->kind == CUE_SWEEP) {
    int32_t f = cue->f0 + ((int32_t)cue->f1 - cue->f0) * (int32_t)t / cue->ms;
    startTone(f);
  } else if (t >= cueStep) {
    // Noise: a new random low pitch every 25 ms.
    startTone(random(cue->f0, cue->f1 + 1));
    cueStep = t + 25;
  }
}

// ---- Input -------------------------------------------------------------
// The terminal sits with the power port at the bottom, so Seeed's 5-way
// names are turned a quarter: right is DOWN, left is UP, up is RIGHT.
// The top-left button (KEY_C) changes mode and the top middle button
// (KEY_B) toggles sound. Serial lines starting with I inject the same
// inputs for testing: R and L hold for 10 frames, U jumps, P presses,
// C is the mode button, S the sound button.

static uint8_t injRight = 0, injLeft = 0;
static bool injUp = false, injPress = false, injMode = false, injSound = false;

struct Input { bool right, left, up, press, mode, sound; };

static Input readInput() {
  Input in;
  in.right = digitalRead(WIO_5S_DOWN) == LOW || injRight > 0;
  in.left = digitalRead(WIO_5S_UP) == LOW || injLeft > 0;
  in.up = digitalRead(WIO_5S_RIGHT) == LOW || injUp;
  in.press = digitalRead(WIO_5S_PRESS) == LOW || injPress;
  in.mode = digitalRead(WIO_KEY_C) == LOW || injMode;
  in.sound = digitalRead(WIO_KEY_B) == LOW || injSound;
  if (injRight) --injRight;
  if (injLeft) --injLeft;
  injUp = injPress = injMode = injSound = false;
  return in;
}

static char lineBuf[48];
static uint8_t lineLen = 0;

static void handleLine() {
  lineBuf[lineLen] = 0;
  if (lineBuf[0] == 'I') {
    for (uint8_t i = 1; i < lineLen; ++i) {
      switch (lineBuf[i]) {
        case 'R': injRight = 10; break;
        case 'L': injLeft = 10; break;
        case 'U': injUp = true; break;
        case 'P': injPress = true; break;
        case 'C': injMode = true; break;
        case 'S': injSound = true; break;
      }
    }
  }
  lineLen = 0;
}

static void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen) handleLine();
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// ---- Game --------------------------------------------------------------

static const uint32_t FRAME_MS = 33;
static const float X_MIN = 40, X_MAX = 200, X_CRUISE = 108;
static const float SPEED_MIN = 1.5f, SPEED_MAX = 4.0f;  // pixels per frame
static const float MOVE_STEP = 1.5f;
// The video's jump: about 10 pixels high and 1.4 s from takeoff to landing.
static const int JUMP_FRAMES = 42;
static const float JUMP_HEIGHT = 10;
static const float JUMP_V = 4 * JUMP_HEIGHT / JUMP_FRAMES;
static const float GRAVITY = 8 * JUMP_HEIGHT / (JUMP_FRAMES * JUMP_FRAMES);
static const int CRASH_FRAMES = 45;
static const float COURSE_LEN = 6000;

// Points. Every score change in the 2600 video is +100, including clearing
// a hole; the plan keeps +200 for an attacker.
static const uint16_t POINTS_ROCK = 100;
static const uint16_t POINTS_ATTACKER = 200;
static const uint16_t POINTS_HOLE = 100;

enum Mode : uint8_t { MODE_AUTO, MODE_GAME };

// Holes and rocks share one ground path and scroll with it.
enum Kind : uint8_t { HOLE_S, HOLE_L, ROCK_S, ROCK_L };
struct Obstacle {
  float x;
  Kind kind;
  uint8_t hp;     // rocks: shots left
  uint8_t burst;  // rocks: frames left of the destroyed flash
  bool on;
  bool cleared;   // holes: already scored
};
static const uint8_t OBSTACLE_MAX = 8;
static Obstacle obstacles[OBSTACLE_MAX];

// Attackers fly back and forth across the sky, reversing at the screen
// edges, and bob up and down. A wave enters from the left with the swoop
// cue from the video.
static const uint8_t ATTACKER_MAX = 3;
static const int ATTACKER_W = 16;
static const int SKY_TOP = 30, SKY_BOTTOM = 96;
struct Attacker { float x, vx, baseY, phase; uint8_t burst; bool on; };
static Attacker attackers[ATTACKER_MAX];
static int waveDelay = 0;

// Shots. One ground dash and one air dash at a time, both fired by one
// press. Bombs fall from the attackers.
struct Shot { float x, y; bool on; };
static Shot groundShot, airShot;
static float groundShotStart = 0;
static const float GROUND_SHOT_V = 6, GROUND_SHOT_RANGE = 160;
static const float AIR_SHOT_V = 5;
static const int GROUND_SHOT_W = 8, AIR_SHOT_H = 7;
static const uint8_t BOMB_MAX = 3;
static const int BOMB_H = 6;
static const float BOMB_V = 2;
static Shot bombs[BOMB_MAX];

static Mode mode = MODE_AUTO;
static uint32_t score = 0;
static float buggyX = X_CRUISE;
static float buggyLift = 0;  // pixels above the ground
static float buggyVy = 0;
static bool airborne = false;
static int crashTimer = 0;
static float wheelPhase = 0;
static bool wheelB = false;
static float hillScroll = 0;
static float distance = 0;
static float sinceSpawn = 0;
static float nextGap = 0;
static int autoFireWait = 0;
static uint32_t frameCount = 0;
static Input prevIn = {};

static float speedForX(float x) {
  return SPEED_MIN + (x - X_MIN) * (SPEED_MAX - SPEED_MIN) / (X_MAX - X_MIN);
}

static bool isHole(const Obstacle &o) { return o.kind <= HOLE_L; }

static uint8_t obstacleWidth(const Obstacle &o) {
  switch (o.kind) {
    case HOLE_S: return HOLE_SMALL_W;
    case HOLE_L: return HOLE_LARGE_W;
    case ROCK_S: return ROCK_SMALL_W;
    default: return ROCK_BIG_W;
  }
}

static uint8_t rockHeight(const Obstacle &o) {
  return o.kind == ROCK_S ? ROCK_SMALL_H : ROCK_BIG_H;
}

static int buggyTop() {
  return GROUND_TOP - BUGGY_H - (int)(buggyLift + 0.5f);
}

static void printPosSpeed() {
  Serial.print("POS ");
  Serial.println((int)buggyX);
  // Pixels per second.
  Serial.print("SPEED ");
  Serial.println((int)(speedForX(buggyX) * 1000 / FRAME_MS + 0.5f));
}

static void printScore() {
  Serial.print("SCORE ");
  Serial.println(score);
}

static void addPoints(const char *what, uint16_t points) {
  score += points;
  Serial.print(what);
  Serial.print(' ');
  Serial.println(points);
  printScore();
}

static int attackerY(const Attacker &a) {
  return (int)(a.baseY + 6 * sinf(a.phase));
}

static void launchWave() {
  uint8_t n = random(2, ATTACKER_MAX + 1);
  for (uint8_t i = 0; i < ATTACKER_MAX; ++i) {
    Attacker &a = attackers[i];
    a.on = i < n;
    a.burst = 0;
    a.x = -ATTACKER_W - i * 28;
    a.vx = 1.2f + random(0, 8) / 10.0f;
    a.baseY = random(SKY_TOP + 8, SKY_BOTTOM - 14);
    a.phase = random(0, 628) / 100.0f;
  }
  playCue(CUE_WAVE_DOWN);
}

static void resetField() {
  for (Obstacle &o : obstacles) o.on = false;
  for (Shot &b : bombs) b.on = false;
  groundShot.on = airShot.on = false;
  launchWave();
  waveDelay = 0;
  buggyX = X_CRUISE;
  buggyLift = 0;
  buggyVy = 0;
  airborne = false;
  crashTimer = 0;
  distance = 0;
  sinceSpawn = 0;
  nextGap = 200;
}

static void jump() {
  if (airborne || crashTimer) return;
  airborne = true;
  buggyVy = JUMP_V;
  Serial.println("JUMP");
  playCue(CUE_JUMP);
}

// Like Rover.attack: one press fires a ground shot forward and an air shot
// up, each only if that one is not already in flight.
static void fire() {
  if (crashTimer) return;
  bool fired = false;
  int top = buggyTop();
  if (!groundShot.on) {
    groundShot = {buggyX + BUGGY_W, (float)(top + 4), true};
    groundShotStart = groundShot.x;
    fired = true;
  }
  if (!airShot.on) {
    airShot = {buggyX + 12, (float)(top - AIR_SHOT_H), true};
    fired = true;
  }
  if (fired) {
    Serial.println("SHOT");
    playCue(CUE_SHOT);
  }
}

static void crash() {
  crashTimer = CRASH_FRAMES;
  groundShot.on = airShot.on = false;
  Serial.println("CRASH");
  printScore();
  playCue(CUE_CRASH);
}

static void recover() {
  // Clear the ground around the buggy and any bombs so it can drive on.
  for (Obstacle &o : obstacles) {
    if (o.on && o.x < buggyX + 80 && o.x + obstacleWidth(o) > buggyX - 40) {
      o.on = false;
    }
  }
  for (Shot &b : bombs) b.on = false;
  sinceSpawn = 0;
  nextGap = 160;
  Serial.println("RECOVER");
  printScore();
}

static void addObstacle(float x, Kind kind) {
  for (Obstacle &o : obstacles) {
    if (!o.on) {
      o = {x, kind, (uint8_t)(kind == ROCK_L ? 2 : 1), 0, true, false};
      return;
    }
  }
}

static void spawnObstacles(float step) {
  sinceSpawn += step;
  if (sinceSpawn < nextGap) return;
  addObstacle(W, (Kind)random(4));
  sinceSpawn = 0;
  nextGap = random(150, 300);
}

static bool hitsObstacle() {
  // Holes catch a wheel on the ground; rocks block the body unless the
  // buggy is high enough to clear them.
  const float wheels[2] = {buggyX + 7, buggyX + 19};
  for (const Obstacle &o : obstacles) {
    if (!o.on || o.burst) continue;
    float w = obstacleWidth(o);
    if (isHole(o)) {
      if (airborne) continue;
      for (float wx : wheels) {
        if (wx >= o.x + 2 && wx <= o.x + w - 2) return true;
      }
    } else if (buggyLift < rockHeight(o) - 1 && o.x < buggyX + BUGGY_W - 2 &&
               o.x + w > buggyX + 2) {
      return true;
    }
  }
  return false;
}

static void moveAttackers() {
  for (Attacker &a : attackers) {
    if (!a.on) continue;
    if (a.burst) {
      if (--a.burst == 0) a.on = false;
      continue;
    }
    a.x += a.vx;
    // Reverse at the edges once fully on screen.
    if ((a.vx > 0 && a.x > W - ATTACKER_W) || (a.vx < 0 && a.x < 0)) {
      a.vx = -a.vx;
    }
    a.phase += 0.1f;
    if (a.phase > 6.2832f) a.phase -= 6.2832f;
    // Sometimes drop a bomb.
    if (a.x >= 0 && a.x <= W - ATTACKER_W && random(90) == 0) {
      for (Shot &b : bombs) {
        if (!b.on) {
          b = {a.x + 7, (float)(attackerY(a) + ATTACKER_H), true};
          playCue(CUE_BOMB);
          break;
        }
      }
    }
  }
  bool any = false;
  for (const Attacker &a : attackers) any |= a.on;
  if (!any) {
    // A new wave about three seconds after the last attacker goes.
    if (waveDelay == 0) waveDelay = 90;
    else if (--waveDelay == 0) launchWave();
  }
}

static void moveShots() {
  if (groundShot.on) {
    groundShot.x += GROUND_SHOT_V;
    if (groundShot.x > W || groundShot.x - groundShotStart > GROUND_SHOT_RANGE) {
      groundShot.on = false;
    }
    for (Obstacle &o : obstacles) {
      if (!groundShot.on) break;
      if (!o.on || isHole(o) || o.burst) continue;
      if (groundShot.x + GROUND_SHOT_W >= o.x && groundShot.x <= o.x + obstacleWidth(o) &&
          groundShot.y >= GROUND_TOP - rockHeight(o)) {
        groundShot.on = false;
        if (--o.hp == 0) {
          o.burst = 8;
          addPoints("ROCK", POINTS_ROCK);
          playCue(CUE_THUD);
        } else {
          Serial.println("ROCK HIT");
        }
      }
    }
  }
  if (airShot.on) {
    airShot.y -= AIR_SHOT_V;
    if (airShot.y + AIR_SHOT_H < 0) airShot.on = false;
    for (Attacker &a : attackers) {
      if (!airShot.on) break;
      if (!a.on || a.burst) continue;
      int ay = attackerY(a);
      if (airShot.x + 2 > a.x && airShot.x < a.x + ATTACKER_W &&
          airShot.y < ay + ATTACKER_H && airShot.y + AIR_SHOT_H > ay) {
        airShot.on = false;
        a.burst = 10;
        addPoints("ATTACKER", POINTS_ATTACKER);
        playCue(CUE_THUD);
      }
    }
  }
}

static void moveBombs() {
  for (Shot &b : bombs) {
    if (!b.on) continue;
    b.y += BOMB_V;
    int top = buggyTop();
    if (!crashTimer && b.x + 2 > buggyX + 2 && b.x < buggyX + BUGGY_W - 2 &&
        b.y + BOMB_H > top && b.y < top + BUGGY_H) {
      b.on = false;
      crash();
      continue;
    }
    if (b.y + BOMB_H >= GROUND_TOP) {
      b.on = false;
      // A bomb that lands well ahead of the buggy can open a crater.
      if (b.x > buggyX + BUGGY_W + 30 && random(2)) {
        addObstacle(b.x - HOLE_SMALL_W / 2, HOLE_S);
      }
    }
  }
}

static void scoreClearedHoles() {
  for (Obstacle &o : obstacles) {
    if (o.on && isHole(o) && !o.cleared && o.x + obstacleWidth(o) < buggyX) {
      o.cleared = true;
      addPoints("HOLE", POINTS_HOLE);
    }
  }
}

static void autopilot(float speed) {
  // Ease back to cruise position. Shoot rocks ahead and attackers overhead,
  // and take off so the buggy is at the top of its arc over a hole or a
  // rock that is still standing.
  if (buggyX < X_CRUISE - 0.5f) buggyX += 0.5f;
  else if (buggyX > X_CRUISE + 0.5f) buggyX -= 0.5f;

  if (autoFireWait) --autoFireWait;
  bool wantFire = false;
  float shotX = buggyX + 13;
  for (const Attacker &a : attackers) {
    if (a.on && !a.burst && fabsf(a.x + ATTACKER_W / 2 - shotX) < 10) wantFire = true;
  }
  float reach = speed * JUMP_FRAMES / 2;
  bool wantJump = false;
  for (const Obstacle &o : obstacles) {
    if (!o.on || o.burst) continue;
    float d = o.x + obstacleWidth(o) / 2.0f - (buggyX + BUGGY_W / 2.0f);
    if (d <= 0) continue;
    if (!isHole(o) && d < 140) wantFire = true;
    if (d <= reach) wantJump = true;
  }
  if (wantFire && !autoFireWait) {
    fire();
    autoFireWait = 10;
  }
  if (wantJump && !airborne) jump();
}

static void updateGame(const Input &in) {
  if (in.sound && !prevIn.sound) setSound(!soundOn);
  if (in.mode && !prevIn.mode) {
    if (mode == MODE_AUTO) {
      mode = MODE_GAME;
      score = 0;
      resetField();
      Serial.println("NEW");
      setSound(true);
    } else {
      mode = MODE_AUTO;
      Serial.println("AUTO");
      setSound(false);
    }
    printScore();
  }

  moveAttackers();
  moveBombs();
  for (Obstacle &o : obstacles) {
    if (o.on && o.burst && --o.burst == 0) o.on = false;
  }

  if (crashTimer) {
    if (--crashTimer == 0) recover();
    return;
  }

  if (mode == MODE_GAME) {
    bool moving = in.right != in.left;
    if (in.right && !in.left) buggyX += MOVE_STEP;
    if (in.left && !in.right) buggyX -= MOVE_STEP;
    buggyX = constrain(buggyX, X_MIN, X_MAX);
    bool wasMoving = prevIn.right != prevIn.left;
    if ((moving && frameCount % 6 == 0) || (wasMoving && !moving)) {
      printPosSpeed();
    }
    if (in.up && !prevIn.up) jump();
    if (in.press && !prevIn.press) fire();
  }

  float speed = speedForX(buggyX);
  if (mode == MODE_AUTO) autopilot(speed);

  for (Obstacle &o : obstacles) {
    if (!o.on) continue;
    o.x -= speed;
    if (o.x + obstacleWidth(o) < 0) o.on = false;
  }
  // Ground shots are fired from a moving buggy; keep their range in
  // ground terms.
  groundShotStart -= speed;
  moveShots();
  scoreClearedHoles();

  hillScroll += speed / 2;
  if (hillScroll >= HILL_PERIOD) hillScroll -= HILL_PERIOD;
  distance += speed;
  if (distance >= COURSE_LEN) distance -= COURSE_LEN;
  spawnObstacles(speed);

  wheelPhase += speed;
  if (wheelPhase >= 8) {
    wheelPhase -= 8;
    wheelB = !wheelB;
  }

  if (airborne) {
    buggyVy -= GRAVITY;
    buggyLift += buggyVy;
    if (buggyLift <= 0) {
      buggyLift = 0;
      airborne = false;
    }
  }
  if (hitsObstacle()) crash();
}

// ---- Drawing -----------------------------------------------------------

// Rows covered by things that move in the sky, this frame and last. Only
// these and the driving band are pushed to the panel.
static bool skyRows[PLAY_H], lastSkyRows[PLAY_H];

static void markRows(int y, int h) {
  for (int r = max(y, 0); r < min(y + h, PLAY_H); ++r) skyRows[r] = true;
}

static void renderPlayfield() {
  memcpy(lastSkyRows, skyRows, sizeof(skyRows));
  memset(skyRows, 0, sizeof(skyRows));
  memset(fb, C_SKY, sizeof(fb));
  for (const Star &s : STARS) fbRect(s.x, s.y, 2, 1, s.color);

  for (const Attacker &a : attackers) {
    if (!a.on) continue;
    int ay = attackerY(a);
    if (a.burst) {
      drawBits((int)a.x, ay, BURST_SPRITE[(a.burst / 3) % 2], BURST_H, 8, 2, C_UFO);
    } else {
      drawBits((int)a.x, ay, ATTACKER_SPRITE, ATTACKER_H, 8, 2, C_UFO);
    }
    markRows(ay, ATTACKER_H);
  }

  int scroll = (int)hillScroll;
  for (int x = 0; x < W; ++x) {
    int crest = HILL_CREST[((x + scroll) % HILL_PERIOD) / 8];
    for (int y = crest; y < GROUND_TOP; ++y) fb[y][x] = C_HILL;
  }
  fbRect(0, GROUND_TOP, W, HUD_TOP - GROUND_TOP, C_GROUND);
  for (const Obstacle &o : obstacles) {
    if (!o.on) continue;
    int x = (int)o.x;
    switch (o.kind) {
      case HOLE_S:
        drawBits(x, GROUND_TOP, HOLE_SMALL, HOLE_H, 8, 2, C_HILL);
        break;
      case HOLE_L:
        drawBits(x, GROUND_TOP, HOLE_LARGE, HOLE_H, 12, 2, C_HILL);
        break;
      case ROCK_S:
        drawBits(x, GROUND_TOP - ROCK_SMALL_H, ROCK_SMALL, ROCK_SMALL_H, 6, 2,
                 o.burst ? C_SHOT : C_ROCK);
        break;
      case ROCK_L:
        drawBits(x, GROUND_TOP - ROCK_BIG_H, ROCK_BIG, ROCK_BIG_H, 8, 2,
                 o.burst ? C_SHOT : C_ROCK);
        break;
    }
  }

  int bx = (int)buggyX;
  if (crashTimer) {
    uint8_t color = (crashTimer / 6) % 2 ? C_BUGGY : C_SHOT;
    drawBits(bx, GROUND_TOP - DEBRIS_H, DEBRIS_SPRITE, DEBRIS_H, 8, 4, color);
  } else {
    BuggyFrame frame =
        airborne ? BUGGY_JUMP : (wheelB ? BUGGY_RUN_B : BUGGY_RUN_A);
    drawBits(bx, buggyTop(), BUGGY_SPRITE[frame], BUGGY_H, 7, 4, C_BUGGY);
  }

  if (groundShot.on) fbRect((int)groundShot.x, (int)groundShot.y, GROUND_SHOT_W, 1, C_WHITE);
  if (airShot.on) {
    fbRect((int)airShot.x, (int)airShot.y, 2, AIR_SHOT_H, C_SHOT);
    markRows((int)airShot.y, AIR_SHOT_H);
  }
  for (const Shot &b : bombs) {
    if (!b.on) continue;
    fbRect((int)b.x, (int)b.y, 2, BOMB_H, C_BUGGY);
    markRows((int)b.y, BOMB_H);
  }
}

static void pushRows(int y0, int y1) {
  static uint16_t line[W * 8];
  for (int y = y0; y < y1; y += 8) {
    int n = min(8, y1 - y);
    for (int j = 0; j < n; ++j) {
      const uint8_t *src = fb[y + j];
      uint16_t *dst = &line[j * W];
      for (int x = 0; x < W; ++x) dst[x] = PALETTE[src[x]];
    }
    tft.pushImage(0, y, W, n, line);
    soundUpdate();
  }
}

static void pushChangedRows() {
  int y = 0;
  while (y < PLAY_H) {
    auto changed = [](int r) {
      return r >= BAND_TOP || skyRows[r] || lastSkyRows[r];
    };
    if (!changed(y)) { ++y; continue; }
    int end = y;
    while (end < PLAY_H && changed(end)) ++end;
    pushRows(y, end);
    y = end;
  }
}

// HUD, yellow on black. The reference's lines are only 5-7 pixels tall,
// too small to read on the desk, so the text uses the 8x8 font at 2x.
static const int HUD_SCORE_Y = 181;
static const int HUD_LINE2_Y = 200;
static const int BAR_Y = 222;  // top row of the course bar

// The course bar from the reference: two rails, checkpoint ticks every 16
// pixels, and a 6-pixel marker that walks right as the course is driven.
static void drawCourseBar(int marker) {
  const uint16_t col = PALETTE[C_HUD];
  tft.fillRect(108, BAR_Y, 84, 5, PALETTE[C_SKY]);
  tft.fillRect(108, BAR_Y + 1, 84, 1, col);
  tft.fillRect(108, BAR_Y + 3, 84, 1, col);
  tft.fillRect(108, BAR_Y + 2, 2, 1, col);
  tft.fillRect(190, BAR_Y + 2, 2, 1, col);
  for (int x = 122; x < 186; x += 16) {
    tft.fillRect(x, BAR_Y, 2, 1, col);
    tft.fillRect(x, BAR_Y + 4, 2, 1, col);
  }
  tft.fillRect(marker, BAR_Y, 6, 1, col);
  tft.fillRect(marker, BAR_Y + 4, 6, 1, col);
}

static void drawScore() {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)score);
  tft.setTextColor(PALETTE[C_HUD], PALETTE[C_SKY]);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextPadding(120);
  tft.drawString(buf, W / 2, HUD_SCORE_Y);
  tft.setTextPadding(0);
}

static void drawHud() {
  tft.fillRect(0, HUD_TOP, W, 240 - HUD_TOP, PALETTE[C_SKY]);
  drawScore();
  tft.setTextColor(PALETTE[C_HUD], PALETTE[C_SKY]);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("10-12  TIME 12:47", W / 2, HUD_LINE2_Y);
}

static uint32_t shownScore = UINT32_MAX;
static int shownMarker = -1;

static void updateHud() {
  if (score != shownScore) {
    shownScore = score;
    drawScore();
  }
  int marker = 108 + (int)(distance / COURSE_LEN * (186 - 108));
  if (marker != shownMarker) {
    shownMarker = marker;
    drawCourseBar(marker);
  }
}

// ---- Main --------------------------------------------------------------

static uint32_t lastFrame = 0;
static uint32_t fpsStart = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, HIGH);
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  pinMode(WIO_BUZZER, OUTPUT);
  randomSeed(analogRead(A0) ^ micros());

  tft.begin();
  // Landscape with the power port at the bottom of the desk pose.
  // Rotation 1 put the HUD on the far edge; 3 puts it toward the power port.
  tft.setRotation(3);
  tft.setSwapBytes(true);
  resetField();
  renderPlayfield();
  pushRows(0, PLAY_H);
  drawHud();
  updateHud();

  Serial.println("READY");
  Serial.println("AUTO");
  Serial.println("SOUND OFF");
  printScore();
  lastFrame = fpsStart = millis();
}

void loop() {
  readSerial();
  soundUpdate();
  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = (now - lastFrame > 2 * FRAME_MS) ? now : lastFrame + FRAME_MS;

  Input in = readInput();
  updateGame(in);
  prevIn = in;
  renderPlayfield();
  pushChangedRows();
  updateHud();

  if (++frameCount == 150) {
    Serial.print("FPS ");
    Serial.println(150000 / (millis() - fpsStart));
  }
}
