#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>
#include <Ultrasonic.h>
#include <NewTone.h>

//----------------------------------------------------------------------
// led matrix
//----------------------------------------------------------------------

#define DATA 3
#define SHIFT 4
#define STORE 5
enum Direct {left, down, right, up};
byte pic[4][8] = 
{
  {112,136,128,136,112,0,0,0},  // left
  {80,136,136,136,112,0,0,0},   // down
  {112,136,8,136,112,0,0,0},    // right
  {112,136,136,136,80,0,0,0}    // up
};
TaskHandle_t LEDM;
void Task_LEDM( void *pvParameters );

//----------------------------------------------------------------------
// ultrasonic 
//----------------------------------------------------------------------
#define TRIG 12
#define ECHO 13
Ultrasonic ultrasonic(TRIG, ECHO); // (Trig PIN,Echo PIN)
int distance;
TaskHandle_t ULTRA;
void Task_ULTRA( void *pvParameters );

//----------------------------------------------------------------------
// IR remote
//----------------------------------------------------------------------
#define RECV_PIN 2
IRrecv irrecv(RECV_PIN);
decode_results results;
char ch = 'z';

//----------------------------------------------------------------------
// piezo buzzer
//----------------------------------------------------------------------
#define BUZZER 9
#define FREQ 880
#define DURA 450

//----------------------------------------------------------------------
// lcd
//----------------------------------------------------------------------
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
TaskHandle_t LCD;
void Task_LCD( void *pvParameters );

//----------------------------------------------------------------------
// setup
//----------------------------------------------------------------------
enum State {standby, measure, game, over};
State state;
byte game_round;
byte seq[3];
byte Win, Loss;
#define DISTANCE 250

void setup() {
  randomSeed(analogRead(A0));
  Serial.begin(9600);

  //----------led matrix----------
  pinMode(DATA, OUTPUT);
  pinMode(SHIFT, OUTPUT);
  pinMode(STORE, OUTPUT);

  shiftOut(DATA, SHIFT, MSBFIRST, 0);
  shiftOut(DATA, SHIFT, MSBFIRST, 0);
  digitalWrite(STORE, HIGH);
  digitalWrite(STORE, LOW);

  //----------ultrasonic----------

  //----------IR remote----------
  irrecv.enableIRIn(); // Start the receiver

  //----------piezo buzzer----------
  pinMode(BUZZER, OUTPUT);

  //----------lcd----------
  lcd.begin(16, 2);
  lcd.backlight();

  state = standby;
  attachInterrupt(0, presshandler, RISING);
  xTaskCreate(Task_LEDM,  (const portCHAR *)"LEDM",  90, NULL, 1, &LEDM);
  vTaskSuspend(LEDM);
  xTaskCreate(Task_ULTRA, (const portCHAR *)"ULTRA", 90, NULL, 1, &ULTRA);
  xTaskCreate(Task_LCD,   (const portCHAR *)"LCD",   108, NULL, 1, &LCD);
  vTaskStartScheduler();
}

void loop() {
}

void game_init() {
  while(1) {
    seq[0] = random(0, 4);
    seq[1] = random(0, 4);
    seq[2] = random(0, 4);
    if(seq[0] != seq[1] && seq[1] != seq[2])
      break;
  }
  game_round = 0; 
  Win = 0;
  Loss = 0;
}

//----------------------------------------------------------------------
// led matrix
//----------------------------------------------------------------------

void Task_LEDM(void *pvParameters) {
  (void) pvParameters;
  
  for(;;) {
    for (int i=0; i<8; i++) {
      shiftOut(DATA, SHIFT, LSBFIRST, ~pic[seq[game_round]][i]);
      shiftOut(DATA, SHIFT, LSBFIRST, 128 >> i);
      digitalWrite(STORE, HIGH);
      vTaskDelay(10/portTICK_PERIOD_MS);
      digitalWrite(STORE, LOW);
      vTaskDelay(10/portTICK_PERIOD_MS);
      if(ch != 'z') {
        if(ch == '2' && seq[game_round] == up || 
           ch == '4' && seq[game_round] == left ||
           ch == '6' && seq[game_round] == right ||
           ch == '8' && seq[game_round] == down
        ) {
          Win++;
        }
        else {
          Loss++;
        }
        game_round++;
        vTaskResume(LCD);
        if(game_round >= 3) {
          state = over;
          shiftOut(DATA, SHIFT, MSBFIRST, 0);
          shiftOut(DATA, SHIFT, MSBFIRST, 0);
          digitalWrite(STORE, HIGH);
          digitalWrite(STORE, LOW);
          vTaskResume(ULTRA);
          vTaskResume(LCD);
          vTaskSuspend(LEDM);
        }
        ch = 'z';
      }
    }
  }
}

//----------------------------------------------------------------------
// ultrasonic 
//----------------------------------------------------------------------

void Task_ULTRA(void *pvParameters) {
  (void) pvParameters;
  int count = 0;
  
  for(;;) {
    if(ch == '5') {
      ch = 'z';
      Serial.println(1);
      if(state == standby) {
        state = measure;
      }
    }
    if(state == measure) {
      distance = ultrasonic.read();
      if(distance > 300) distance = -1;
      vTaskResume(LCD);
      if(distance > DISTANCE) {
        count++;
      }
      else {
        count = 0;
      }
      vTaskDelay(1000/portTICK_PERIOD_MS);
      if(count >= 4) {
        count = 0;
        game_init();
        state = game;
        vTaskResume(LEDM);
        vTaskResume(LCD);
        vTaskSuspend(ULTRA);
      }
    }
  }
}

//----------------------------------------------------------------------
// IR remote
//----------------------------------------------------------------------

void presshandler() {
  while(irrecv.decode(&results)) {
    translateIR(); // Function to translate code
    irrecv.resume();    // Receive the next code 
    if(ch != 'z') {
      NewTone(BUZZER, FREQ, DURA);
    }
  }
}

char translateIR() 
{
  switch (results.value) {
    case 0xFF18E7:
      ch = '2';
      break;
    case 0xFF10EF:
      ch = '4';
      break;
    case 0xFF38C7:
      ch = '5';
      break;
    case 0xFF5AA5:
      ch = '6';
      break;
    case 0xFF4AB5:
      ch = '8';
      break;
    case 0xFFFFFF:
      break;
    default:
      ch = 'z';
  } 
  delay(500);
}

//----------------------------------------------------------------------
// lcd
//----------------------------------------------------------------------

void Task_LCD(void *pvParameters) {
  (void) pvParameters;
  
  for(;;) {
    lcd.clear();
    if(state == standby) {
      lcd.setCursor(0, 0);
      lcd.print("Press 5");
      lcd.setCursor(0, 1);
      lcd.print("to start");
    }
    else if(state == measure) {
      lcd.setCursor(0, 0);
      lcd.print("distance");
      lcd.setCursor(0, 1);
      lcd.print(distance);
      lcd.setCursor(4, 1);
      lcd.print("cm");
    }
    else if(state == game) {
      lcd.setCursor(0, 0);
      lcd.print("Measuring...");
    }
    else if(state == over) {
      lcd.setCursor(0, 0);
      if(Win >= 2) {
        lcd.print("Good Eye!");
      }
      else {
        lcd.print("nearsighted!");
      }
      lcd.setCursor(0, 1);
      lcd.print("5: Test Again.");
      state = standby;
    }
    vTaskSuspend(LCD);
  }
}
