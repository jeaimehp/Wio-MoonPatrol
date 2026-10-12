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
  C_WHITE, C_GOLD, C_LOGO, C_COUNT
};
// Not const: the title logo's entry (C_LOGO) cycles color.
static uint16_t PALETTE[C_COUNT] = {
    rgb(0, 0, 0),       rgb(48, 32, 152),   rgb(108, 152, 80),
    rgb(208, 112, 112), rgb(132, 104, 48),  rgb(208, 132, 192),
    rgb(184, 156, 88),  rgb(252, 252, 104), rgb(140, 208, 140),
    rgb(124, 112, 208), rgb(156, 204, 124), rgb(252, 180, 180),
    rgb(188, 180, 252), rgb(104, 136, 204), rgb(92, 156, 92),
    rgb(208, 180, 108), rgb(236, 236, 236), rgb(228, 190, 64),
    rgb(75, 200, 76)};

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

// The other two attacker types, from single frames of the video's title
// screen. The saucer is 16x7; the white one is a pair of 6x4 sprites
// drawn 10 pixels apart, the 2600's copied player.
static const uint8_t SAUCER_H = 7;
static const uint8_t SAUCER_SPRITE[SAUCER_H] = {
    0b00011000, 0b00111100, 0b11111111, 0b00011000,
    0b11111111, 0b00111100, 0b01000010};
static const uint8_t TWIN_H = 4;
static const uint8_t TWIN_SPRITE[TWIN_H] = {0b010, 0b101, 0b111, 0b010};

// Title screen, traced from the start of the gameplay video at panel
// scale: the MOON logo with PATROL cut through it, and the copyright.
static const int LOGO_X = 110, LOGO_Y = 139;
static const char *const LOGO[] = {
    "....##........##..............########................########..........####............####",
    "..######....######..........############............############........####............####",
    "..######....######........################........################......######..........####",
    "####################......######....######........######....######......######..........####",
    "####..########..####....######........######....######........######....########........####",
    "####....####....####....####............####....####............####....########........####",
    "####....####....####....####............####....####............####....####..####......####",
    "####....####....############################################################..####......####",
    "####....####....######......####..####......##....######....####..##########....####....####",
    "####....####....######..##..##..##..####..####..##..##..####..##..##########....####....####",
    "####....####....######..##..##..##..####..####..##..##..####..##..##########......####..####",
    "####....####....######......##......####..####....####..####..##..##########......####..####",
    "####....####....######..######..##..####..####..##..##..####..##..##########........########",
    "####....####....######..######..##..####..####..##..##..####..##..##########........########",
    "####....####....############################################################........########",
    "####....####....############################################################..........######",
    "####....####....####....######........######....######........######....####..........######",
    "####....####....####......######....######........######....######......####............####",
    "####....####....####......################........################......####............####",
    "####....####....####........############............############........####............####",
    "####....####....####..........########................########..........####............####"};
static const int COPY_X = 110, COPY_Y = 191;
static const char *const COPYRIGHT[] = {
    "..########",
    "..########",
    "##..####..##..####....######..######..######..........######..######..######..######..##",
    "##..####..##..####....##..##..##..##......##..........##..##....##....##..##..##..##..##",
    "##..##....##....##....######..######..######..........######....##....######..####....##",
    "##..####..##....##........##..##..##......##..........##..##....##....##..##..##..##..##",
    "##........##..######..######..######..######..........##..##....##....##..##..##..##..##",
    "..########"};
// The logo steps through these every half second, as in the video.
static const uint16_t LOGO_COLORS[] = {
    rgb(75, 200, 76),  rgb(242, 142, 252), rgb(231, 190, 64), rgb(245, 49, 50),
    rgb(100, 55, 242), rgb(224, 45, 185),  rgb(252, 252, 252), rgb(49, 105, 231)};

