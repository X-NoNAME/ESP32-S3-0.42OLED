#include <Arduino.h>
#include <U8g2lib.h>
#include <EEPROM.h>  // ✅ Добавляем поддержку EEPROM

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define SDA_PIN 5
#define SCL_PIN 6
#define BUTTON_PIN 0
#define LED_PIN 8

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE);  // ← перевернули экран

// Параметры игры
int birdY = 20;
int birdVelocity = 0;
int gravity = 1;
int flapPower = -4;

struct Pipe {
  int x;
  int gapY;
  int gapHeight = 16;
  bool passed = false;
};

Pipe pipes[3];
int pipeSpeed = 2;
int pipeSpacing = 30;
int nextPipeX = 72;

int score = 0;
int bestScore = 0;  // 🌟 Лучший счёт
bool gameOver = false;
bool showBestBeforeStart = false;  // Показать лучший счёт перед стартом
unsigned long showBestUntil = 0;

unsigned long lastUpdate = 0;
const int frameDelay = 100;

// Адрес в EEPROM для хранения bestScore (можно любой, например 0)
#define EEPROM_ADDR 0

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setFont(u8g2_font_5x7_tr);

  // Инициализация EEPROM
  EEPROM.begin(64);  // 64 байта достаточно
  bestScore = EEPROM.read(EEPROM_ADDR);
  // Защита от "мусора" — если > 255, сбросим в 0
  if (bestScore > 100) bestScore = 0;

  resetGame();
  showBestBeforeStart = true;
  showBestUntil = millis() + 2000;  // Показать 2 секунды
}

void resetGame() {
  birdY = 20;
  birdVelocity = 0;
  score = 0;
  gameOver = false;

  for (int i = 0; i < 3; i++) {
    pipes[i].x = nextPipeX + i * pipeSpacing;
    pipes[i].gapY = random(5, 25);
    pipes[i].passed = false;
  }
}

void loop() {
  unsigned long now = millis();

  // Показ лучшего счёта перед стартом
  if (showBestBeforeStart) {
    if (now < showBestUntil) {
      drawBestScoreScreen();
      return;
    } else {
      showBestBeforeStart = false;
    }
  }

  if (now - lastUpdate < frameDelay) return;
  lastUpdate = now;

  if (gameOver) {
    // Обновляем bestScore при необходимости
    if (score > bestScore) {
      bestScore = score;
      EEPROM.write(EEPROM_ADDR, bestScore);
      EEPROM.commit();  // 💾 Сохраняем в память (для ESP)
    }

    if (digitalRead(BUTTON_PIN) == HIGH) {
      delay(150);  // антидребезг
      resetGame();
      showBestBeforeStart = true;
      showBestUntil = millis() + 2000;
    }
    drawGameOver();
    return;
  }

  // Управление
  static unsigned long lastPress = 0;
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastPress > 150) {
      birdVelocity = flapPower;
      lastPress = millis();
    }
  }

  // Физика птицы
  birdVelocity += gravity;
  birdY += birdVelocity;

  if (birdY < 0) birdY = 0;
  if (birdY > 36) {
    gameOver = true;
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
  }

  // Обновление труб
  for (int i = 0; i < 3; i++) {
    pipes[i].x -= pipeSpeed;

    // Проверка столкновения
    if (pipes[i].x < 6 && pipes[i].x + 4 > 2) {
      if (birdY < pipes[i].gapY || birdY + 4 > pipes[i].gapY + pipes[i].gapHeight) {
        gameOver = true;
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
      }
    }

    // Подсчёт очков
    if (!pipes[i].passed && pipes[i].x < 2) {
      pipes[i].passed = true;
      score++;
    }

    // Респавн
    if (pipes[i].x < -6) {
      pipes[i].x = nextPipeX;
      pipes[i].gapY = random(5, 25);
      pipes[i].passed = false;
    }
  }

  drawGame();
}

void drawGame() {
  u8g2.clearBuffer();
  u8g2.drawBox(2, birdY, 4, 4);

  for (int i = 0; i < 3; i++) {
    u8g2.drawBox(pipes[i].x, 0, 4, pipes[i].gapY);
    u8g2.drawBox(pipes[i].x, pipes[i].gapY + pipes[i].gapHeight, 4, 40 - (pipes[i].gapY + pipes[i].gapHeight));
  }

  u8g2.setCursor(0, 8);
  u8g2.print("Score:");
  u8g2.setCursor(40, 8);
  u8g2.print(score);

  u8g2.sendBuffer();
}

void drawGameOver() {
  u8g2.clearBuffer();
  u8g2.setCursor(5, 15);
  u8g2.print("Game Over");
  u8g2.setCursor(5, 25);
  u8g2.print("Score:");
  u8g2.setCursor(40, 25);
  u8g2.print(score);
  u8g2.setCursor(5, 35);
  u8g2.print("Best:");
  u8g2.setCursor(40, 35);
  u8g2.print(bestScore);
  u8g2.sendBuffer();
}

void drawBestScoreScreen() {
  u8g2.clearBuffer();
  u8g2.setCursor(5, 15);
  u8g2.print("Flappy Bird");
  u8g2.setCursor(5, 25);
  u8g2.print("Best:");
  u8g2.setCursor(40, 25);
  u8g2.print(bestScore);
  u8g2.setCursor(0, 35);
  u8g2.print("Press to start");
  u8g2.sendBuffer();
}