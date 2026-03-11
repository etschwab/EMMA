#include <TFT_eSPI.h>
#include <Keypad.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite gfx = TFT_eSprite(&tft);

#ifndef TFT_BL
  #define TFT_BL 4
#endif

#ifndef TFT_BACKLIGHT_ON
  #define TFT_BACKLIGHT_ON HIGH
#endif

// =====================================================
// SHARED SCREEN / KEYPAD
// =====================================================
constexpr int APP_SCREEN_W = 240;
constexpr int APP_SCREEN_H = 135;

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {21, 27, 26, 22};
byte colPins[COLS] = {33, 32, 25};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// =====================================================
// APP / LAUNCHER
// =====================================================
enum AppMode {
  APP_LAUNCHER,
  APP_SPACE,
  APP_PONG,
  APP_DOOM
};

AppMode appMode = APP_LAUNCHER;
int launcherIndex = 0;
unsigned long launcherMoveLock = 0;

void launcherReadInput(bool &upPressed, bool &downPressed, bool &selectPressed) {
  upPressed = false;
  downPressed = false;
  selectPressed = false;

  keypad.getKeys();

  for (int i = 0; i < LIST_MAX; i++) {
    if (keypad.key[i].kchar == NO_KEY) continue;

    char k = keypad.key[i].kchar;
    KeyState s = keypad.key[i].kstate;

    if (k == '2' && s == PRESSED) upPressed = true;
    if (k == '8' && s == PRESSED) downPressed = true;
    if (k == '#' && s == PRESSED) selectPressed = true;
  }
}

