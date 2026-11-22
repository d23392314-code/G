// MOCHI ESP32 + OLED SSD1306 0.96" (128x64, I2C)
// Wiring: SDA -> GPIO21, SCL -> GPIO22, VCC -> 3V3, GND -> GND
// Tombol: BOOT bawaan board (GPIO0) -> ganti mode wajah <-> jam

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== ISI SENDIRI ======
#define WIFI_SSID  "NAMA_WIFI"
#define WIFI_PASS  "PASSWORD_WIFI"
#define GMT_OFFSET 25200          // WIB = 7*3600 (WITA 28800, WIT 32400)
// =========================

#define OLED_ADDR 0x3C
#define SDA_PIN   21
#define SCL_PIN   22
#define BTN_PIN   0

#define FACE_MS   40000UL         // lama tampil wajah sebelum jam muncul
#define CLOCK_MS  8000UL          // lama jam tampil

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum Exp { E_NORMAL, E_HAPPY, E_SAD, E_ANGRY, E_SLEEPY, E_LOVE, E_SURPRISED, E_WINK, E_DIZZY, E_COUNT };

const int EYE_W = 26, EYE_H = 32, LX_C = 36, RX_C = 92, EY = 24;

const char* HARI[]  = {"Min","Sen","Sel","Rab","Kam","Jum","Sab"};
const char* BULAN[] = {"Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"};

bool synced = false;
unsigned long fallbackSec = 0;

Exp curExp = E_NORMAL;
bool showClock = false;
unsigned long modeStart = 0, expUntil = 0, nextLook = 0, nextBlink = 2000, blinkStart = 0;
bool blinking = false;
float curLx = 0, curLy = 0, tgtLx = 0, tgtLy = 0;
bool lastBtn = HIGH;
unsigned long lastBtnTime = 0;

// ---------- helper gambar ----------
void eye(int cx, int cy, int h) {
  if (h < 2) h = 2;
  int r = min(8, h / 2);
  display.fillRoundRect(cx - EYE_W / 2, cy - h / 2, EYE_W, h, r, WHITE);
}

void smile(int cx, int y, int w) {
  for (int i = -w; i <= w; i++) {
    int yy = y + w / 2 - (i * i) / (2 * w);
    display.fillRect(cx + i, yy, 2, 2, WHITE);
  }
}

void frown(int cx, int y, int w) {
  for (int i = -w; i <= w; i++) {
    int yy = y - w / 2 + (i * i) / (2 * w);
    display.fillRect(cx + i, yy, 2, 2, WHITE);
  }
}

void heart(int cx, int cy, int s) {
  int r = s / 2;
  display.fillCircle(cx - r, cy, r, WHITE);
  display.fillCircle(cx + r, cy, r, WHITE);
  display.fillTriangle(cx - s, cy + 1, cx + s, cy + 1, cx, cy + s + 3, WHITE);
}

void caretEye(int cx, int cy) {
  for (int k = 0; k < 3; k++) {
    display.drawLine(cx - 11, cy + 6 + k, cx, cy - 6 + k, WHITE);
    display.drawLine(cx, cy - 6 + k, cx + 11, cy + 6 + k, WHITE);
  }
}

void xEye(int cx, int cy) {
  for (int k = 0; k < 2; k++) {
    display.drawLine(cx - 9 + k, cy - 9, cx + 9 + k, cy + 9, WHITE);
    display.drawLine(cx - 9 + k, cy + 9, cx + 9 + k, cy - 9, WHITE);
  }
}

// ---------- render wajah ----------
void render(Exp e, int lx, int ly, int h) {
  display.clearDisplay();
  int a = LX_C + lx, b = RX_C + lx, cy = EY + ly;
  unsigned long t = millis();

  switch (e) {
    case E_NORMAL:
      eye(a, cy, h); eye(b, cy, h);
      smile(64 + lx / 2, 50, 6);
      break;

    case E_HAPPY:
      caretEye(a, cy); caretEye(b, cy);
      smile(64, 48, 10);
      break;

    case E_SAD:
      eye(a, cy, EYE_H); eye(b, cy, EYE_H);
      display.fillTriangle(a - 14, cy - 17, a + 14, cy - 17, a - 14, cy - 2, BLACK);
      display.fillTriangle(b - 14, cy - 17, b + 14, cy - 17, b + 14, cy - 2, BLACK);
      display.fillCircle(b + 8, cy + 20 + (t / 150) % 8, 2, WHITE);
      frown(64, 54, 6);
      break;

    case E_ANGRY:
      eye(a, cy, EYE_H); eye(b, cy, EYE_H);
      display.fillTriangle(a - 14, cy - 17, a + 14, cy - 17, a + 14, cy - 2, BLACK);
      display.fillTriangle(b - 14, cy - 17, b + 14, cy - 17, b - 14, cy - 2, BLACK);
      display.drawFastHLine(56, 54, 16, WHITE);
      display.drawFastHLine(56, 55, 16, WHITE);
      break;

    case E_SLEEPY: {
      eye(a, cy + 8, 4); eye(b, cy + 8, 4);
      int r = 2 + (t / 700) % 2;
      display.drawCircle(64, 52, r, WHITE);
      display.setTextColor(WHITE);
      int p = (t / 500) % 3;
      display.setTextSize(1);
      display.setCursor(104 + p * 4, 20 - p * 6);
      display.print("Z");
      display.setTextSize(2);
      display.setCursor(110, 2);
      if (p >= 1) display.print("z");
      break;
    }

    case E_LOVE: {
      int s = 9 + (t / 180) % 2 * 2;
      heart(a, cy - 6, s); heart(b, cy - 6, s);
      smile(64, 50, 8);
      break;
    }

    case E_SURPRISED:
      display.fillCircle(a, cy, 15, WHITE);
      display.fillCircle(b, cy, 15, WHITE);
      display.fillCircle(a + lx / 2, cy + ly / 2, 5, BLACK);
      display.fillCircle(b + lx / 2, cy + ly / 2, 5, BLACK);
      display.drawCircle(64, 54, 4, WHITE);
      break;

    case E_WINK:
      eye(a, cy, h);
      for (int k = 0; k < 3; k++) display.drawLine(b - 11, cy + k, b + 11, cy + k, WHITE);
      smile(64, 49, 8);
      break;

    case E_DIZZY: {
      int wob = (int)(3 * sin(t / 120.0));
      xEye(a + wob, cy); xEye(b + wob, cy);
      for (int i = -10; i <= 10; i++) {
        int yy = 54 + (int)(3 * sin(i * 0.8 + t / 100.0));
        display.fillRect(64 + i, yy, 2, 2, WHITE);
      }
      break;
    }

    default: break;
  }
  display.display();
}

