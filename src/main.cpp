#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

static const uint16_t COL_SKY = TFT_BLACK;
static const uint16_t COL_HILL = 0x3113;    // 48, 32, 152
static const uint16_t COL_GROUND = 0x6CCA;  // 108, 152, 80
static const uint16_t COL_BUGGY = 0xD38E;   // 208, 112, 112
static const uint16_t COL_ROCK = 0x8346;    // 132, 104, 48
static const uint16_t COL_UFO = 0xD438;     // 208, 132, 192
static const uint16_t COL_HUD = 0xBCEB;     // 184, 156, 88
static const uint16_t COL_STAR = TFT_WHITE;
static const uint16_t COL_SPECK = 0xFFE0;

static const int HILL_BASE = 160;
static const int GROUND_TOP = 160;
static const int GROUND_BOTTOM = 177;
static const int HUD_TOP = 177;

// Crest of the single blue hill, one sample every 8 pixels. Matches the reference.
static const uint8_t HILL_CREST[] = {
    118, 118, 118, 120, 120, 122, 120, 122, 124, 126,
    128, 130, 128, 128, 126, 128, 126, 124, 124, 122,
    120, 118, 118, 120, 120, 122, 120, 122, 124, 126,
    128, 130, 128, 128, 126, 128, 126, 124, 124, 122};

static const uint16_t STARS[][2] = {
    {20, 44},  {36, 37},  {36, 72},  {68, 39},  {84, 60},
    {84, 108}, {116, 90}, {148, 52}, {165, 113}, {203, 80},
    {203, 107}, {229, 50}, {229, 105}, {277, 88}, {293, 101},
    {308, 82}};

static void drawHill() {
  for (int i = 0; i < 40; ++i) {
    int y = HILL_CREST[i];
    tft.fillRect(i * 8, y, 8, HILL_BASE - y, COL_HILL);
  }
}

static void drawStars() {
  for (uint8_t i = 0; i < sizeof(STARS) / sizeof(STARS[0]); ++i) {
    uint16_t color = (i % 5 == 0) ? COL_SPECK : COL_STAR;
    tft.fillRect(STARS[i][0], STARS[i][1], 2, 2, color);
  }
}

// Sprites are cut from reference/atari-2600.png. Each row is a bitmask with
// the most significant used bit on the left; one bit is a block of `blockW`
// panel pixels by 1 row, the 2600's wide sprite pixel.
static void drawBits(int x, int y, const uint8_t *rows, uint8_t h,
                     uint8_t cols, uint8_t blockW, uint16_t color) {
  for (uint8_t row = 0; row < h; ++row) {
    for (uint8_t col = 0; col < cols; ++col) {
      if (rows[row] & (1 << (cols - 1 - col))) {
        tft.fillRect(x + col * blockW, y + row, blockW, 1, color);
      }
    }
  }
}

// Magenta attacker, 16x8, 2-pixel blocks.
static const uint8_t ATTACKER_W = 16;
static const uint8_t ATTACKER_H = 8;
static const uint8_t ATTACKER_SPRITE[ATTACKER_H] = {
    0b00100100, 0b00011000, 0b00111100, 0b00111100,
    0b01111110, 0b11100111, 0b11000011, 0b01000010};

static void drawAttacker(int x, int y) {
  drawBits(x, y, ATTACKER_SPRITE, ATTACKER_H, 8, 2, COL_UFO);
}

// The 2600 buggy, 28x9, 4-pixel blocks, facing right. Run A is the
// reference frame; run B and the jump frame are traced from the 1080p
// gameplay video.
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

// x, y is the top-left of the 28x9 box; the wheels touch y + 8.
static void drawBuggy(int x, int y, BuggyFrame frame) {
  drawBits(x, y, BUGGY_SPRITE[frame], BUGGY_H, 7, 4, COL_BUGGY);
}

// Olive rock, 12x6, 2-pixel blocks. Its base sits on the ground line.
static const uint8_t ROCK_W = 12;
static const uint8_t ROCK_H = 6;
static const uint8_t ROCK_SPRITE[ROCK_H] = {
    0b001000, 0b001100, 0b011110, 0b011110, 0b111111, 0b111111};

static void drawRock(int x, int y) {
  drawBits(x, y, ROCK_SPRITE, ROCK_H, 6, 2, COL_ROCK);
}

static void drawHud() {
  tft.setTextColor(COL_HUD, COL_SKY);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("900", 160, 180);

  tft.setTextDatum(TL_DATUM);
  tft.drawString("10-12  TIME 12:47", 70, 190);

  // Segmented course bar, a gap marking progress.
  for (int x = 108; x < 192; x += 6) {
    if (x == 150) {
      continue;
    }
    tft.fillRect(x, 198, 4, 3, COL_HUD);
  }
}

static void drawScene() {
  tft.fillScreen(COL_SKY);
  drawStars();
  drawAttacker(168, 55);
  drawAttacker(268, 40);
  drawHill();
  tft.fillRect(0, GROUND_TOP, 320, GROUND_BOTTOM - GROUND_TOP, COL_GROUND);
  drawBuggy(108, GROUND_TOP - BUGGY_H, BUGGY_RUN_A);
  drawRock(284, GROUND_TOP - ROCK_H);
  tft.fillRect(0, HUD_TOP, 320, 240 - HUD_TOP, COL_SKY);
  drawHud();
}

void setup() {
  Serial.begin(115200);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, HIGH);

  tft.begin();
  // Landscape with the power port at the bottom of the desk pose.
  // Rotation 1 put the HUD on the far edge; 3 puts it toward the power port.
  tft.setRotation(3);
  drawScene();
  Serial.println("READY");
  Serial.println("SCENE");
}

void loop() {
  delay(2000);
  Serial.println("SCENE");
}