void drawLauncher() {
  gfx.fillSprite(TFT_BLACK);

  gfx.setTextSize(2);
  gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
  gfx.setCursor(54, 10);
  gfx.print("GAME LAUNCHER");

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setCursor(52, 34);
  gfx.print("2/8 = MENU   # = START");

  const char* items[3] = {
    "SPACE INVADERS",
    "NEON PONG",
    "MINI DOOM"
  };

  for (int i = 0; i < 3; i++) {
    int y = 58 + i * 22;

    if (launcherIndex == i) {
      gfx.fillRoundRect(42, y - 4, 156, 18, 4, TFT_DARKGREY);
      gfx.setTextColor(TFT_BLACK, TFT_DARKGREY);
    } else {
      gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    gfx.setCursor(68, y);
    gfx.print(items[i]);
  }

  gfx.setTextColor(TFT_CYAN, TFT_BLACK);
  gfx.setCursor(26, 126);
  gfx.print("* im Spiel = zurueck zum Launcher");

  gfx.pushSprite(0, 0);
}

void updateLauncher() {
  bool upPressed, downPressed, selectPressed;
  launcherReadInput(upPressed, downPressed, selectPressed);

  if (millis() - launcherMoveLock > 150) {
    if (upPressed) {
      launcherIndex--;
      if (launcherIndex < 0) launcherIndex = 2;
      launcherMoveLock = millis();
    }
    if (downPressed) {
      launcherIndex++;
      if (launcherIndex > 2) launcherIndex = 0;
      launcherMoveLock = millis();
    }
  }

  if (selectPressed) {
    if (launcherIndex == 0) {
      extern void SI_enter();
      SI_enter();
      appMode = APP_SPACE;
    }
    else if (launcherIndex == 1) {
      extern void PONG_enter();
      PONG_enter();
      appMode = APP_PONG;
    }
    else if (launcherIndex == 2) {
      extern void DOOM_enter();
      DOOM_enter();
      appMode = APP_DOOM;
    }
  }
}

// =====================================================
// ================= SPACE INVADERS =====================
// =====================================================
enum SI_GameState {
  SI_STATE_MENU,
  SI_STATE_PLAYING,
  SI_STATE_PAUSED
};

SI_GameState SI_gameState = SI_STATE_MENU;

constexpr int SI_SCREEN_W = 240;
constexpr int SI_SCREEN_H = 135;

constexpr unsigned long SI_FRAME_TIME_MS = 62;
unsigned long SI_lastFrameTime = 0;

// colors
constexpr uint16_t SI_COL_BG             = TFT_BLACK;
constexpr uint16_t SI_COL_PLAYER         = TFT_CYAN;
constexpr uint16_t SI_COL_PLAYER_SHIELD  = TFT_WHITE;
constexpr uint16_t SI_COL_SHIELD_RING_1  = TFT_BLUE;
constexpr uint16_t SI_COL_SHIELD_RING_2  = 0x7D7C;
constexpr uint16_t SI_COL_BULLET         = TFT_YELLOW;
constexpr uint16_t SI_COL_TEXT           = TFT_WHITE;
constexpr uint16_t SI_COL_ACCENT         = TFT_GREEN;
constexpr uint16_t SI_COL_WARN           = 0xFD20;
constexpr uint16_t SI_COL_PANEL          = 0x18C3;
constexpr uint16_t SI_COL_STAR           = TFT_WHITE;
constexpr uint16_t SI_COL_POWERUP_TRIPLE = 0xFEA0;
constexpr uint16_t SI_COL_POWERUP_SHIELD = TFT_BLUE;
constexpr uint16_t SI_COL_POWERUP_LIFE   = TFT_GREEN;
constexpr uint16_t SI_COL_BAR_BG         = 0x2104;
constexpr uint16_t SI_COL_BORDER         = TFT_WHITE;
constexpr uint16_t SI_COL_DIM            = 0x7BEF;

constexpr uint16_t SI_ENEMY_COLORS[] = {
  TFT_RED,
  TFT_ORANGE,
  0x8000
};
constexpr int SI_ENEMY_COLOR_COUNT = sizeof(SI_ENEMY_COLORS) / sizeof(SI_ENEMY_COLORS[0]);

// input
bool SI_keyLeftHeld = false;
bool SI_keyRightHeld = false;
bool SI_keyFireHeld = false;
bool SI_keySelectPressed = false;
bool SI_keyBackPressed = false;

// player
int SI_playerX = 112;
constexpr int SI_playerY = 116;
constexpr int SI_playerW = 16;
constexpr int SI_playerH = 12;
constexpr int SI_playerSpeed = 4;

int SI_lives = 5;
constexpr int SI_MAX_LIVES = 9;

int SI_score = 0;
int SI_highScore = 0;

int SI_playerInvFrames = 0;
constexpr int SI_PLAYER_INV_MAX = 18;

// bullets
struct SI_Bullet {
  float x;
  float y;
  float dx;
  float dy;
  bool active;
};

constexpr int SI_MAX_BULLETS = 24;
SI_Bullet SI_bullets[SI_MAX_BULLETS];

constexpr int SI_bulletW = 3;
constexpr int SI_bulletH = 6;
constexpr float SI_bulletSpeedStraight = 8.0f;
constexpr float SI_bulletSpeedDiagX = 2.4f;
constexpr float SI_bulletSpeedDiagY = 6.7f;

constexpr int SI_MAX_AMMO = 10;
int SI_ammo = SI_MAX_AMMO;
int SI_ammoRechargeFrames = 0;
constexpr int SI_AMMO_RECHARGE_TIME = 8;

int SI_fireCooldown = 0;
constexpr int SI_FIRE_COOLDOWN_MAX = 2;

// powerups
enum SI_PowerUpType {
  SI_POWERUP_TRIPLE,
  SI_POWERUP_SHIELD,
  SI_POWERUP_LIFE
};

bool SI_tripleShotActive = false;
int SI_tripleShotFrames = 0;
constexpr int SI_TRIPLE_SHOT_DURATION = 80;

bool SI_shieldActive = false;
int SI_shieldFrames = 0;
constexpr int SI_SHIELD_DURATION = 96;

struct SI_PowerUp {
  int x;
  int y;
  int w;
  int h;
  int speed;
  bool active;
  SI_PowerUpType type;
};

SI_PowerUp SI_powerUp;
int SI_powerUpSpawnTimer = 0;

// enemies
struct SI_Enemy {
  int x;
  int y;
  int w;
  int h;
  int speed;
  bool active;
  uint16_t color;
};

constexpr int SI_MAX_ENEMIES = 6;
SI_Enemy SI_enemies[SI_MAX_ENEMIES];

// starfield
struct SI_Star {
  int x;
  int y;
  int speed;
};

constexpr int SI_STAR_COUNT = 28;
SI_Star SI_stars[SI_STAR_COUNT];

bool SI_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

int SI_randomEnemySpeed() {
  int base = 2 + (SI_score / 10);
  if (base > 5) base = 5;
  return random(base, base + 2);
}

uint16_t SI_randomEnemyColor() {
  return SI_ENEMY_COLORS[random(0, SI_ENEMY_COLOR_COUNT)];
}

void SI_spawnEnemy(int index) {
  SI_enemies[index].w = 14;
  SI_enemies[index].h = 10;
  SI_enemies[index].x = random(6, SI_SCREEN_W - SI_enemies[index].w - 6);
  SI_enemies[index].y = random(-90, -10);
  SI_enemies[index].speed = SI_randomEnemySpeed();
  SI_enemies[index].active = true;
  SI_enemies[index].color = SI_randomEnemyColor();
}

void SI_initEnemies() {
  for (int i = 0; i < SI_MAX_ENEMIES; i++) {
    SI_spawnEnemy(i);
    SI_enemies[i].y -= i * 24;
  }
}

void SI_initBullets() {
  for (int i = 0; i < SI_MAX_BULLETS; i++) {
    SI_bullets[i].active = false;
    SI_bullets[i].x = 0;
    SI_bullets[i].y = 0;
    SI_bullets[i].dx = 0;
    SI_bullets[i].dy = 0;
  }
}

void SI_initStars() {
  for (int i = 0; i < SI_STAR_COUNT; i++) {
    SI_stars[i].x = random(0, SI_SCREEN_W);
    SI_stars[i].y = random(0, SI_SCREEN_H);
    SI_stars[i].speed = random(1, 4);
  }
}

void SI_resetPowerUp() {
  SI_powerUp.active = false;
  SI_powerUp.x = 0;
  SI_powerUp.y = 0;
  SI_powerUp.w = 12;
  SI_powerUp.h = 12;
  SI_powerUp.speed = 2;
  SI_powerUp.type = SI_POWERUP_TRIPLE;
  SI_powerUpSpawnTimer = random(90, 180);
}

void SI_spawnPowerUp() {
  SI_powerUp.active = true;
  SI_powerUp.w = 12;
  SI_powerUp.h = 12;
  SI_powerUp.x = random(8, SI_SCREEN_W - SI_powerUp.w - 8);
  SI_powerUp.y = -12;
  SI_powerUp.speed = 2;

  int r = random(0, 100);
  if (r < 40) SI_powerUp.type = SI_POWERUP_TRIPLE;
  else if (r < 75) SI_powerUp.type = SI_POWERUP_SHIELD;
  else SI_powerUp.type = SI_POWERUP_LIFE;
}

void SI_resetGame() {
  SI_playerX = (SI_SCREEN_W - SI_playerW) / 2;
  SI_lives = 5;
  SI_score = 0;
  SI_playerInvFrames = 0;

  SI_ammo = SI_MAX_AMMO;
  SI_ammoRechargeFrames = 0;
  SI_fireCooldown = 0;

  SI_tripleShotActive = false;
  SI_tripleShotFrames = 0;

  SI_shieldActive = false;
  SI_shieldFrames = 0;

  SI_initBullets();
  SI_initEnemies();
  SI_resetPowerUp();
}

bool SI_spawnBullet(float x, float y, float dx, float dy) {
  for (int i = 0; i < SI_MAX_BULLETS; i++) {
    if (!SI_bullets[i].active) {
      SI_bullets[i].active = true;
      SI_bullets[i].x = x;
      SI_bullets[i].y = y;
      SI_bullets[i].dx = dx;
      SI_bullets[i].dy = dy;
      return true;
    }
  }
  return false;
}

void SI_resetInputFlags() {
  SI_keyLeftHeld = false;
  SI_keyRightHeld = false;
  SI_keyFireHeld = false;
  SI_keySelectPressed = false;
  SI_keyBackPressed = false;
}

void SI_readInput() {
  SI_resetInputFlags();

  keypad.getKeys();

  for (int i = 0; i < LIST_MAX; i++) {
    char k = keypad.key[i].kchar;
    KeyState st = keypad.key[i].kstate;
    bool changed = keypad.key[i].stateChanged;
    bool held = (st == PRESSED || st == HOLD);

    if (k == '4' && held) SI_keyLeftHeld = true;
    if (k == '6' && held) SI_keyRightHeld = true;
    if ((k == '2' || k == '5') && held) SI_keyFireHeld = true;

    if ((k == '#' || k == '5') && changed && st == PRESSED) SI_keySelectPressed = true;
    if (k == '*' && changed && st == PRESSED) SI_keyBackPressed = true;
  }
}

void SI_drawCentered(const char* txt, int y, uint16_t col, int font = 2) {
  gfx.setTextColor(col, SI_COL_BG);
  gfx.drawCentreString(txt, SI_SCREEN_W / 2, y, font);
}

void SI_drawBar(int x, int y, int w, int h, int value, int maxValue, uint16_t fillCol) {
  if (maxValue <= 0) return;
  if (value < 0) value = 0;
  if (value > maxValue) value = maxValue;

  gfx.drawRect(x, y, w, h, SI_COL_BORDER);
  gfx.fillRect(x + 1, y + 1, w - 2, h - 2, SI_COL_BAR_BG);

  int innerW = w - 2;
  int fillW = (value * innerW) / maxValue;
  if (fillW > 0) gfx.fillRect(x + 1, y + 1, fillW, h - 2, fillCol);
}

void SI_drawPanelBox(int x, int y, int w, int h) {
  gfx.fillRect(x, y, w, h, SI_COL_BG);
  gfx.drawRect(x, y, w, h, SI_COL_BORDER);
}

void SI_drawStars() {
  for (int i = 0; i < SI_STAR_COUNT; i++) {
    uint16_t col = (SI_stars[i].speed >= 3) ? TFT_WHITE : SI_COL_DIM;
    gfx.drawPixel(SI_stars[i].x, SI_stars[i].y, col);
    if (SI_stars[i].speed >= 3 && SI_stars[i].y + 1 < SI_SCREEN_H) gfx.drawPixel(SI_stars[i].x, SI_stars[i].y + 1, col);
  }
}

void SI_drawPlayer() {
  if (SI_playerInvFrames > 0 && ((SI_playerInvFrames / 2) % 2 == 0)) return;

  uint16_t shipColor = SI_shieldActive ? SI_COL_PLAYER_SHIELD : SI_COL_PLAYER;

  if (SI_shieldActive) {
    gfx.drawCircle(SI_playerX + 8, SI_playerY + 7, 11, SI_COL_SHIELD_RING_1);
    gfx.drawCircle(SI_playerX + 8, SI_playerY + 7, 12, SI_COL_SHIELD_RING_2);
  }

  gfx.fillRect(SI_playerX + 6, SI_playerY, 4, 3, shipColor);
  gfx.fillRect(SI_playerX + 5, SI_playerY + 3, 6, 2, shipColor);
  gfx.fillRect(SI_playerX + 3, SI_playerY + 5, 10, 2, shipColor);
  gfx.fillRect(SI_playerX, SI_playerY + 7, 16, 4, shipColor);

  gfx.drawRect(SI_playerX, SI_playerY + 7, 16, 4, SI_COL_BORDER);
  gfx.drawPixel(SI_playerX + 2, SI_playerY + 8, SI_COL_BORDER);
  gfx.drawPixel(SI_playerX + 13, SI_playerY + 8, SI_COL_BORDER);

  gfx.drawPixel(SI_playerX + 4, SI_playerY + 11, TFT_ORANGE);
  gfx.drawPixel(SI_playerX + 11, SI_playerY + 11, TFT_ORANGE);
  gfx.drawPixel(SI_playerX + 7, SI_playerY + 11, TFT_YELLOW);
  gfx.drawPixel(SI_playerX + 8, SI_playerY + 11, TFT_YELLOW);
}

void SI_drawBullets() {
  for (int i = 0; i < SI_MAX_BULLETS; i++) {
    if (!SI_bullets[i].active) continue;

    int bx = (int)SI_bullets[i].x;
    int by = (int)SI_bullets[i].y;

    gfx.fillRect(bx, by, SI_bulletW, SI_bulletH, SI_COL_BULLET);
    if (by > 0) gfx.drawPixel(bx + 1, by - 1, TFT_WHITE);
  }
}

void SI_drawEnemies() {
  for (int i = 0; i < SI_MAX_ENEMIES; i++) {
    if (!SI_enemies[i].active) continue;

    int x = SI_enemies[i].x;
    int y = SI_enemies[i].y;
    int w = SI_enemies[i].w;
    int h = SI_enemies[i].h;
    uint16_t c = SI_enemies[i].color;

    gfx.fillRect(x, y, w, h, c);
    gfx.drawRect(x, y, w, h, SI_COL_BORDER);
    gfx.fillRect(x + 2, y + 2, 2, 2, SI_COL_BORDER);
    gfx.fillRect(x + w - 4, y + 2, 2, 2, SI_COL_BORDER);
    gfx.drawFastHLine(x + 3, y + h - 3, w - 6, TFT_BLACK);

    gfx.drawPixel(x - 1, y + 4, c);
    gfx.drawPixel(x + w, y + 4, c);
  }
}

void SI_drawPowerUpGlow(int cx, int cy, int r, uint16_t glowColor) {
  gfx.drawCircle(cx, cy, r + 2, glowColor);
  gfx.drawCircle(cx, cy, r + 3, glowColor);
}

void SI_drawPowerUp() {
  if (!SI_powerUp.active) return;

  uint16_t c = SI_COL_POWERUP_TRIPLE;
  if (SI_powerUp.type == SI_POWERUP_SHIELD) c = SI_COL_POWERUP_SHIELD;
  if (SI_powerUp.type == SI_POWERUP_LIFE)   c = SI_COL_POWERUP_LIFE;

  int cx = SI_powerUp.x + SI_powerUp.w / 2;
  int cy = SI_powerUp.y + SI_powerUp.h / 2;
  int r = 5;

  SI_drawPowerUpGlow(cx, cy, r, c);
  gfx.fillCircle(cx, cy, r, c);
  gfx.drawCircle(cx, cy, r, SI_COL_BORDER);
  gfx.drawCircle(cx, cy, r + 1, SI_COL_BORDER);

  gfx.drawPixel(cx - 2, cy - 2, TFT_WHITE);
  gfx.drawPixel(cx - 1, cy - 2, TFT_WHITE);
  gfx.drawPixel(cx - 2, cy - 1, TFT_WHITE);

  if (SI_powerUp.type == SI_POWERUP_TRIPLE) {
    gfx.drawFastVLine(cx, cy - 2, 5, SI_COL_BORDER);
    gfx.drawFastVLine(cx - 2, cy, 3, SI_COL_BORDER);
    gfx.drawFastVLine(cx + 2, cy, 3, SI_COL_BORDER);
  } else if (SI_powerUp.type == SI_POWERUP_SHIELD) {
    gfx.drawCircle(cx, cy, 2, SI_COL_BORDER);
    gfx.drawPixel(cx, cy, SI_COL_BORDER);
  } else if (SI_powerUp.type == SI_POWERUP_LIFE) {
    gfx.drawFastVLine(cx, cy - 2, 5, SI_COL_BORDER);
    gfx.drawFastHLine(cx - 2, cy, 5, SI_COL_BORDER);
  }
}

void SI_drawHUD() {
  gfx.fillRect(0, 0, SI_SCREEN_W, 18, SI_COL_PANEL);
  gfx.drawFastHLine(0, 18, SI_SCREEN_W, SI_COL_BORDER);

  gfx.setTextColor(SI_COL_TEXT, SI_COL_PANEL);

  gfx.setCursor(4, 4);   gfx.printf("S:%d", SI_score);
  gfx.setCursor(52, 4);  gfx.printf("L:%d", SI_lives);
  gfx.setCursor(94, 4);  gfx.printf("H:%d", SI_highScore);

  SI_drawBar(142, 3, 44, 9, SI_ammo, SI_MAX_AMMO, TFT_YELLOW);

  gfx.setCursor(192, 4);
  if (SI_shieldActive) gfx.print("SH");
  else if (SI_tripleShotActive) gfx.print("3W");
  else gfx.print("N");

  if (SI_tripleShotActive) SI_drawBar(142, 12, 44, 5, SI_tripleShotFrames, SI_TRIPLE_SHOT_DURATION, SI_COL_POWERUP_TRIPLE);
  if (SI_shieldActive) SI_drawBar(190, 12, 44, 5, SI_shieldFrames, SI_SHIELD_DURATION, SI_COL_SHIELD_RING_1);
}

void SI_drawWorld() {
  gfx.fillSprite(SI_COL_BG);
  SI_drawStars();

  gfx.setTextColor(0x2104, SI_COL_BG);
  gfx.drawString("etiisda", 5, SI_SCREEN_H - 15, 2);

  SI_drawHUD();
  SI_drawBullets();
  SI_drawEnemies();
  SI_drawPowerUp();
  SI_drawPlayer();
}

void SI_renderMenu() {
  gfx.fillSprite(SI_COL_BG);
  SI_drawStars();

  SI_drawCentered("SPACE SHOOTER", 10, TFT_CYAN, 4);

  SI_drawPanelBox(10, 38, 220, 86);
  SI_drawCentered("4 / 6 = bewegen", 32, SI_COL_TEXT, 2);
  SI_drawCentered("2 / 5 = feuern", 46, SI_COL_TEXT, 2);
  SI_drawCentered("Ammo laedt automatisch", 60, TFT_YELLOW, 2);
  SI_drawCentered("Gruen = +1 Leben", 74, TFT_GREEN, 2);
  SI_drawCentered("Blau = Schild", 88, TFT_BLUE, 2);
  SI_drawCentered("Gold = Triple", 102, SI_COL_POWERUP_TRIPLE, 2);
  SI_drawCentered("5 = START", 116, SI_COL_WARN, 2);

  gfx.pushSprite(0, 0);
}

void SI_renderGame() {
  SI_drawWorld();
  gfx.pushSprite(0, 0);
}

void SI_renderPaused() {
  SI_drawWorld();

  gfx.fillRect(28, 30, 184, 74, TFT_BLACK);
  gfx.drawRect(28, 30, 184, 74, SI_COL_BORDER);

  SI_drawCentered("PAUSE", 38, SI_COL_WARN, 4);
  SI_drawCentered("5 / # = weiter", 68, SI_COL_TEXT, 2);
  SI_drawCentered("* = Launcher", 86, SI_COL_ACCENT, 2);

  gfx.pushSprite(0, 0);
}

void SI_updateStars() {
  for (int i = 0; i < SI_STAR_COUNT; i++) {
    SI_stars[i].y += SI_stars[i].speed;
    if (SI_stars[i].y >= SI_SCREEN_H) {
      SI_stars[i].y = 0;
      SI_stars[i].x = random(0, SI_SCREEN_W);
      SI_stars[i].speed = random(1, 4);
    }
  }
}

void SI_handlePlayerMovement() {
  if (SI_keyLeftHeld && !SI_keyRightHeld) SI_playerX -= SI_playerSpeed;
  if (SI_keyRightHeld && !SI_keyLeftHeld) SI_playerX += SI_playerSpeed;

  if (SI_playerX < 0) SI_playerX = 0;
  if (SI_playerX > SI_SCREEN_W - SI_playerW) SI_playerX = SI_SCREEN_W - SI_playerW;
}

void SI_updateAmmoRecharge() {
  if (SI_ammo < SI_MAX_AMMO) {
    SI_ammoRechargeFrames++;
    if (SI_ammoRechargeFrames >= SI_AMMO_RECHARGE_TIME) {
      SI_ammo++;
      if (SI_ammo > SI_MAX_AMMO) SI_ammo = SI_MAX_AMMO;
      SI_ammoRechargeFrames = 0;
    }
  } else {
    SI_ammoRechargeFrames = 0;
  }
}

void SI_updateTripleShotTimer() {
  if (SI_tripleShotActive) {
    SI_tripleShotFrames--;
    if (SI_tripleShotFrames <= 0) {
      SI_tripleShotActive = false;
      SI_tripleShotFrames = 0;
    }
  }
}

void SI_updateShieldTimer() {
  if (SI_shieldActive) {
    SI_shieldFrames--;
    if (SI_shieldFrames <= 0) {
      SI_shieldActive = false;
      SI_shieldFrames = 0;
    }
  }
}

void SI_tryFireBullet() {
  if (!SI_keyFireHeld) return;
  if (SI_fireCooldown > 0) return;
  if (SI_ammo <= 0) return;

  float startX = SI_playerX + (SI_playerW / 2.0f) - (SI_bulletW / 2.0f);
  float startY = SI_playerY - SI_bulletH;
  bool fired = false;

  if (SI_tripleShotActive) {
    bool b1 = SI_spawnBullet(startX, startY, 0.0f, -SI_bulletSpeedStraight);
    bool b2 = SI_spawnBullet(startX, startY, -SI_bulletSpeedDiagX, -SI_bulletSpeedDiagY);
    bool b3 = SI_spawnBullet(startX, startY,  SI_bulletSpeedDiagX, -SI_bulletSpeedDiagY);
    fired = b1 || b2 || b3;
  } else {
    fired = SI_spawnBullet(startX, startY, 0.0f, -SI_bulletSpeedStraight);
  }

  if (fired) {
    SI_ammo--;
    if (SI_ammo < 0) SI_ammo = 0;
    SI_fireCooldown = SI_FIRE_COOLDOWN_MAX;
  }
}

void SI_updateBullets() {
  if (SI_fireCooldown > 0) SI_fireCooldown--;

  for (int i = 0; i < SI_MAX_BULLETS; i++) {
    if (!SI_bullets[i].active) continue;

    SI_bullets[i].x += SI_bullets[i].dx;
    SI_bullets[i].y += SI_bullets[i].dy;

    if (SI_bullets[i].y + SI_bulletH < 0 ||
        SI_bullets[i].x + SI_bulletW < 0 ||
        SI_bullets[i].x > SI_SCREEN_W ||
        SI_bullets[i].y > SI_SCREEN_H) {
      SI_bullets[i].active = false;
    }
  }
}

void SI_updateEnemies() {
  for (int i = 0; i < SI_MAX_ENEMIES; i++) {
    if (!SI_enemies[i].active) continue;

    SI_enemies[i].y += SI_enemies[i].speed;

    if (SI_enemies[i].y > SI_SCREEN_H + 2) SI_spawnEnemy(i);
  }
}

void SI_updatePowerUp() {
  if (SI_powerUp.active) {
    SI_powerUp.y += SI_powerUp.speed;

    if (SI_powerUp.y > SI_SCREEN_H) {
      SI_resetPowerUp();
      return;
    }

    if (SI_overlap(SI_playerX, SI_playerY, SI_playerW, SI_playerH, SI_powerUp.x, SI_powerUp.y, SI_powerUp.w, SI_powerUp.h)) {
      if (SI_powerUp.type == SI_POWERUP_TRIPLE) {
        SI_tripleShotActive = true;
        SI_tripleShotFrames = SI_TRIPLE_SHOT_DURATION;
      }
      else if (SI_powerUp.type == SI_POWERUP_SHIELD) {
        SI_shieldActive = true;
        SI_shieldFrames = SI_SHIELD_DURATION;
      }
      else if (SI_powerUp.type == SI_POWERUP_LIFE) {
        SI_lives++;
        if (SI_lives > SI_MAX_LIVES) SI_lives = SI_MAX_LIVES;
      }

      SI_resetPowerUp();
      return;
    }
  } else {
    SI_powerUpSpawnTimer--;
    if (SI_powerUpSpawnTimer <= 0) SI_spawnPowerUp();
  }
}

void SI_handleBulletEnemyCollisions() {
  for (int b = 0; b < SI_MAX_BULLETS; b++) {
    if (!SI_bullets[b].active) continue;

    for (int e = 0; e < SI_MAX_ENEMIES; e++) {
      if (!SI_enemies[e].active) continue;

      if (SI_overlap((int)SI_bullets[b].x, (int)SI_bullets[b].y, SI_bulletW, SI_bulletH,
                     SI_enemies[e].x, SI_enemies[e].y, SI_enemies[e].w, SI_enemies[e].h)) {
        SI_bullets[b].active = false;
        SI_score++;

        if (SI_score > SI_highScore) SI_highScore = SI_score;

        if (!SI_powerUp.active && random(0, 10) == 0) SI_spawnPowerUp();

        SI_spawnEnemy(e);
        SI_enemies[e].y = random(-80, -20);
        break;
      }
    }
  }
}

void SI_handlePlayerEnemyCollisions() {
  if (SI_playerInvFrames > 0) {
    SI_playerInvFrames--;
    return;
  }

  for (int e = 0; e < SI_MAX_ENEMIES; e++) {
    if (!SI_enemies[e].active) continue;

    if (SI_overlap(SI_playerX, SI_playerY, SI_playerW, SI_playerH,
                   SI_enemies[e].x, SI_enemies[e].y, SI_enemies[e].w, SI_enemies[e].h)) {
      SI_spawnEnemy(e);
      SI_enemies[e].y = random(-70, -20);

      if (SI_shieldActive) {
        SI_shieldActive = false;
        SI_shieldFrames = 0;
        SI_playerInvFrames = SI_PLAYER_INV_MAX;
        return;
      }

      SI_lives--;
      SI_playerInvFrames = SI_PLAYER_INV_MAX;

      if (SI_lives <= 0) {
        if (SI_score > SI_highScore) SI_highScore = SI_score;
        SI_gameState = SI_STATE_MENU;
      }
      return;
    }
  }
}

void SI_updateMenu() {
  SI_updateStars();
  if (SI_keyBackPressed) {
    appMode = APP_LAUNCHER;
    return;
  }
  if (SI_keySelectPressed || SI_keyFireHeld) {
    SI_resetGame();
    SI_gameState = SI_STATE_PLAYING;
  }
}

void SI_updatePlaying() {
  if (SI_keyBackPressed) {
    SI_gameState = SI_STATE_PAUSED;
    return;
  }

  SI_updateStars();
  SI_handlePlayerMovement();
  SI_updateAmmoRecharge();
  SI_updateTripleShotTimer();
  SI_updateShieldTimer();
  SI_tryFireBullet();
  SI_updateBullets();
  SI_updateEnemies();
  SI_updatePowerUp();
  SI_handleBulletEnemyCollisions();
  SI_handlePlayerEnemyCollisions();
}

void SI_updatePaused() {
  SI_updateStars();

  if (SI_keySelectPressed) {
    SI_gameState = SI_STATE_PLAYING;
    return;
  }

  if (SI_keyBackPressed) {
    appMode = APP_LAUNCHER;
    return;
  }
}

void SI_enter() {
  SI_gameState = SI_STATE_MENU;
  SI_initStars();
  SI_initBullets();
  SI_initEnemies();
  SI_resetPowerUp();
  SI_lastFrameTime = 0;
}

void SI_loop() {
  SI_readInput();

  unsigned long now = millis();
  if (now - SI_lastFrameTime < SI_FRAME_TIME_MS) return;
  SI_lastFrameTime = now;

  switch (SI_gameState) {
    case SI_STATE_MENU:
      SI_updateMenu();
      if (appMode == APP_SPACE) SI_renderMenu();
      break;

    case SI_STATE_PLAYING:
      SI_updatePlaying();
      if (appMode != APP_SPACE) return;
      if (SI_gameState == SI_STATE_MENU) SI_renderMenu();
      else SI_renderGame();
      break;

    case SI_STATE_PAUSED:
      SI_updatePaused();
      if (appMode != APP_SPACE) return;
      if (SI_gameState == SI_STATE_PLAYING) SI_renderGame();
      else if (SI_gameState == SI_STATE_MENU) SI_renderMenu();
      else SI_renderPaused();
      break;
  }
}

// =====================================================
// ======================= PONG =========================
// =====================================================
enum PONG_GameState {
  PONG_TITLE,
  PONG_PLAYING,
  PONG_PAUSED,
  PONG_GOAL_ANIM,
  PONG_GAMEOVER
};

PONG_GameState PONG_state = PONG_TITLE;

uint16_t PONG_COL_BG       = TFT_BLACK;
uint16_t PONG_COL_GRID     = TFT_DARKGREY;
uint16_t PONG_COL_TEXT     = TFT_WHITE;
uint16_t PONG_COL_ACCENT   = TFT_CYAN;
uint16_t PONG_COL_PLAYER   = TFT_GREEN;
uint16_t PONG_COL_ENEMY    = TFT_RED;
uint16_t PONG_COL_BALL     = TFT_YELLOW;
uint16_t PONG_COL_TRAIL    = TFT_CYAN;
uint16_t PONG_COL_WIN      = TFT_GREEN;
uint16_t PONG_COL_LOSE     = TFT_RED;

const int PONG_paddleW = 6;
const int PONG_paddleH = 26;
const int PONG_paddleMargin = 8;

float PONG_playerX = 0;
float PONG_playerY = 0;
float PONG_enemyX = 0;
float PONG_enemyY = 0;

float PONG_playerSpeed = 3.6;
float PONG_enemySpeed = 2.4;

float PONG_ballX = 0;
float PONG_ballY = 0;
float PONG_ballVX = 0;
float PONG_ballVY = 0;
const int PONG_ballR = 3;

int PONG_playerScore = 0;
int PONG_enemyScore = 0;
const int PONG_maxScore = 7;

unsigned long PONG_frameCount = 0;
unsigned long PONG_goalAnimStart = 0;
const unsigned long PONG_GOAL_ANIM_DURATION = 1100;
String PONG_goalText = "";
uint16_t PONG_goalColor = TFT_WHITE;
bool PONG_nextServeToPlayer = false;

bool PONG_keyUpHeld = false;
bool PONG_keyDownHeld = false;
bool PONG_keyStartPressed = false;
bool PONG_keyPausePressed = false;

const int PONG_MAX_PART = 48;
struct PONG_Particle {
  float x;
  float y;
  float vx;
  float vy;
  int life;
  uint16_t color;
  bool active;
};
PONG_Particle PONG_parts[PONG_MAX_PART];

void PONG_addParticle(float x, float y, float vx, float vy, int life, uint16_t color) {
  for (int i = 0; i < PONG_MAX_PART; i++) {
    if (!PONG_parts[i].active) {
      PONG_parts[i].active = true;
      PONG_parts[i].x = x;
      PONG_parts[i].y = y;
      PONG_parts[i].vx = vx;
      PONG_parts[i].vy = vy;
      PONG_parts[i].life = life;
      PONG_parts[i].color = color;
      return;
    }
  }
}

void PONG_burst(float x, float y, int amount, uint16_t color) {
  for (int i = 0; i < amount; i++) {
    float vx = random(-22, 23) / 10.0;
    float vy = random(-22, 23) / 10.0;
    PONG_addParticle(x, y, vx, vy, random(10, 22), color);
  }
}

void PONG_ringBurst(float x, float y, int amount, float speed, uint16_t color) {
  for (int i = 0; i < amount; i++) {
    float a = (2.0 * PI * i) / amount;
    float vx = cos(a) * speed;
    float vy = sin(a) * speed;
    PONG_addParticle(x, y, vx, vy, random(12, 22), color);
  }
}

void PONG_updateParticles() {
  for (int i = 0; i < PONG_MAX_PART; i++) {
    if (PONG_parts[i].active) {
      PONG_parts[i].x += PONG_parts[i].vx;
      PONG_parts[i].y += PONG_parts[i].vy;
      PONG_parts[i].vx *= 0.985;
      PONG_parts[i].vy *= 0.985;
      PONG_parts[i].life--;
      if (PONG_parts[i].life <= 0) PONG_parts[i].active = false;
    }
  }
}

void PONG_drawParticles() {
  for (int i = 0; i < PONG_MAX_PART; i++) {
    if (PONG_parts[i].active) {
      int px = (int)PONG_parts[i].x;
      int py = (int)PONG_parts[i].y;
      if (px >= 0 && px < APP_SCREEN_W && py >= 0 && py < APP_SCREEN_H) gfx.drawPixel(px, py, PONG_parts[i].color);
    }
  }
}

void PONG_clampPaddles() {
  if (PONG_playerY < 12) PONG_playerY = 12;
  if (PONG_playerY > APP_SCREEN_H - 12 - PONG_paddleH) PONG_playerY = APP_SCREEN_H - 12 - PONG_paddleH;
  if (PONG_enemyY < 12) PONG_enemyY = 12;
  if (PONG_enemyY > APP_SCREEN_H - 12 - PONG_paddleH) PONG_enemyY = APP_SCREEN_H - 12 - PONG_paddleH;
}

void PONG_resetBall(bool serveToPlayer) {
  PONG_ballX = APP_SCREEN_W / 2;
  PONG_ballY = APP_SCREEN_H / 2;

  float speedX = random(18, 24) / 10.0;
  float speedY = random(-12, 13) / 10.0;
  if (speedY > -0.4 && speedY < 0.4) speedY = 0.7;

  PONG_ballVX = serveToPlayer ? -speedX : speedX;
  PONG_ballVY = speedY;

  PONG_burst(PONG_ballX, PONG_ballY, 10, PONG_COL_BALL);
}

void PONG_resetMatch() {
  PONG_playerScore = 0;
  PONG_enemyScore = 0;

  PONG_playerY = (APP_SCREEN_H - PONG_paddleH) / 2;
  PONG_enemyY = (APP_SCREEN_H - PONG_paddleH) / 2;

  PONG_keyUpHeld = false;
  PONG_keyDownHeld = false;
  PONG_keyStartPressed = false;
  PONG_keyPausePressed = false;

  for (int i = 0; i < PONG_MAX_PART; i++) PONG_parts[i].active = false;

  PONG_resetBall(false);
  PONG_state = PONG_PLAYING;
}

void PONG_updateInput() {
  PONG_keyUpHeld = false;
  PONG_keyDownHeld = false;
  PONG_keyStartPressed = false;
  PONG_keyPausePressed = false;

  keypad.getKeys();

  for (int i = 0; i < LIST_MAX; i++) {
    if (!keypad.key[i].kchar) continue;

    char k = keypad.key[i].kchar;
    KeyState s = keypad.key[i].kstate;

    if (k == '2' && (s == PRESSED || s == HOLD)) PONG_keyUpHeld = true;
    if (k == '8' && (s == PRESSED || s == HOLD)) PONG_keyDownHeld = true;
    if (k == '#' && s == PRESSED) PONG_keyStartPressed = true;
    if (k == '*' && s == PRESSED) PONG_keyPausePressed = true;
  }
}

void PONG_drawBackground() {
  gfx.fillSprite(PONG_COL_BG);

  for (int y = 0; y < APP_SCREEN_H; y += 12) {
    int off = (PONG_frameCount / 2 + y) % 24;
    for (int x = -24; x < APP_SCREEN_W + 24; x += 24) {
      gfx.drawFastHLine(x + off, y, 8, PONG_COL_GRID);
    }
  }

  for (int y = 0; y < APP_SCREEN_H; y += 10) {
    gfx.drawFastVLine(APP_SCREEN_W / 2, y + (PONG_frameCount % 4), 5, TFT_DARKGREY);
  }
}

void PONG_drawPaddle(int x, int y, uint16_t mainColor, uint16_t shineColor) {
  gfx.fillRoundRect(x, y, PONG_paddleW, PONG_paddleH, 2, mainColor);
  gfx.drawRoundRect(x, y, PONG_paddleW, PONG_paddleH, 2, TFT_WHITE);

  int innerW = PONG_paddleW - 2;
  int innerH = PONG_paddleH - 4;
  if (innerW > 0 && innerH > 0) gfx.fillRect(x + 1, y + 2, innerW, innerH, shineColor);
}

void PONG_drawBall() {
  for (int i = 1; i <= 4; i++) {
    int tx = (int)(PONG_ballX - PONG_ballVX * i * 1.8);
    int ty = (int)(PONG_ballY - PONG_ballVY * i * 1.8);
    if (tx >= 0 && tx < APP_SCREEN_W && ty >= 0 && ty < APP_SCREEN_H) gfx.drawPixel(tx, ty, PONG_COL_TRAIL);
  }

  gfx.fillCircle((int)PONG_ballX, (int)PONG_ballY, PONG_ballR, PONG_COL_BALL);
  gfx.drawCircle((int)PONG_ballX, (int)PONG_ballY, PONG_ballR, TFT_WHITE);
}

void PONG_drawHUD() {
  gfx.setTextSize(1);
  gfx.setTextColor(PONG_COL_TEXT, PONG_COL_BG);

  gfx.drawString(String(PONG_playerScore), APP_SCREEN_W / 2 - 24, 2);
  gfx.drawString(String(PONG_enemyScore), APP_SCREEN_W / 2 + 16, 2);

  gfx.drawString("P1", 8, 2);
  gfx.drawString("CPU", APP_SCREEN_W - 24, 2);
}

void PONG_drawGoalAnimation() {
  unsigned long elapsed = millis() - PONG_goalAnimStart;
  if (elapsed > PONG_GOAL_ANIM_DURATION) elapsed = PONG_GOAL_ANIM_DURATION;

  int phase = elapsed / 60;
  int flashBand = (phase * 12) % (APP_SCREEN_W + 40) - 20;

  for (int i = 0; i < 4; i++) {
    int x = flashBand - i * 18;
    if (x >= 0 && x < APP_SCREEN_W) gfx.drawFastVLine(x, 0, APP_SCREEN_H, PONG_goalColor);
    if (x + 1 >= 0 && x + 1 < APP_SCREEN_W) gfx.drawFastVLine(x + 1, 0, APP_SCREEN_H, TFT_WHITE);
  }

  int boxW = 108 + (int)(8 * sin(elapsed * 0.02));
  int boxH = 34;
  int boxX = (APP_SCREEN_W - boxW) / 2;
  int boxY = 46;

  gfx.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
  gfx.drawRect(boxX, boxY, boxW, boxH, PONG_goalColor);
  gfx.drawRect(boxX - 1, boxY - 1, boxW + 2, boxH + 2, TFT_WHITE);

  gfx.setTextColor(PONG_goalColor, TFT_BLACK);
  gfx.setTextSize(2);
  gfx.drawCentreString(PONG_goalText, APP_SCREEN_W / 2, 54, 1);

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.drawCentreString("naechster aufschlag...", APP_SCREEN_W / 2, 76, 1);
}

void PONG_drawTitle() {
  PONG_drawBackground();

  gfx.setTextSize(2);
  gfx.setTextColor(PONG_COL_ACCENT, PONG_COL_BG);
  gfx.drawString("NEON PONG", 18, 16);

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_YELLOW, PONG_COL_BG);
  gfx.drawString("ESP32 Keypad Edition", 24, 38);

  PONG_drawPaddle(18, 64, PONG_COL_PLAYER, TFT_DARKGREEN);
  PONG_drawPaddle(APP_SCREEN_W - 24, 52, PONG_COL_ENEMY, TFT_MAROON);
  gfx.fillCircle(APP_SCREEN_W / 2, 70, PONG_ballR, PONG_COL_BALL);

  gfx.setTextColor(PONG_COL_TEXT, PONG_COL_BG);
  gfx.drawString("2 = HOCH halten", 38, 88);
  gfx.drawString("8 = RUNTER halten", 34, 100);
  gfx.drawString("# = START", 48, 112);
  gfx.drawString("* = LAUNCHER", 40, 124);

  gfx.pushSprite(0, 0);
}