// ---------- waktu ----------
bool getNow(int &h, int &m, int &s, int &wd, int &d, int &mo) {
  if (synced) {
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
      h = ti.tm_hour; m = ti.tm_min; s = ti.tm_sec;
      wd = ti.tm_wday; d = ti.tm_mday; mo = ti.tm_mon;
      return true;
    }
  }
  unsigned long sec = fallbackSec + millis() / 1000;
  h = (sec / 3600) % 24; m = (sec / 60) % 60; s = sec % 60;
  wd = d = mo = 0;
  return false;
}

void drawClock() {
  int h, m, s, wd, d, mo;
  bool dateOk = getNow(h, m, s, wd, d, mo);
  display.clearDisplay();
  display.setTextColor(WHITE);

  char buf[8];
  bool colon = (millis() / 500) % 2 == 0;
  snprintf(buf, sizeof(buf), colon ? "%02d:%02d" : "%02d %02d", h, m);
  display.setTextSize(4);
  display.setCursor(4, 6);
  display.print(buf);

  display.drawRect(4, 44, 120, 4, WHITE);
  display.fillRect(4, 44, 2 * s, 4, WHITE);

  display.setTextSize(1);
  char dbuf[20];
  if (dateOk) snprintf(dbuf, sizeof(dbuf), "%s, %d %s", HARI[wd], d, BULAN[mo]);
  else snprintf(dbuf, sizeof(dbuf), "(offline)");
  int w = strlen(dbuf) * 6;
  display.setCursor((128 - w) / 2, 54);
  display.print(dbuf);
  display.display();
}

// ---------- logika ----------
void pickExp() {
  Exp n;
  do {
    n = (random(100) < 40) ? E_NORMAL : (Exp)random(E_COUNT);
  } while (n == curExp && n != E_NORMAL);
  curExp = n;
  unsigned long now = millis();
  expUntil = now + (n == E_NORMAL ? random(3000, 7000) : random(2500, 5000));
  if (n == E_DIZZY) expUntil = now + 2500;
}

void checkButton() {
  bool b = digitalRead(BTN_PIN);
  if (b == LOW && lastBtn == HIGH && millis() - lastBtnTime > 250) {
    lastBtnTime = millis();
    showClock = !showClock;
    modeStart = millis();
    if (!showClock) pickExp();
  }
  lastBtn = b;
}

void showText(const char* l1, const char* l2 = "") {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20); display.println(l1);
  display.setCursor(0, 34); display.println(l2);
  display.display();
}

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;) delay(1000);
  }
  display.clearDisplay();
  display.display();
  randomSeed(esp_random());

  // jam cadangan dari waktu compile (kalau WiFi gagal)
  fallbackSec = ((__TIME__[0]-'0')*10 + (__TIME__[1]-'0')) * 3600UL
              + ((__TIME__[3]-'0')*10 + (__TIME__[4]-'0')) * 60UL
              + ((__TIME__[6]-'0')*10 + (__TIME__[7]-'0'));

  showText("Mochi bangun...", "nyambung WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long st = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - st < 12000) delay(200);

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET, 0, "pool.ntp.org", "time.google.com");
    struct tm ti;
    st = millis();
    while (!getLocalTime(&ti, 200) && millis() - st < 8000) { }
    synced = getLocalTime(&ti, 0);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  showText(synced ? "Jam sinkron :)" : "WiFi gagal, jam offline");
  delay(1200);

  modeStart = millis();
  pickExp();
}

void loop() {
  unsigned long now = millis();
  checkButton();

  if (showClock) {
    drawClock();
    if (now - modeStart > CLOCK_MS) {
      showClock = false;
      modeStart = now;
      pickExp();
    }
    delay(60);
    return;
  }

  if (now - modeStart > FACE_MS) {
    showClock = true;
    modeStart = now;
    return;
  }

  if (now >= expUntil) pickExp();

  if (now >= nextLook) {
    tgtLx = random(-8, 9);
    tgtLy = random(-4, 5);
    nextLook = now + random(800, 2500);
  }
  curLx += (tgtLx - curLx) / 3.0;
  curLy += (tgtLy - curLy) / 3.0;

  if (!blinking && now >= nextBlink) {
    blinking = true;
    blinkStart = now;
  }
  int h = EYE_H;
  if (blinking) {
    unsigned long dt = now - blinkStart;
    if (dt >= 240) {
      blinking = false;
      nextBlink = now + random(2000, 5000);
    } else if (dt < 120) {
      h = EYE_H - (EYE_H - 3) * dt / 120;
    } else {
      h = 3 + (EYE_H - 3) * (dt - 120) / 120;
    }
  }

  render(curExp, (int)curLx, (int)curLy, h);
  delay(15);
}
