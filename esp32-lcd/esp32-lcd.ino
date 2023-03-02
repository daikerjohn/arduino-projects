//www.diyusthad.com
#include <LiquidCrystal.h>

const int rs = 12;
const int en = 11;
const int d4 = 5;
const int d5 = 4;
const int d6 = 3;
const int d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  Serial.println("Done setup");
  lcd.print("Scrolling TExt Example");
}

void loop() {
  lcd.scrollDisplayLeft();
  delay(500);
}

/*
String Hello="Hello World!";

void loop() {
  lcd.clear(); //d
  delay(4000);//
  lcd.blink();
  lcd.setCursor(0, 0);
  delay(6000); //2000

  for(int i=0;i<Hello.length();i++){
    lcd.print(Hello.charAt(i));
    delay(400);
  }  
  delay(6000);//2000 
  lcd.noBlink();
  delay(3000);
  lcd.clear();
}
*/