void PONG_drawPause() {
  gfx.fillRect(34, 44, 92, 30, TFT_BLACK);
  gfx.drawRect(34, 44, 92, 30, TFT_WHITE);
  gfx.setTextSize(2);
  gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
  gfx.drawString("PAUSE", 42, 50);
}

void PONG_drawGameOver(bool playerWon) {
  gfx.fillRect(16, 38, 128, 42, TFT_BLACK);
  gfx.drawRect(16, 38, 128, 42, playerWon ? PONG_COL_WIN : PONG_COL_LOSE);

  gfx.setTextSize(2);
  gfx.setTextColor(playerWon ? PONG_COL_WIN : PONG_COL_LOSE, TFT_BLACK);
  if (playerWon) gfx.drawCentreString("SIEG!", APP_SCREEN_W / 2, 46, 1);
  else gfx.drawCentreString("GAME OVER", APP_SCREEN_W / 2, 46, 1);

  gfx.setTextSize(1);
  gfx.setTextColor(PONG_COL_TEXT, PONG_COL_BG);
  gfx.drawString("# = nochmal", 46, 88);
  gfx.drawString("* = launcher", 44, 100);
}

void PONG_renderGame() {
  PONG_drawBackground();
  PONG_drawPaddle((int)PONG_playerX, (int)PONG_playerY, PONG_COL_PLAYER, TFT_DARKGREEN);
  PONG_drawPaddle((int)PONG_enemyX, (int)PONG_enemyY, PONG_COL_ENEMY, TFT_MAROON);

  if (PONG_state == PONG_PLAYING) PONG_drawBall();

  PONG_drawParticles();
  PONG_drawHUD();

  if (PONG_state == PONG_PAUSED) PONG_drawPause();
  if (PONG_state == PONG_GOAL_ANIM) PONG_drawGoalAnimation();
  if (PONG_state == PONG_GAMEOVER) PONG_drawGameOver(PONG_playerScore >= PONG_maxScore);

  gfx.pushSprite(0, 0);
}