// The lives icon from the reference HUD, 14x5, drawn at 2x.
static const uint16_t LIVES_ICON[5] = {
    0b00111100000000, 0b11111100110000, 0b11111111111111,
    0b11000000000000, 0b00001100110000};

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
// 5-way pins as read on the user's terminal in its desk pose (checked by
// hand, 2026-10-12): right is RIGHT, left is LEFT, up is DOWN, and down
// (UP) is unused.
// The top-left button (KEY_C) changes mode and the top middle button
// (KEY_B) toggles sound. Serial lines starting with I inject the same
// inputs for testing: R and L hold for 10 frames, U jumps, P presses,
// C is the mode button, S the sound button.

static uint8_t injRight = 0, injLeft = 0;
static bool injUp = false, injPress = false, injMode = false, injSound = false;

struct Input { bool right, left, up, press, mode, sound; };

static Input readInput() {
  Input in;
  in.right = digitalRead(WIO_5S_RIGHT) == LOW || injRight > 0;
  in.left = digitalRead(WIO_5S_LEFT) == LOW || injLeft > 0;
  in.up = digitalRead(WIO_5S_DOWN) == LOW || injUp;
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
// One autoplay run is one course, about three minutes at cruise speed.
static const float COURSE_LEN = 12000;
static const uint8_t LIVES_START = 3, LIVES_MAX = 5;
static const uint32_t BONUS_EVERY = 10000;
static const int TITLE_FRAMES = 150;  // 5 s
static const int OVER_FRAMES = 90;    // 3 s
// Attacker types join as the score climbs.
static const uint32_t SAUCER_SCORE = 3000, TWIN_SCORE = 6000;

// Points. Every score change in the 2600 video is +100, including clearing
// a hole; the plan keeps +200 for an attacker.
static const uint16_t POINTS_ROCK = 100;
static const uint16_t POINTS_ATTACKER = 200;
static const uint16_t POINTS_HOLE = 100;

enum Mode : uint8_t { MODE_AUTO, MODE_GAME, MODE_TITLE, MODE_OVER };

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
enum AttackerType : uint8_t { MAGENTA, SAUCER, TWIN };
struct Attacker {
  float x, vx, baseY, phase;
  AttackerType type;
  uint8_t burst;
  bool on;
};
// Per type: height, bob amplitude, bob step, speed range, bomb odds.
struct AttackerKind {
  uint8_t h;
  float amp, step, vmin, vmax;
  uint8_t bombOdds;
};
static const AttackerKind KINDS[] = {
    {ATTACKER_H, 6, 0.10f, 1.2f, 2.0f, 160},
    {SAUCER_H, 10, 0.12f, 2.0f, 2.8f, 110},
    {TWIN_H, 12, 0.25f, 2.5f, 3.5f, 80}};
static Attacker attackers[ATTACKER_MAX];
static int waveDelay = 0;

// Shots. Up to three ground dashes and three air dashes in flight; one
// press fires one of each. Bombs fall from the attackers.
struct Shot { float x, y; bool on; float start; };
static const uint8_t SHOT_MAX = 3;
static Shot groundShots[SHOT_MAX], airShots[SHOT_MAX];
static const float GROUND_SHOT_V = 6, GROUND_SHOT_RANGE = 160;
static const float AIR_SHOT_V = 5;

static void clearShots() {
  for (Shot &g : groundShots) g.on = false;
  for (Shot &a : airShots) a.on = false;
}

static Shot *freeShot(Shot *shots) {
  for (uint8_t i = 0; i < SHOT_MAX; ++i) {
    if (!shots[i].on) return &shots[i];
  }
  return nullptr;
}
static const int GROUND_SHOT_W = 8, AIR_SHOT_H = 7;
static const uint8_t BOMB_MAX = 3;
static const int BOMB_H = 6;
static const float BOMB_V = 2;
static Shot bombs[BOMB_MAX];

static Mode mode = MODE_AUTO;
static Mode titleNext = MODE_AUTO;
static int modeTimer = 0;
static uint8_t lives = 0;
static uint32_t nextBonus = BONUS_EVERY;
static bool fullRedraw = false;
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

static void printLives() {
  Serial.print("LIVES ");
  Serial.println(lives);
}

static void addPoints(const char *what, uint16_t points) {
  score += points;
  Serial.print(what);
  Serial.print(' ');
  Serial.println(points);
  printScore();
  // An extra life at every 10000, up to five.
  while (mode == MODE_GAME && score >= nextBonus) {
    nextBonus += BONUS_EVERY;
    if (lives < LIVES_MAX) {
      ++lives;
      Serial.println("EXTRA LIFE");
      printLives();
    }
  }
}

static int attackerY(const Attacker &a) {
  return (int)(a.baseY + KINDS[a.type].amp * sinf(a.phase));
}

static void launchWave() {
  uint8_t n = random(2, ATTACKER_MAX + 1);
  for (uint8_t i = 0; i < ATTACKER_MAX; ++i) {
    Attacker &a = attackers[i];
    uint8_t level = score >= TWIN_SCORE ? 2 : score >= SAUCER_SCORE ? 1 : 0;
    a.type = (AttackerType)random(0, level + 1);
    const AttackerKind &k = KINDS[a.type];
    a.on = i < n;
    a.burst = 0;
    a.x = -ATTACKER_W - i * 28;
    a.vx = k.vmin + random(0, (long)((k.vmax - k.vmin) * 10) + 1) / 10.0f;
    a.baseY = random(SKY_TOP + 12, SKY_BOTTOM - 12);
    a.phase = random(0, 628) / 100.0f;
  }
  playCue(CUE_WAVE_DOWN);
}

static void resetField() {
  for (Obstacle &o : obstacles) o.on = false;
  for (Shot &b : bombs) b.on = false;
  clearShots();
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
// up, each only if fewer than three of that kind are in flight.
static void fire() {
  if (crashTimer) return;
  bool fired = false;
  int top = buggyTop();
  if (Shot *g = freeShot(groundShots)) {
    float x = buggyX + BUGGY_W;
    *g = {x, (float)(top + 4), true, x};
    fired = true;
  }
  if (Shot *a = freeShot(airShots)) {
    *a = {buggyX + 12, (float)(top - AIR_SHOT_H), true, 0};
    fired = true;
  }
  if (fired) {
    Serial.println("SHOT");
    playCue(CUE_SHOT);
  }
}

static void crash(const char *cause) {
  crashTimer = CRASH_FRAMES;
  clearShots();
  Serial.println("CRASH");
  Serial.print("BY ");
  Serial.println(cause);
  if (mode == MODE_GAME) {
    --lives;
    printLives();
  }
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

static const char *hitsObstacle() {
  // Holes catch a wheel on the ground; rocks block the body unless the
  // buggy is high enough to clear them.
  const float wheels[2] = {buggyX + 7, buggyX + 19};
  for (const Obstacle &o : obstacles) {
    if (!o.on || o.burst) continue;
    float w = obstacleWidth(o);
    if (isHole(o)) {
      if (airborne) continue;
      for (float wx : wheels) {
        if (wx >= o.x + 2 && wx <= o.x + w - 2) return "HOLE";
      }
    } else if (buggyLift < rockHeight(o) - 1 && o.x < buggyX + BUGGY_W - 2 &&
               o.x + w > buggyX + 2) {
      return "ROCK";
    }
  }
  return nullptr;
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
    const AttackerKind &k = KINDS[a.type];
    a.phase += k.step;
    if (a.phase > 6.2832f) a.phase -= 6.2832f;
    // Sometimes drop a bomb.
    if (a.x >= 0 && a.x <= W - ATTACKER_W && random(k.bombOdds) == 0) {
      for (Shot &b : bombs) {
        if (!b.on) {
          b = {a.x + 7, (float)(attackerY(a) + k.h), true};
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
  for (Shot &g : groundShots) {
    if (!g.on) continue;
    g.x += GROUND_SHOT_V;
    if (g.x > W || g.x - g.start > GROUND_SHOT_RANGE) g.on = false;
    for (Obstacle &o : obstacles) {
      if (!g.on) break;
      if (!o.on || isHole(o) || o.burst) continue;
      if (g.x + GROUND_SHOT_W >= o.x && g.x <= o.x + obstacleWidth(o) &&
          g.y >= GROUND_TOP - rockHeight(o)) {
        g.on = false;
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
  for (Shot &s : airShots) {
    if (!s.on) continue;
    s.y -= AIR_SHOT_V;
    if (s.y + AIR_SHOT_H < 0) s.on = false;
    // Air shots also knock out bombs.
    for (Shot &b : bombs) {
      if (s.on && b.on && fabsf(b.x - s.x) < 4 && s.y < b.y + BOMB_H &&
          s.y + AIR_SHOT_H > b.y) {
        s.on = b.on = false;
        Serial.println("BOMB HIT");
      }
    }
    for (Attacker &a : attackers) {
      if (!s.on) break;
      if (!a.on || a.burst) continue;
      int ay = attackerY(a);
      if (s.x + 2 > a.x && s.x < a.x + ATTACKER_W &&
          s.y < ay + KINDS[a.type].h && s.y + AIR_SHOT_H > ay) {
        s.on = false;
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
      crash("BOMB");
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
  // Drive like a player: speed up (move right) on open road, ease back to
  // line up a hole or rock, and hold a middle pace under attackers. Then
  // shoot rocks ahead and attackers overhead, and take off so the buggy is
  // at the top of its arc over a hole or a rock that is still standing.
  float nearest = 1e9f;
  for (const Obstacle &o : obstacles) {
    if (!o.on || o.burst) continue;
    float d = o.x - (buggyX + BUGGY_W);
    if (d > -obstacleWidth(o) && d < nearest) nearest = d;
  }
  bool threat = false;
  for (const Attacker &a : attackers) {
    threat |= a.on && !a.burst && fabsf(a.x - buggyX) < 70;
  }
  for (const Shot &b : bombs) threat |= b.on;
  uint8_t pace = nearest < 120 ? 0 : threat ? 1 : nearest > 220 ? 2 : 1;
  // A bomb coming down on the buggy: shoot at it and get out from under
  // it, braking if it is ahead of the middle and speeding up if behind.
  float mid = buggyX + BUGGY_W / 2.0f;
  bool bombOverhead = false;
  for (const Shot &b : bombs) {
    if (!b.on || fabsf(b.x - mid) > 24) continue;
    bombOverhead = true;
    if (nearest >= 60) pace = b.x > mid ? 0 : 2;
  }
  static const float PACE_X[3] = {X_CRUISE - 30, X_CRUISE + 20, X_MAX - 20};
  static uint8_t lastPace = 255;
  if (pace != lastPace) {
    lastPace = pace;
    printPosSpeed();
  }
  // Hold the pace in the air: changing speed mid-jump lands it short.
  float target = PACE_X[pace];
  if (!airborne) {
    if (buggyX < target - 0.5f) buggyX += 0.5f;
    else if (buggyX > target + 0.5f) buggyX -= 0.5f;
  }

  if (autoFireWait) --autoFireWait;
  bool wantFire = bombOverhead;
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

// Title screen flyers: two of each attacker type bouncing around the sky.
struct Flyer { float x, y, vx, vy; AttackerType type; };
static Flyer flyers[6];

static void startTitle(Mode next) {
  mode = MODE_TITLE;
  titleNext = next;
  modeTimer = TITLE_FRAMES;
  for (uint8_t i = 0; i < 6; ++i) {
    Flyer &f = flyers[i];
    f.type = (AttackerType)(i / 2);
    f.x = random(0, W - ATTACKER_W);
    f.y = random(20, 120);
    f.vx = (random(2) ? 1 : -1) * (1.0f + random(0, 10) / 10.0f);
    f.vy = (random(2) ? 1 : -1) * (0.5f + random(0, 10) / 10.0f);
  }
  stopSound();
  fullRedraw = true;
  Serial.println("TITLE");
}

static void startAuto(bool newRun) {
  mode = MODE_AUTO;
  if (newRun) {
    score = 0;
    resetField();
  }
  fullRedraw = true;
  Serial.println("AUTO");
  setSound(false);
  printScore();
}

static void startGame() {
  mode = MODE_GAME;
  score = 0;
  lives = LIVES_START;
  nextBonus = BONUS_EVERY;
  resetField();
  fullRedraw = true;
  Serial.println("NEW");
  setSound(true);
  printLives();
  printScore();
}

static void startGameOver() {
  mode = MODE_OVER;
  modeTimer = OVER_FRAMES;
  Serial.println("GAME OVER");
  printScore();
  tft.setTextColor(PALETTE[C_HUD], PALETTE[C_SKY]);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(200);
  tft.drawString("GAME OVER", W / 2, 70);
  tft.setTextPadding(0);
}

static void moveFlyers() {
  for (Flyer &f : flyers) {
    f.x += f.vx;
    f.y += f.vy;
    if (f.x < 0 || f.x > W - ATTACKER_W) f.vx = -f.vx;
    if (f.y < 16 || f.y > 125) f.vy = -f.vy;
  }
}

static void updateGame(const Input &in) {
  if (in.sound && !prevIn.sound) setSound(!soundOn);
  bool modePressed = in.mode && !prevIn.mode;

  if (mode == MODE_TITLE) {
    moveFlyers();
    // The top-left button or a press skips to the game.
    bool skip = modePressed || (titleNext == MODE_GAME && in.press && !prevIn.press);
    if (skip || --modeTimer == 0) {
      if (titleNext == MODE_GAME || modePressed) startGame();
      else startAuto(true);
    }
    return;
  }
  if (mode == MODE_OVER) {
    if (--modeTimer == 0) startTitle(MODE_AUTO);
    return;
  }
  if (modePressed) {
    if (mode == MODE_AUTO) {
      startTitle(MODE_GAME);
    } else {
      startAuto(false);
    }
    return;
  }

  moveAttackers();
  moveBombs();
  for (Obstacle &o : obstacles) {
    if (o.on && o.burst && --o.burst == 0) o.on = false;
  }

  if (crashTimer) {
    if (--crashTimer == 0) {
      if (mode == MODE_GAME && lives == 0) startGameOver();
      else recover();
    }
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
  for (Shot &g : groundShots) g.start -= speed;
  moveShots();
  scoreClearedHoles();

  hillScroll += speed / 2;
  if (hillScroll >= HILL_PERIOD) hillScroll -= HILL_PERIOD;
  distance += speed;
  if (distance >= COURSE_LEN) {
    distance -= COURSE_LEN;
    // Autoplay runs one course, then shows the title like an attract loop.
    if (mode == MODE_AUTO) {
      startTitle(MODE_AUTO);
      return;
    }
  }
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
  if (const char *cause = hitsObstacle()) crash(cause);
}

// ---- Drawing -----------------------------------------------------------

// Rows covered by things that move in the sky, this frame and last. Only
// these and the driving band are pushed to the panel.
static bool skyRows[PLAY_H], lastSkyRows[PLAY_H];

static void markRows(int y, int h) {
  for (int r = max(y, 0); r < min(y + h, PLAY_H); ++r) skyRows[r] = true;
}

static void drawAttacker(int x, int y, AttackerType type) {
  switch (type) {
    case MAGENTA:
      drawBits(x, y, ATTACKER_SPRITE, ATTACKER_H, 8, 2, C_UFO);
      break;
    case SAUCER:
      drawBits(x, y, SAUCER_SPRITE, SAUCER_H, 8, 2, C_GOLD);
      break;
    case TWIN:
      drawBits(x, y, TWIN_SPRITE, TWIN_H, 3, 2, C_WHITE);
      drawBits(x + 10, y, TWIN_SPRITE, TWIN_H, 3, 2, C_WHITE);
      break;
  }
}

static void drawArt(int x, int y, const char *const *rows, uint8_t h,
                    uint8_t color) {
  for (uint8_t r = 0; r < h; ++r) {
    for (int c = 0; rows[r][c]; ++c) {
      if (rows[r][c] == '#') fbRect(x + c, y + r, 1, 1, color);
    }
  }
}

static void renderTitle() {
  memcpy(lastSkyRows, skyRows, sizeof(skyRows));
  memset(skyRows, 0, sizeof(skyRows));
  memset(fb, C_SKY, sizeof(fb));
  PALETTE[C_LOGO] = LOGO_COLORS[(modeTimer / 15) % 8];
  for (const Flyer &f : flyers) {
    drawAttacker((int)f.x, (int)f.y, f.type);
    markRows((int)f.y, ATTACKER_H);
  }
  drawArt(LOGO_X, LOGO_Y, LOGO, 21, C_LOGO);
}

// The copyright line sits in the HUD rows, so it is drawn straight to the
// panel when the logo color changes.
static void drawCopyright() {
  tft.fillRect(0, HUD_TOP, W, 240 - HUD_TOP, PALETTE[C_SKY]);
  for (uint8_t r = 0; r < 8; ++r) {
    const char *row = COPYRIGHT[r];
    for (int c = 0; row[c];) {
      if (row[c] != '#') { ++c; continue; }
      int start = c;
      while (row[c] == '#') ++c;
      tft.drawFastHLine(COPY_X + start, COPY_Y + r, c - start, PALETTE[C_LOGO]);
    }
  }
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
      drawBits((int)a.x, ay, BURST_SPRITE[(a.burst / 3) % 2], BURST_H, 8, 2,
               a.type == SAUCER ? C_GOLD : a.type == TWIN ? C_WHITE : C_UFO);
      markRows(ay, BURST_H);
    } else {
      drawAttacker((int)a.x, ay, a.type);
      markRows(ay, KINDS[a.type].h);
    }
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

  for (const Shot &g : groundShots) {
    if (g.on) fbRect((int)g.x, (int)g.y, GROUND_SHOT_W, 1, C_WHITE);
  }
  for (const Shot &a : airShots) {
    if (!a.on) continue;
    fbRect((int)a.x, (int)a.y, 2, AIR_SHOT_H, C_SHOT);
    markRows((int)a.y, AIR_SHOT_H);
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

// Second HUD line. In a game it reads like the 2600's: the lives icon, the
// lives count, and the clock. In autoplay the date takes the lives slot.
static void drawLine2() {
  const uint16_t col = PALETTE[C_HUD];
  tft.fillRect(0, HUD_LINE2_Y, W, 16, PALETTE[C_SKY]);
  tft.setTextColor(col, PALETTE[C_SKY]);
  tft.setTextSize(2);
  if (mode == MODE_GAME) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%u  TIME 12:47", lives);
    int textW = tft.textWidth(buf);
    int x = (W - (28 + 8 + textW)) / 2;
    for (uint8_t r = 0; r < 5; ++r) {
      for (uint8_t c = 0; c < 14; ++c) {
        if (LIVES_ICON[r] & (1u << (13 - c))) {
          tft.fillRect(x + c * 2, HUD_LINE2_Y + 3 + r * 2, 2, 2, col);
        }
      }
    }
    tft.setTextDatum(TL_DATUM);
    tft.drawString(buf, x + 36, HUD_LINE2_Y);
  } else {
    tft.setTextDatum(TC_DATUM);
    tft.drawString("10-12  TIME 12:47", W / 2, HUD_LINE2_Y);
  }
}

static uint32_t shownScore = UINT32_MAX;
static int shownMarker = -1;
static int shownLine2 = -1;

static void drawHud() {
  tft.fillRect(0, HUD_TOP, W, 240 - HUD_TOP, PALETTE[C_SKY]);
  shownScore = UINT32_MAX;
  shownMarker = -1;
  shownLine2 = -1;
}

static void updateHud() {
  int line2 = mode == MODE_GAME ? 10 + lives : 0;
  if (line2 != shownLine2) {
    shownLine2 = line2;
    drawLine2();
  }
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
  if (mode == MODE_OVER) {
    // The scene holds still under GAME OVER.
  } else if (mode == MODE_TITLE) {
    uint16_t before = PALETTE[C_LOGO];
    renderTitle();
    if (fullRedraw) {
      pushRows(0, PLAY_H);
      fullRedraw = false;
      drawCopyright();
    } else {
      pushChangedRows();
      if (PALETTE[C_LOGO] != before) drawCopyright();
    }
  } else {
    renderPlayfield();
    if (fullRedraw) {
      pushRows(0, PLAY_H);
      fullRedraw = false;
      drawHud();
    } else {
      pushChangedRows();
    }
    updateHud();
  }

  if (++frameCount == 150) {
    Serial.print("FPS ");
    Serial.println(150000 / (millis() - fpsStart));
  }
}