void PONG_handleInput() {
  if (PONG_keyUpHeld) PONG_playerY -= PONG_playerSpeed;
  if (PONG_keyDownHeld) PONG_playerY += PONG_playerSpeed;
}

void PONG_updateEnemy() {
  float enemyCenter = PONG_enemyY + PONG_paddleH / 2.0;
  if (PONG_ballY < enemyCenter - 3) PONG_enemyY -= PONG_enemySpeed;
  if (PONG_ballY > enemyCenter + 3) PONG_enemyY += PONG_enemySpeed;
  PONG_clampPaddles();
}

void PONG_bounceFromPaddle(float paddleY, bool playerPaddle) {
  float paddleCenter = paddleY + PONG_paddleH / 2.0;
  float impact = (PONG_ballY - paddleCenter) / (PONG_paddleH / 2.0);

  if (impact < -1.0) impact = -1.0;
  if (impact > 1.0) impact = 1.0;

  float speed = sqrt(PONG_ballVX * PONG_ballVX + PONG_ballVY * PONG_ballVY);
  speed += 0.16;
  if (speed > 4.9) speed = 4.9;

  PONG_ballVX = playerPaddle ? speed : -speed;
  PONG_ballVY = impact * 2.8;

  PONG_burst(PONG_ballX, PONG_ballY, 8, playerPaddle ? PONG_COL_PLAYER : PONG_COL_ENEMY);
}

void PONG_startGoalAnimation(bool playerScored) {
  PONG_goalAnimStart = millis();
  PONG_state = PONG_GOAL_ANIM;

  if (playerScored) {
    PONG_goalText = "GOAL!";
    PONG_goalColor = PONG_COL_PLAYER;
    PONG_nextServeToPlayer = true;
  } else {
    PONG_goalText = "CPU SCORE!";
    PONG_goalColor = PONG_COL_ENEMY;
    PONG_nextServeToPlayer = false;
  }

  PONG_ringBurst(APP_SCREEN_W / 2, APP_SCREEN_H / 2, 18, 1.8, PONG_goalColor);
  PONG_burst(APP_SCREEN_W / 2, APP_SCREEN_H / 2, 18, TFT_WHITE);
}

void PONG_finishGoalAnimation() {
  if (PONG_playerScore >= PONG_maxScore || PONG_enemyScore >= PONG_maxScore) {
    PONG_state = PONG_GAMEOVER;
  } else {
    PONG_resetBall(PONG_nextServeToPlayer);
    PONG_state = PONG_PLAYING;
  }
}

void PONG_updateBall() {
  PONG_ballX += PONG_ballVX;
  PONG_ballY += PONG_ballVY;

  if (PONG_ballY - PONG_ballR <= 12) {
    PONG_ballY = 12 + PONG_ballR;
    PONG_ballVY = -PONG_ballVY;
    PONG_burst(PONG_ballX, PONG_ballY, 4, PONG_COL_BALL);
  }

  if (PONG_ballY + PONG_ballR >= APP_SCREEN_H - 1) {
    PONG_ballY = APP_SCREEN_H - 1 - PONG_ballR;
    PONG_ballVY = -PONG_ballVY;
    PONG_burst(PONG_ballX, PONG_ballY, 4, PONG_COL_BALL);
  }

  if (PONG_ballVX < 0) {
    if (PONG_ballX - PONG_ballR <= PONG_playerX + PONG_paddleW &&
        PONG_ballX > PONG_playerX &&
        PONG_ballY + PONG_ballR >= PONG_playerY &&
        PONG_ballY - PONG_ballR <= PONG_playerY + PONG_paddleH) {
      PONG_ballX = PONG_playerX + PONG_paddleW + PONG_ballR;
      PONG_bounceFromPaddle(PONG_playerY, true);
    }
  }

  if (PONG_ballVX > 0) {
    if (PONG_ballX + PONG_ballR >= PONG_enemyX &&
        PONG_ballX < PONG_enemyX + PONG_paddleW &&
        PONG_ballY + PONG_ballR >= PONG_enemyY &&
        PONG_ballY - PONG_ballR <= PONG_enemyY + PONG_paddleH) {
      PONG_ballX = PONG_enemyX - PONG_ballR;
      PONG_bounceFromPaddle(PONG_enemyY, false);
    }
  }

  if (PONG_ballX < -10) {
    PONG_enemyScore++;
    PONG_burst(PONG_ballX + 8, PONG_ballY, 22, PONG_COL_ENEMY);
    PONG_ringBurst(APP_SCREEN_W / 2, APP_SCREEN_H / 2, 20, 2.0, PONG_COL_ENEMY);
    PONG_startGoalAnimation(false);
  }

  if (PONG_ballX > APP_SCREEN_W + 10) {
    PONG_playerScore++;
    PONG_burst(PONG_ballX - 8, PONG_ballY, 22, PONG_COL_PLAYER);
    PONG_ringBurst(APP_SCREEN_W / 2, APP_SCREEN_H / 2, 20, 2.0, PONG_COL_PLAYER);
    PONG_startGoalAnimation(true);
  }
}

void PONG_updateGame() {
  PONG_handleInput();
  PONG_clampPaddles();
  PONG_updateEnemy();
  PONG_updateBall();
  PONG_updateParticles();
}

void PONG_updateGoalAnim() {
  PONG_updateParticles();
  if (millis() - PONG_goalAnimStart > PONG_GOAL_ANIM_DURATION) PONG_finishGoalAnimation();
}

void PONG_enter() {
  PONG_playerX = PONG_paddleMargin;
  PONG_enemyX = APP_SCREEN_W - PONG_paddleMargin - PONG_paddleW;
  PONG_playerY = (APP_SCREEN_H - PONG_paddleH) / 2;
  PONG_enemyY = (APP_SCREEN_H - PONG_paddleH) / 2;
  PONG_resetBall(false);
  PONG_state = PONG_TITLE;
}

void PONG_loop() {
  PONG_updateInput();
  PONG_frameCount++;

  if (PONG_keyPausePressed && PONG_state != PONG_TITLE) {
    appMode = APP_LAUNCHER;
    return;
  }

  if (PONG_state == PONG_TITLE) {
    if (PONG_keyPausePressed) {
      appMode = APP_LAUNCHER;
      return;
    }
    if (PONG_keyStartPressed) {
      PONG_resetMatch();
      delay(120);
    }
    PONG_drawTitle();
    delay(16);
    return;
  }

  if (PONG_state == PONG_PLAYING) {
    PONG_updateGame();
    PONG_renderGame();
    delay(16);
    return;
  }

  if (PONG_state == PONG_GOAL_ANIM) {
    PONG_updateGoalAnim();
    PONG_renderGame();
    delay(16);
    return;
  }

  if (PONG_state == PONG_GAMEOVER) {
    PONG_updateParticles();
    PONG_renderGame();

    if (PONG_keyStartPressed) {
      PONG_resetMatch();
      delay(120);
    }
    if (PONG_keyPausePressed) {
      appMode = APP_LAUNCHER;
      return;
    }

    delay(16);
    return;
  }
}

// =====================================================
// ======================== DOOM ========================
// =====================================================
#define DOOM_MAP_W 10
#define DOOM_MAP_H 10

int DOOM_worldMap[DOOM_MAP_H][DOOM_MAP_W] = {
  {1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,0,1},
  {1,0,1,0,1,0,1,0,0,1},
  {1,0,1,0,1,0,1,0,0,1},
  {1,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,1,0,1},
  {1,0,1,0,1,0,0,0,0,1},
  {1,0,0,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1}
};

float DOOM_playerX = 3.5f;
float DOOM_playerY = 3.5f;
float DOOM_playerAngle = 0.0f;

#define DOOM_FOV 1.0f
#define DOOM_MAX_DEPTH 8.0f
#define DOOM_COLUMN_W 2
#define DOOM_NUM_RAYS (APP_SCREEN_W / DOOM_COLUMN_W)

bool DOOM_held2 = false;
bool DOOM_held4 = false;
bool DOOM_held6 = false;
bool DOOM_held8 = false;
bool DOOM_shootPressed = false;
bool DOOM_backPressed = false;

unsigned long DOOM_lastMoveTime = 0;
unsigned long DOOM_lastRenderTime = 0;
unsigned long DOOM_lastShotTime = 0;
unsigned long DOOM_lastDamageTime = 0;
unsigned long DOOM_roundMessageUntil = 0;
unsigned long DOOM_lastMenuMoveTime = 0;

const unsigned long DOOM_moveInterval = 22;
const unsigned long DOOM_renderInterval = 24;
const unsigned long DOOM_shotCooldown = 180;
const unsigned long DOOM_damageCooldown = 800;
const unsigned long DOOM_menuMoveCooldown = 180;

uint16_t DOOM_COLOR_SKY;
uint16_t DOOM_COLOR_FLOOR;
uint16_t DOOM_COLOR_WALL;
uint16_t DOOM_COLOR_ENEMY_1;
uint16_t DOOM_COLOR_ENEMY_2;
uint16_t DOOM_COLOR_BULLET;
uint16_t DOOM_COLOR_EDGE;
uint16_t DOOM_COLOR_GUN_WOOD;
uint16_t DOOM_COLOR_GUN_WOOD_DARK;
uint16_t DOOM_COLOR_GUN_METAL;
uint16_t DOOM_COLOR_POWERUP_TRIPLE;
uint16_t DOOM_COLOR_POWERUP_SHIELD;
uint16_t DOOM_COLOR_POWERUP_LIFE;

float DOOM_depthBuffer[DOOM_NUM_RAYS];

#define DOOM_MAX_ENEMIES 8
struct DOOM_Enemy {
  float x;
  float y;
  bool alive;
  float dirX;
  float dirY;
  unsigned long nextDecisionTime;
};
DOOM_Enemy DOOM_enemies[DOOM_MAX_ENEMIES];

#define DOOM_MAX_BULLETS 8
struct DOOM_Bullet {
  bool active;
  float x;
  float y;
  float dx;
  float dy;
  float distTraveled;
};
DOOM_Bullet DOOM_bullets[DOOM_MAX_BULLETS];

enum DOOM_PowerUpType {
  DOOM_POWERUP_TRIPLE,
  DOOM_POWERUP_SHIELD,
  DOOM_POWERUP_LIFE
};

struct DOOM_PowerUp {
  bool active;
  float x;
  float y;
  DOOM_PowerUpType type;
};
DOOM_PowerUp DOOM_powerUp;

bool DOOM_tripleShotActive = false;
unsigned long DOOM_tripleShotUntil = 0;
const unsigned long DOOM_TRIPLE_SHOT_DURATION = 5500;

bool DOOM_shieldActive = false;
unsigned long DOOM_shieldUntil = 0;
const unsigned long DOOM_SHIELD_DURATION = 6500;

const int DOOM_BASE_HEALTH = 3;
const int DOOM_MAX_HEALTH = 5;
int DOOM_playerHealth = DOOM_BASE_HEALTH;
int DOOM_roundNumber = 1;
int DOOM_killCount = 0;

enum DOOM_GameState {
  DOOM_STATE_MENU,
  DOOM_STATE_PLAYING
};

DOOM_GameState DOOM_gameState = DOOM_STATE_MENU;
int DOOM_menuIndex = 0;
const int DOOM_menuItemCount = 2;

float DOOM_normalizeAngle(float a) {
  while (a < 0.0f) a += 2.0f * PI;
  while (a >= 2.0f * PI) a -= 2.0f * PI;
  return a;
}

float DOOM_angleDiff(float a, float b) {
  float d = a - b;
  while (d > PI) d -= 2.0f * PI;
  while (d < -PI) d += 2.0f * PI;
  return d;
}

bool DOOM_isWall(float x, float y) {
  int tx = (int)x;
  int ty = (int)y;
  if (tx < 0 || tx >= DOOM_MAP_W || ty < 0 || ty >= DOOM_MAP_H) return true;
  return DOOM_worldMap[ty][tx] == 1;
}

bool DOOM_isBlocked(float x, float y) {
  const float r = 0.15f;
  if (DOOM_isWall(x - r, y - r)) return true;
  if (DOOM_isWall(x + r, y - r)) return true;
  if (DOOM_isWall(x - r, y + r)) return true;
  if (DOOM_isWall(x + r, y + r)) return true;
  return false;
}

bool DOOM_isBlockedEnemy(float x, float y) {
  const float r = 0.14f;
  if (DOOM_isWall(x - r, y - r)) return true;
  if (DOOM_isWall(x + r, y - r)) return true;
  if (DOOM_isWall(x - r, y + r)) return true;
  if (DOOM_isWall(x + r, y + r)) return true;
  return false;
}

float DOOM_dist2D(float ax, float ay, float bx, float by) {
  float dx = bx - ax;
  float dy = by - ay;
  return sqrtf(dx * dx + dy * dy);
}

bool DOOM_enemyTooCloseToPlayer(float x, float y) {
  return DOOM_dist2D(x, y, DOOM_playerX, DOOM_playerY) < 1.8f;
}

void DOOM_deactivateAllBullets() {
  for (int i = 0; i < DOOM_MAX_BULLETS; i++) DOOM_bullets[i].active = false;
}

void DOOM_resetPlayerForNewRound() {
  DOOM_playerX = 3.5f;
  DOOM_playerY = 3.5f;
  DOOM_playerAngle = 0.0f;
  DOOM_playerHealth = DOOM_BASE_HEALTH;
  DOOM_shieldActive = false;
  DOOM_shieldUntil = 0;
  DOOM_tripleShotActive = false;
  DOOM_tripleShotUntil = 0;
  DOOM_deactivateAllBullets();
  DOOM_roundMessageUntil = millis() + 1200;
}

void DOOM_resetWholeGame() {
  DOOM_roundNumber = 1;
  DOOM_killCount = 0;
  DOOM_lastShotTime = 0;
  DOOM_lastDamageTime = 0;
  DOOM_resetPlayerForNewRound();
  DOOM_powerUp.active = false;
}

void DOOM_spawnEnemyAt(int i) {
  for (int tries = 0; tries < 150; tries++) {
    float ex = (float)(random(1, DOOM_MAP_W - 1)) + 0.5f;
    float ey = (float)(random(1, DOOM_MAP_H - 1)) + 0.5f;

    if (DOOM_isWall(ex, ey)) continue;
    if (DOOM_enemyTooCloseToPlayer(ex, ey)) continue;

    bool tooCloseToOthers = false;
    for (int j = 0; j < DOOM_MAX_ENEMIES; j++) {
      if (j == i || !DOOM_enemies[j].alive) continue;
      if (DOOM_dist2D(ex, ey, DOOM_enemies[j].x, DOOM_enemies[j].y) < 0.7f) {
        tooCloseToOthers = true;
        break;
      }
    }

    if (tooCloseToOthers) continue;

    DOOM_enemies[i].x = ex;
    DOOM_enemies[i].y = ey;
    DOOM_enemies[i].alive = true;

    float a = random(0, 628) / 100.0f;
    DOOM_enemies[i].dirX = cosf(a);
    DOOM_enemies[i].dirY = sinf(a);
    DOOM_enemies[i].nextDecisionTime = millis() + random(250, 700);
    return;
  }

  DOOM_enemies[i].x = 7.5f;
  DOOM_enemies[i].y = 7.5f;
  DOOM_enemies[i].alive = true;
  DOOM_enemies[i].dirX = -1.0f;
  DOOM_enemies[i].dirY = 0.0f;
  DOOM_enemies[i].nextDecisionTime = millis() + 400;
}

void DOOM_initEnemies() {
  for (int i = 0; i < DOOM_MAX_ENEMIES; i++) {
    DOOM_enemies[i].alive = false;
    DOOM_spawnEnemyAt(i);
  }
}

void DOOM_startNewRound() {
  DOOM_roundNumber++;
  DOOM_resetPlayerForNewRound();
  DOOM_initEnemies();
  DOOM_powerUp.active = false;
}

void DOOM_startGameFromMenu() {
  DOOM_resetWholeGame();
  DOOM_initEnemies();
  DOOM_gameState = DOOM_STATE_PLAYING;
}

bool DOOM_spawnBullet(float x, float y, float dx, float dy) {
  for (int i = 0; i < DOOM_MAX_BULLETS; i++) {
    if (!DOOM_bullets[i].active) {
      DOOM_bullets[i].active = true;
      DOOM_bullets[i].x = x;
      DOOM_bullets[i].y = y;
      DOOM_bullets[i].dx = dx;
      DOOM_bullets[i].dy = dy;
      DOOM_bullets[i].distTraveled = 0.0f;
      return true;
    }
  }
  return false;
}

void DOOM_trySpawnPowerUpAt(float x, float y) {
  if (DOOM_powerUp.active) return;
  if (random(0, 100) > 24) return;

  DOOM_powerUp.active = true;
  DOOM_powerUp.x = x;
  DOOM_powerUp.y = y;

  int r = random(0, 100);
  if (r < 40) DOOM_powerUp.type = DOOM_POWERUP_TRIPLE;
  else if (r < 75) DOOM_powerUp.type = DOOM_POWERUP_SHIELD;
  else DOOM_powerUp.type = DOOM_POWERUP_LIFE;
}

void DOOM_updatePowerupTimers() {
  unsigned long now = millis();

  if (DOOM_tripleShotActive && now >= DOOM_tripleShotUntil) DOOM_tripleShotActive = false;
  if (DOOM_shieldActive && now >= DOOM_shieldUntil) DOOM_shieldActive = false;
}

void DOOM_updatePowerUpPickup() {
  if (!DOOM_powerUp.active) return;

  if (DOOM_dist2D(DOOM_playerX, DOOM_playerY, DOOM_powerUp.x, DOOM_powerUp.y) < 0.45f) {
    if (DOOM_powerUp.type == DOOM_POWERUP_TRIPLE) {
      DOOM_tripleShotActive = true;
      DOOM_tripleShotUntil = millis() + DOOM_TRIPLE_SHOT_DURATION;
    }
    else if (DOOM_powerUp.type == DOOM_POWERUP_SHIELD) {
      DOOM_shieldActive = true;
      DOOM_shieldUntil = millis() + DOOM_SHIELD_DURATION;
    }
    else if (DOOM_powerUp.type == DOOM_POWERUP_LIFE) {
      DOOM_playerHealth++;
      if (DOOM_playerHealth > DOOM_MAX_HEALTH) DOOM_playerHealth = DOOM_MAX_HEALTH;
    }

    DOOM_powerUp.active = false;
  }
}

void DOOM_handleInput() {
  keypad.getKeys();

  DOOM_held2 = false;
  DOOM_held4 = false;
  DOOM_held6 = false;
  DOOM_held8 = false;
  DOOM_shootPressed = false;
  DOOM_backPressed = false;

  for (int i = 0; i < LIST_MAX; i++) {
    if (keypad.key[i].kchar == NO_KEY) continue;

    char k = keypad.key[i].kchar;
    KeyState s = keypad.key[i].kstate;
    bool active = (s == PRESSED || s == HOLD);

    if (DOOM_gameState == DOOM_STATE_PLAYING) {
      if (k == '2' && active) DOOM_held2 = true;
      if (k == '4' && active) DOOM_held4 = true;
      if (k == '6' && active) DOOM_held6 = true;
      if (k == '8' && active) DOOM_held8 = true;
      if (k == '#' && s == PRESSED) DOOM_shootPressed = true;
      if (k == '*' && s == PRESSED) DOOM_backPressed = true;
    }
  }
}

void DOOM_handleMenuInput() {
  keypad.getKeys();

  bool upPressed = false;
  bool downPressed = false;
  bool selectPressed = false;
  bool backPressed = false;

  for (int i = 0; i < LIST_MAX; i++) {
    if (keypad.key[i].kchar == NO_KEY) continue;

    char k = keypad.key[i].kchar;
    KeyState s = keypad.key[i].kstate;

    if (k == '2' && s == PRESSED) upPressed = true;
    if (k == '8' && s == PRESSED) downPressed = true;
    if (k == '#' && s == PRESSED) selectPressed = true;
    if (k == '*' && s == PRESSED) backPressed = true;
  }

  if (backPressed) {
    appMode = APP_LAUNCHER;
    return;
  }

  if (millis() - DOOM_lastMenuMoveTime > DOOM_menuMoveCooldown) {
    if (upPressed) {
      DOOM_menuIndex--;
      if (DOOM_menuIndex < 0) DOOM_menuIndex = DOOM_menuItemCount - 1;
      DOOM_lastMenuMoveTime = millis();
    }
    if (downPressed) {
      DOOM_menuIndex++;
      if (DOOM_menuIndex >= DOOM_menuItemCount) DOOM_menuIndex = 0;
      DOOM_lastMenuMoveTime = millis();
    }
  }

  if (selectPressed) {
    if (DOOM_menuIndex == 0) DOOM_startGameFromMenu();
  }
}

void DOOM_movePlayer() {
  if (millis() - DOOM_lastMoveTime < DOOM_moveInterval) return;
  DOOM_lastMoveTime = millis();

  const float turnStep = 0.085f;
  const float moveStep = 0.11f;

  if (DOOM_held4) DOOM_playerAngle -= turnStep;
  if (DOOM_held6) DOOM_playerAngle += turnStep;

  DOOM_playerAngle = DOOM_normalizeAngle(DOOM_playerAngle);

  float moveX = 0.0f;
  float moveY = 0.0f;

  if (DOOM_held2) {
    moveX += cosf(DOOM_playerAngle) * moveStep;
    moveY += sinf(DOOM_playerAngle) * moveStep;
  }

  if (DOOM_held8) {
    moveX -= cosf(DOOM_playerAngle) * moveStep;
    moveY -= sinf(DOOM_playerAngle) * moveStep;
  }

  float newX = DOOM_playerX + moveX;
  float newY = DOOM_playerY + moveY;

  if (!DOOM_isBlocked(newX, DOOM_playerY)) DOOM_playerX = newX;
  if (!DOOM_isBlocked(DOOM_playerX, newY)) DOOM_playerY = newY;
}

void DOOM_tryShoot() {
  if (!DOOM_shootPressed) return;
  if (millis() - DOOM_lastShotTime < DOOM_shotCooldown) return;

  DOOM_lastShotTime = millis();

  float baseX = DOOM_playerX + cosf(DOOM_playerAngle) * 0.25f;
  float baseY = DOOM_playerY + sinf(DOOM_playerAngle) * 0.25f;
  float speed = 0.22f;

  if (DOOM_tripleShotActive) {
    float spread = 0.16f;
    DOOM_spawnBullet(baseX, baseY, cosf(DOOM_playerAngle) * speed, sinf(DOOM_playerAngle) * speed);
    DOOM_spawnBullet(baseX, baseY, cosf(DOOM_playerAngle - spread) * speed, sinf(DOOM_playerAngle - spread) * speed);
    DOOM_spawnBullet(baseX, baseY, cosf(DOOM_playerAngle + spread) * speed, sinf(DOOM_playerAngle + spread) * speed);
  } else {
    DOOM_spawnBullet(baseX, baseY, cosf(DOOM_playerAngle) * speed, sinf(DOOM_playerAngle) * speed);
  }
}

void DOOM_updateBullets() {
  for (int i = 0; i < DOOM_MAX_BULLETS; i++) {
    if (!DOOM_bullets[i].active) continue;

    for (int step = 0; step < 2; step++) {
      DOOM_bullets[i].x += DOOM_bullets[i].dx;
      DOOM_bullets[i].y += DOOM_bullets[i].dy;
      DOOM_bullets[i].distTraveled += 0.22f;

      if (DOOM_isWall(DOOM_bullets[i].x, DOOM_bullets[i].y) || DOOM_bullets[i].distTraveled > DOOM_MAX_DEPTH) {
        DOOM_bullets[i].active = false;
        break;
      }

      bool bulletHit = false;

      for (int e = 0; e < DOOM_MAX_ENEMIES; e++) {
        if (!DOOM_enemies[e].alive) continue;

        if (DOOM_dist2D(DOOM_bullets[i].x, DOOM_bullets[i].y, DOOM_enemies[e].x, DOOM_enemies[e].y) < 0.35f) {
          float ex = DOOM_enemies[e].x;
          float ey = DOOM_enemies[e].y;

          DOOM_enemies[e].alive = false;
          DOOM_bullets[i].active = false;
          DOOM_killCount++;

          DOOM_trySpawnPowerUpAt(ex, ey);
          DOOM_spawnEnemyAt(e);

          bulletHit = true;
          break;
        }
      }

      if (bulletHit) break;
    }
  }
}

void DOOM_updateEnemies() {
  static unsigned long lastEnemyMove = 0;
  if (millis() - lastEnemyMove < 55) return;
  lastEnemyMove = millis();

  for (int i = 0; i < DOOM_MAX_ENEMIES; i++) {
    if (!DOOM_enemies[i].alive) {
      DOOM_spawnEnemyAt(i);
      continue;
    }

    DOOM_Enemy &e = DOOM_enemies[i];

    float toPlayerX = DOOM_playerX - e.x;
    float toPlayerY = DOOM_playerY - e.y;
    float d = sqrtf(toPlayerX * toPlayerX + toPlayerY * toPlayerY);

    if (millis() > e.nextDecisionTime) {
      if (d > 0.01f) {
        float jitterA = random(-35, 36) * 0.0174533f;
        float baseA = atan2f(toPlayerY, toPlayerX);
        float newA = baseA + jitterA;

        e.dirX = cosf(newA);
        e.dirY = sinf(newA);
      } else {
        float a = random(0, 628) / 100.0f;
        e.dirX = cosf(a);
        e.dirY = sinf(a);
      }

      e.nextDecisionTime = millis() + random(160, 420);
    }

    float enemySpeed = 0.028f + (DOOM_roundNumber - 1) * 0.002f;
    if (enemySpeed > 0.05f) enemySpeed = 0.05f;

    float nx = e.x + e.dirX * enemySpeed;
    float ny = e.y + e.dirY * enemySpeed;

    bool moved = false;

    if (!DOOM_isBlockedEnemy(nx, e.y)) {
      e.x = nx;
      moved = true;
    }

    if (!DOOM_isBlockedEnemy(e.x, ny)) {
      e.y = ny;
      moved = true;
    }

    if (!moved) {
      bool foundPath = false;

      for (int attempt = 0; attempt < 8; attempt++) {
        float testAngle = atan2f(e.dirY, e.dirX) + (-1.2f + 0.35f * attempt);
        float tx = cosf(testAngle);
        float ty = sinf(testAngle);

        float px = e.x + tx * enemySpeed;
        float py = e.y + ty * enemySpeed;

        if (!DOOM_isBlockedEnemy(px, py)) {
          e.dirX = tx;
          e.dirY = ty;
          e.x = px;
          e.y = py;
          foundPath = true;
          break;
        }
      }

      if (!foundPath) {
        float a = random(0, 628) / 100.0f;
        e.dirX = cosf(a);
        e.dirY = sinf(a);
        e.nextDecisionTime = millis() + random(120, 260);
      }
    }

    if (d < 2.2f && d > 0.01f) {
      float chaseX = toPlayerX / d;
      float chaseY = toPlayerY / d;

      float cx = e.x + chaseX * (enemySpeed * 0.75f);
      float cy = e.y + chaseY * (enemySpeed * 0.75f);

      if (!DOOM_isBlockedEnemy(cx, e.y)) e.x = cx;
      if (!DOOM_isBlockedEnemy(e.x, cy)) e.y = cy;

      e.dirX = chaseX;
      e.dirY = chaseY;
    }
  }
}

void DOOM_checkPlayerDamage() {
  if (millis() - DOOM_lastDamageTime < DOOM_damageCooldown) return;

  for (int i = 0; i < DOOM_MAX_ENEMIES; i++) {
    if (!DOOM_enemies[i].alive) continue;

    float d = DOOM_dist2D(DOOM_playerX, DOOM_playerY, DOOM_enemies[i].x, DOOM_enemies[i].y);
    if (d < 0.55f) {
      DOOM_lastDamageTime = millis();

      if (DOOM_shieldActive) {
        DOOM_shieldActive = false;
        DOOM_shieldUntil = 0;
        return;
      }

      DOOM_playerHealth--;
      if (DOOM_playerHealth <= 0) DOOM_startNewRound();
      return;
    }
  }
}

void DOOM_renderWalls() {
  gfx.fillSprite(TFT_BLACK);
  gfx.fillRect(0, 0, APP_SCREEN_W, APP_SCREEN_H / 2, DOOM_COLOR_SKY);
  gfx.fillRect(0, APP_SCREEN_H / 2, APP_SCREEN_W, APP_SCREEN_H / 2, DOOM_COLOR_FLOOR);

  for (int x = 0; x < APP_SCREEN_W; x += DOOM_COLUMN_W) {
    float rayAngle = DOOM_playerAngle - (DOOM_FOV * 0.5f) + ((float)x / (float)APP_SCREEN_W) * DOOM_FOV;
    rayAngle = DOOM_normalizeAngle(rayAngle);

    float eyeX = cosf(rayAngle);
    float eyeY = sinf(rayAngle);

    float dist = 0.0f;
    bool hit = false;
    bool cornerEdge = false;

    while (!hit && dist < DOOM_MAX_DEPTH) {
      dist += 0.045f;

      float worldX = DOOM_playerX + eyeX * dist;
      float worldY = DOOM_playerY + eyeY * dist;

      int testX = (int)worldX;
      int testY = (int)worldY;

      if (testX < 0 || testX >= DOOM_MAP_W || testY < 0 || testY >= DOOM_MAP_H) {
        hit = true;
        dist = DOOM_MAX_DEPTH;
      } else if (DOOM_worldMap[testY][testX] == 1) {
        hit = true;

        float localX = worldX - testX;
        float localY = worldY - testY;

        bool nearLeft   = (localX < 0.03f);
        bool nearRight  = (localX > 0.97f);
        bool nearTop    = (localY < 0.03f);
        bool nearBottom = (localY > 0.97f);

        if ((nearLeft && nearTop) ||
            (nearRight && nearTop) ||
            (nearLeft && nearBottom) ||
            (nearRight && nearBottom)) {
          cornerEdge = true;
        }
      }
    }

    float correctedDist = dist * cosf(rayAngle - DOOM_playerAngle);
    if (correctedDist < 0.1f) correctedDist = 0.1f;

    DOOM_depthBuffer[x / DOOM_COLUMN_W] = correctedDist;

    int wallHeight = (int)(APP_SCREEN_H / correctedDist);
    int ceiling = (APP_SCREEN_H / 2) - (wallHeight / 2);
    int floorY = (APP_SCREEN_H / 2) + (wallHeight / 2);

    if (ceiling < 0) ceiling = 0;
    if (floorY >= APP_SCREEN_H) floorY = APP_SCREEN_H - 1;

    uint16_t wallColor = cornerEdge ? DOOM_COLOR_EDGE : DOOM_COLOR_WALL;

    if (floorY > ceiling) gfx.fillRect(x, ceiling, DOOM_COLUMN_W, floorY - ceiling, wallColor);
  }
}

void DOOM_renderEnemies() {
  int order[DOOM_MAX_ENEMIES];
  float distArr[DOOM_MAX_ENEMIES];

  for (int i = 0; i < DOOM_MAX_ENEMIES; i++) {
    order[i] = i;
    distArr[i] = DOOM_enemies[i].alive ? DOOM_dist2D(DOOM_playerX, DOOM_playerY, DOOM_enemies[i].x, DOOM_enemies[i].y) : -1.0f;
  }

  for (int i = 0; i < DOOM_MAX_ENEMIES - 1; i++) {
    for (int j = i + 1; j < DOOM_MAX_ENEMIES; j++) {
      if (distArr[order[j]] > distArr[order[i]]) {
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
      }
    }
  }

  for (int n = 0; n < DOOM_MAX_ENEMIES; n++) {
    int i = order[n];
    if (!DOOM_enemies[i].alive) continue;

    float dx = DOOM_enemies[i].x - DOOM_playerX;
    float dy = DOOM_enemies[i].y - DOOM_playerY;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < 0.1f || dist > DOOM_MAX_DEPTH) continue;

    float enemyAngle = atan2f(dy, dx);
    float relAngle = DOOM_angleDiff(enemyAngle, DOOM_playerAngle);

    if (fabs(relAngle) > (DOOM_FOV * 0.6f)) continue;

    float correctedDist = dist * cosf(relAngle);
    if (correctedDist <= 0.1f) continue;

    int spriteScreenX = (int)((0.5f + relAngle / DOOM_FOV) * APP_SCREEN_W);
    int spriteH = (int)(APP_SCREEN_H / correctedDist);
    int spriteW = spriteH / 2;

    int top = APP_SCREEN_H / 2 - spriteH / 2;
    int left = spriteScreenX - spriteW / 2;

    if (spriteH < 4 || spriteW < 2) continue;

    for (int sx = 0; sx < spriteW; sx++) {
      int screenX = left + sx;
      if (screenX < 0 || screenX >= APP_SCREEN_W) continue;

      int rayIdx = screenX / DOOM_COLUMN_W;
      if (rayIdx < 0 || rayIdx >= DOOM_NUM_RAYS) continue;
      if (correctedDist >= DOOM_depthBuffer[rayIdx]) continue;

      for (int sy = 0; sy < spriteH; sy++) {
        int screenY = top + sy;
        if (screenY < 0 || screenY >= APP_SCREEN_H) continue;

        bool drawPixel = false;
        uint16_t c = DOOM_COLOR_ENEMY_1;

        if (sy < spriteH / 3) {
          int cx = spriteW / 2;
          int cy = spriteH / 6;
          int rx = sx - cx;
          int ry = sy - cy;
          if ((rx * rx) * 4 + (ry * ry) * 6 < (spriteW * spriteW) / 3) {
            drawPixel = true;
            c = DOOM_COLOR_ENEMY_2;
          }
        }

        if (sy >= spriteH / 4 && sy < spriteH - spriteH / 6 &&
            sx > spriteW / 5 && sx < spriteW - spriteW / 5) {
          drawPixel = true;
          c = DOOM_COLOR_ENEMY_1;
        }

        if (sy > spriteH / 9 && sy < spriteH / 5) {
          if ((sx > spriteW / 3 && sx < spriteW / 3 + 2) ||
              (sx > (spriteW * 2) / 3 - 2 && sx < (spriteW * 2) / 3 + 1)) {
            drawPixel = true;
            c = TFT_RED;
          }
        }

        if (drawPixel) gfx.drawPixel(screenX, screenY, c);
      }
    }
  }
}

void DOOM_renderBullets() {
  for (int i = 0; i < DOOM_MAX_BULLETS; i++) {
    if (!DOOM_bullets[i].active) continue;

    float dx = DOOM_bullets[i].x - DOOM_playerX;
    float dy = DOOM_bullets[i].y - DOOM_playerY;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist <= 0.05f) continue;

    float ang = atan2f(dy, dx);
    float relAngle = DOOM_angleDiff(ang, DOOM_playerAngle);

    if (fabs(relAngle) > DOOM_FOV * 0.6f) continue;

    float correctedDist = dist * cosf(relAngle);
    if (correctedDist <= 0.05f) continue;

    int screenX = (int)((0.5f + relAngle / DOOM_FOV) * APP_SCREEN_W);
    int size = (int)(10.0f / correctedDist);
    if (size < 2) size = 2;
    if (size > 8) size = 8;

    int screenY = APP_SCREEN_H / 2;

    if (screenX >= 0 && screenX < APP_SCREEN_W) {
      int rayIdx = screenX / DOOM_COLUMN_W;
      if (rayIdx >= 0 && rayIdx < DOOM_NUM_RAYS) {
        if (correctedDist < DOOM_depthBuffer[rayIdx]) gfx.fillCircle(screenX, screenY, size, DOOM_COLOR_BULLET);
      }
    }
  }
}

void DOOM_renderPowerUpSprite() {
  if (!DOOM_powerUp.active) return;

  float dx = DOOM_powerUp.x - DOOM_playerX;
  float dy = DOOM_powerUp.y - DOOM_playerY;
  float dist = sqrtf(dx * dx + dy * dy);

  if (dist < 0.1f || dist > DOOM_MAX_DEPTH) return;

  float spriteAngle = atan2f(dy, dx);
  float relAngle = DOOM_angleDiff(spriteAngle, DOOM_playerAngle);

  if (fabs(relAngle) > (DOOM_FOV * 0.62f)) return;

  float correctedDist = dist * cosf(relAngle);
  if (correctedDist <= 0.1f) return;

  int spriteScreenX = (int)((0.5f + relAngle / DOOM_FOV) * APP_SCREEN_W);
  int spriteH = (int)(APP_SCREEN_H / correctedDist);
  int spriteW = spriteH / 2;

  if (spriteH < 4 || spriteW < 4) return;

  int top = APP_SCREEN_H / 2 - spriteH / 2;
  int left = spriteScreenX - spriteW / 2;

  uint16_t c = DOOM_COLOR_POWERUP_TRIPLE;
  if (DOOM_powerUp.type == DOOM_POWERUP_SHIELD) c = DOOM_COLOR_POWERUP_SHIELD;
  if (DOOM_powerUp.type == DOOM_POWERUP_LIFE) c = DOOM_COLOR_POWERUP_LIFE;

  for (int sx = 0; sx < spriteW; sx++) {
    int screenX = left + sx;
    if (screenX < 0 || screenX >= APP_SCREEN_W) continue;

    int rayIdx = screenX / DOOM_COLUMN_W;
    if (rayIdx < 0 || rayIdx >= DOOM_NUM_RAYS) continue;
    if (correctedDist >= DOOM_depthBuffer[rayIdx]) continue;

    for (int sy = 0; sy < spriteH; sy++) {
      int screenY = top + sy;
      if (screenY < 0 || screenY >= APP_SCREEN_H) continue;

      int cx = spriteW / 2;
      int cy = spriteH / 2;
      int rx = sx - cx;
      int ry = sy - cy;

      bool draw = false;
      uint16_t pc = c;

      if ((rx * rx) * 4 + (ry * ry) * 4 < (spriteW * spriteW) / 2) draw = true;
      if (!draw) continue;

      if (DOOM_powerUp.type == DOOM_POWERUP_TRIPLE) {
        if (abs(sx - cx) <= 1 || abs(sx - (cx - spriteW / 4)) <= 1 || abs(sx - (cx + spriteW / 4)) <= 1) pc = TFT_WHITE;
      }
      else if (DOOM_powerUp.type == DOOM_POWERUP_SHIELD) {
        if ((rx * rx) * 6 + (ry * ry) * 6 < (spriteW * spriteW) / 8) pc = TFT_WHITE;
      }
      else if (DOOM_powerUp.type == DOOM_POWERUP_LIFE) {
        if ((abs(sx - cx) <= 1) || (abs(sy - cy) <= 1)) pc = TFT_WHITE;
      }

      gfx.drawPixel(screenX, screenY, pc);
    }
  }
}

void DOOM_renderHealthBar() {
  int startX = 8;
  int startY = 8;
  int boxW = 14;
  int boxH = 8;
  int gap = 3;

  for (int i = 0; i < DOOM_MAX_HEALTH; i++) {
    int x = startX + i * (boxW + gap);

    if (i < DOOM_playerHealth) gfx.fillRect(x, startY, boxW, boxH, TFT_RED);
    else gfx.drawRect(x, startY, boxW, boxH, TFT_DARKGREY);
  }
}

void DOOM_renderWeapon() {
  int gunW = 56;
  int gunH = 22;
  int gunX = (APP_SCREEN_W - gunW) / 2;
  int gunY = APP_SCREEN_H - gunH;

  gfx.fillRect(gunX, gunY, gunW, gunH, DOOM_COLOR_GUN_WOOD);
  gfx.fillRect(gunX, gunY + 12, 18, 10, DOOM_COLOR_GUN_WOOD_DARK);
  gfx.fillRect(gunX + 38, gunY + 12, 18, 10, DOOM_COLOR_GUN_WOOD_DARK);
  gfx.fillRect(gunX + 20, gunY - 8, 16, 10, DOOM_COLOR_GUN_METAL);
  gfx.drawFastHLine(gunX + 5, gunY + 5, gunW - 10, DOOM_COLOR_GUN_WOOD_DARK);

  if (millis() - DOOM_lastShotTime < 50) {
    gfx.fillTriangle(APP_SCREEN_W / 2 - 5, APP_SCREEN_H - 24,
                     APP_SCREEN_W / 2 + 5, APP_SCREEN_H - 24,
                     APP_SCREEN_W / 2, APP_SCREEN_H - 42,
                     TFT_YELLOW);
  }

  if (DOOM_tripleShotActive) {
    gfx.drawFastVLine(APP_SCREEN_W / 2 - 7, APP_SCREEN_H - 36, 12, DOOM_COLOR_POWERUP_TRIPLE);
    gfx.drawFastVLine(APP_SCREEN_W / 2,     APP_SCREEN_H - 40, 16, DOOM_COLOR_POWERUP_TRIPLE);
    gfx.drawFastVLine(APP_SCREEN_W / 2 + 7, APP_SCREEN_H - 36, 12, DOOM_COLOR_POWERUP_TRIPLE);
  }
}

void DOOM_renderHUD() {
  gfx.drawFastHLine(APP_SCREEN_W / 2 - 4, APP_SCREEN_H / 2, 9, TFT_RED);
  gfx.drawFastVLine(APP_SCREEN_W / 2, APP_SCREEN_H / 2 - 4, 9, TFT_RED);

  DOOM_renderWeapon();
  DOOM_renderHealthBar();

  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextSize(1);

  gfx.setCursor(APP_SCREEN_W - 62, 8);
  gfx.print("R:");
  gfx.print(DOOM_roundNumber);

  gfx.setCursor(APP_SCREEN_W - 62, 20);
  gfx.print("K:");
  gfx.print(DOOM_killCount);

  if (millis() < DOOM_roundMessageUntil) {
    gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
    gfx.setCursor(78, 8);
    gfx.print("NEW ROUND");
  }

  if (DOOM_shieldActive) {
    gfx.setTextColor(DOOM_COLOR_POWERUP_SHIELD, TFT_BLACK);
    gfx.setCursor(78, 20);
    gfx.print("SH");
  } else if (DOOM_tripleShotActive) {
    gfx.setTextColor(DOOM_COLOR_POWERUP_TRIPLE, TFT_BLACK);
    gfx.setCursor(78, 20);
    gfx.print("3W");
  }

  if (millis() - DOOM_lastDamageTime < 120) {
    gfx.drawRect(0, 0, APP_SCREEN_W, APP_SCREEN_H, TFT_RED);
    gfx.drawRect(1, 1, APP_SCREEN_W - 2, APP_SCREEN_H - 2, TFT_RED);
  }

  if (DOOM_shieldActive) {
    gfx.drawRect(2, 2, APP_SCREEN_W - 4, APP_SCREEN_H - 4, DOOM_COLOR_POWERUP_SHIELD);
  }
}

void DOOM_renderFrame() {
  DOOM_renderWalls();
  DOOM_renderEnemies();
  DOOM_renderPowerUpSprite();
  DOOM_renderBullets();
  DOOM_renderHUD();
  gfx.pushSprite(0, 0);
}

void DOOM_renderMenu() {
  gfx.fillSprite(TFT_BLACK);

  gfx.setTextSize(2);
  gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
  gfx.setCursor(58, 16);
  gfx.print("MINI DOOM");

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setCursor(54, 42);
  gfx.print("2/8 = MENU   # = SELECT");

  const char* item0 = "START GAME";
  const char* item1 = "CONTROLS";

  int y0 = 68;
  int y1 = 92;

  if (DOOM_menuIndex == 0) {
    gfx.fillRoundRect(52, y0 - 4, 136, 18, 4, TFT_DARKGREY);
    gfx.setTextColor(TFT_BLACK, TFT_DARKGREY);
  } else {
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  gfx.setCursor(88, y0);
  gfx.print(item0);

  if (DOOM_menuIndex == 1) {
    gfx.fillRoundRect(52, y1 - 4, 136, 18, 4, TFT_DARKGREY);
    gfx.setTextColor(TFT_BLACK, TFT_DARKGREY);
  } else {
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  gfx.setCursor(94, y1);
  gfx.print(item1);

  if (DOOM_menuIndex == 1) {
    gfx.setTextColor(TFT_CYAN, TFT_BLACK);
    gfx.setCursor(18, 112);
    gfx.print("Move: 2/8   Turn: 4/6");
    gfx.setCursor(20, 122);
    gfx.print("Shoot: #   Gold/Blue/Green Pickups");
  }

  gfx.pushSprite(0, 0);
}

void DOOM_enter() {
  DOOM_resetWholeGame();
  DOOM_initEnemies();
  DOOM_gameState = DOOM_STATE_MENU;
  DOOM_menuIndex = 0;
  DOOM_lastRenderTime = 0;
}

void DOOM_loop() {
  if (DOOM_gameState == DOOM_STATE_MENU) {
    DOOM_handleMenuInput();
    if (appMode != APP_DOOM) return;

    if (millis() - DOOM_lastRenderTime >= DOOM_renderInterval) {
      DOOM_lastRenderTime = millis();
      DOOM_renderMenu();
    }
    return;
  }

  DOOM_handleInput();

  if (DOOM_backPressed) {
    appMode = APP_LAUNCHER;
    return;
  }

  DOOM_movePlayer();
  DOOM_tryShoot();
  DOOM_updateBullets();
  DOOM_updateEnemies();
  DOOM_updatePowerupTimers();
  DOOM_updatePowerUpPickup();
  DOOM_checkPlayerDamage();

  if (millis() - DOOM_lastRenderTime >= DOOM_renderInterval) {
    DOOM_lastRenderTime = millis();
    DOOM_renderFrame();
  }
}

// =====================================================
// SETUP / LOOP
// =====================================================
void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  keypad.setHoldTime(120);
  keypad.setDebounceTime(20);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  gfx.setColorDepth(16);
  gfx.createSprite(APP_SCREEN_W, APP_SCREEN_H);
  gfx.fillSprite(TFT_BLACK);
  gfx.setTextWrap(false);

  DOOM_COLOR_SKY            = tft.color565(60, 145, 245);
  DOOM_COLOR_FLOOR          = tft.color565(70, 170, 60);
  DOOM_COLOR_WALL           = tft.color565(135, 145, 155);
  DOOM_COLOR_EDGE           = tft.color565(40, 40, 40);
  DOOM_COLOR_ENEMY_1        = tft.color565(0, 220, 0);
  DOOM_COLOR_ENEMY_2        = tft.color565(0, 140, 0);
  DOOM_COLOR_BULLET         = tft.color565(255, 255, 120);
  DOOM_COLOR_GUN_WOOD       = tft.color565(139, 90, 43);
  DOOM_COLOR_GUN_WOOD_DARK  = tft.color565(92, 58, 24);
  DOOM_COLOR_GUN_METAL      = tft.color565(120, 120, 120);
  DOOM_COLOR_POWERUP_TRIPLE = tft.color565(255, 210, 0);
  DOOM_COLOR_POWERUP_SHIELD = tft.color565(0, 120, 255);
  DOOM_COLOR_POWERUP_LIFE   = tft.color565(0, 220, 80);

  drawLauncher();
}

void loop() {
  if (appMode == APP_LAUNCHER) {
    updateLauncher();
    drawLauncher();
    delay(16);
    return;
  }

  if (appMode == APP_SPACE) {
    SI_loop();
    return;
  }

  if (appMode == APP_PONG) {
    PONG_loop();
    return;
  }

  if (appMode == APP_DOOM) {
    DOOM_loop();
    return;
  }
}
