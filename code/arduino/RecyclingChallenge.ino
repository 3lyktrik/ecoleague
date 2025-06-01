/***************************************************************
 *  3ο ΓΕΛ Τρικάλων - "Οδυσσέας Ελύτης"
 *  Ομάδα Ρομποτικής: Eco League
 *  Project: Διαγωνισμός Ανακύκλωσης
 *  Για τον 7ο Διαγωνισμό Ανοιχτών Τεχνολογιών στην Εκπαίδευση
 *	https://github.com/3lyktrik/ecoleague
 *
 *  Τρίκαλα - 2025
 ***************************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <WiFiS3.h>
#include "secrets.h"
#include "persons.h"
#include <ThingSpeak.h> // always include thingspeak header file after other header files and custom macros

#define RST_PIN   5
#define SS_PIN    10
#define BUZZER_PIN  6
#define LASER_PIN   7
#define LDR_PIN     A0
#define METAL_PIN   8
#define LOADCELL_DOUT_PIN 2
#define LOADCELL_SCK_PIN  3

LiquidCrystal_I2C lcd(0x27,16,2);   // οθόνη LCD με 2 γραμμές και 16 χαρακτήρες, στην I2C διεύθυνση 0x27

String tagID = "";     // για το UID της κάρτας RFID

MFRC522 mfrc522(SS_PIN, RST_PIN);   // αναγνώστης καρτών RFID   

int metalVal = 0;   // αισθητήρας μετάλλων
int weight = 0;     // βάρος

bool metalDetected = false;
bool transparentDetected = false;
bool heavyDetected = false;

HX711 scale;    // πλακέτα (ενισχυτής) του αισθητήρα βάρους

char ssid[] = SECRET_SSID;   // WiFi SSID (name) 
char pass[] = SECRET_PASS;   // WiFi password
int status = WL_IDLE_STATUS;
WiFiClient  client;

// η διεύθυνση του thingspeak
char server[] = "api.thingspeak.com";
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;
const char *myReadAPIKey = SECRET_READ_APIKEY;

// οι 3 μετρητές για τα υλικά που ανακυκλώθηκαν
int metal = 0;      // ο αριθμός των μετάλλων που ανακυκλώθηκαν
int plastic = 0;    // ο αριθμός των πλαστικών που ανακυκλώθηκαν
int glass = 0;      // ο αριθμός των γυαλιών που ανακυκλώθηκαν

// η βαθμολογία που προκύπτει από τα υλικά που ανακυκλώθηκαν
int score = 0;

// για μέτρηση του χρόνου
unsigned long currentTime;
unsigned long previousTime = 0;

// η θέση του παίκτη στον πίνακα αντιστοίχισης ονομάτων-καρτών
int personFieldIndex;

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LASER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(METAL_PIN, INPUT_PULLUP);

  Serial.begin(115200);

  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522

  lcd.init(); // lcd οθόνη
  lcd.backlight();
  // Print a message to the LCD.
  lcd.setCursor(0,0);
  lcd.print("3o GEL TRIKALON");
  lcd.setCursor(0,1);
  lcd.print("Odysseas Elytis");
  
  Serial.println("ΔΙΑΓΩΝΙΣΜΟΣ ΑΝΑΚΥΚΛΩΣΗΣ");
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // ο παρακάτω συντελεστής υπολογίστηκε με το καλιμπράρισμα
  scale.set_scale(1091.32);
  scale.tare();   // reset the scale to 0
  delay(1000);

  // Σύνδεση στο WiFi
  ConnectToWiFi();
  
  // Initialize ThingSpeak
  ThingSpeak.begin(client);
}

void loop()
{
  lcd.setCursor(0,0);
  lcd.print(" Recycling Game ");
  lcd.setCursor(0,1);
  lcd.print("Place your card.");
  // Αναμονή για νέα κάρτα
  tagID = "";
  while( getID() )
  {
    tone(BUZZER_PIN, 1000); // Send sound signal...
    delay(500);             // ...for 0.5 sec
    noTone(BUZZER_PIN);     // Stop sound...

    score = 0;
    // αναζήτηση του UID της κάρτας στον αποθηκευμένο πίνακα
    int i = 0;
    bool found = false;
    while( i<PERSONS_NUMBER && !found )     // το PERSONS_NUMBER ορίζεται στο persons.h
    {
      if( tagID == cardsUIDs[i] )           // το cardUIDs ορίζεται στο persons.h
      {
        found = true;
        Serial.println(names[i]);           // το names ορίζεται στο persons.h
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Welcome");
        //int x = (16 - names[i].length()) div 2; // για στοίχιση στο κέντρο
        //lcd.setCursor(x,1);
        lcd.setCursor(8,0);
        lcd.print(names[i]);
        personFieldIndex = i+1; // ο πίνακας είναι από 0-7, ενώ τα fields του thingspeak απο 1-8
      }
      i++;
    }
    if( !found )
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(" Unknown Card...");
      delay(3000);
      break;
    }

    // Αναμονή για τοποθέτηση υλικού για ανακύκλωση
    lcd.setCursor(0,1);
    lcd.print("Place material..");
    Serial.println("Place material...");

    // Διάβασμα ζυγαριάς
    if( scale.is_ready() )
    {
      // όσο δεν τοποθετήθηκε υλικό στη ζυγαριά, περίμενε
      while( (weight=scale.get_units()) < 3 )
      {
        delay(500);
      }
      delay(1000);  // αναμονή για να ισορροπήσει ο αισθητήρας
      weight = scale.get_units();
      Serial.print("Weight=");
      Serial.println(weight);
    }

    // Ανίχνευση βάρους
    if( weight < 100 )
      heavyDetected = false;
    else
      heavyDetected = true;

    // Ανίχνευση μετάλλου
    metalVal = digitalRead(METAL_PIN);
    if( metalVal == LOW )
      metalDetected = true;
    else
      metalDetected = false;
  
    // Ανίχνευση διαφάνειας
    digitalWrite(LASER_PIN, HIGH);
    int counter = 0;    // μέτρηση 5 τιμών
    for( int i=0; i<5; i++)
    {
      int ldrStatus = analogRead(LDR_PIN);
      if( ldrStatus > 700 )
        counter++;
      delay(50);
    }
    digitalWrite(LASER_PIN, LOW);
    if( counter > 3 )
      transparentDetected = true;
    else
      transparentDetected = false;
  
    // Αποτελέσματα
    if( metalDetected )
    {
      Serial.println("Μέταλλο");
      lcd.setCursor(0,1);
      lcd.print("Metal: 50 pts   ");
      metal++;
    }
    else if( transparentDetected )
    {
      if( heavyDetected )
      {
        Serial.println("Γυαλί");
        lcd.setCursor(0,1);
        lcd.print("Glass: 30 pts   ");
        glass++;
      }
      else
      {
        Serial.println("Πλαστικό");
        lcd.setCursor(0,1);
        lcd.print("Plastic: 40 pts ");
        plastic++;
      }
    }
    else
    {
      Serial.println("Άλλο");
      lcd.setCursor(0,1);
      lcd.print("Other: 0 pts   ");
      delay(3000);
    }

    // έλεγχος του χρόνου
    currentTime = millis();
    // συγχρονισμός με το ThingSpeak κάθε 16 sec
    if( currentTime - previousTime >= 16000 )
    {
      // η βαθμολογία που προκύπτει από τις ανακυκλώσεις
      score = metal*50 + plastic*40 + glass*30;
  
      // αν υπάρχει νέο αντικείμενο, στείλε τη βαθμολογία στο ThingSpeak
      if( score != 0 )
      {
        if( personFieldIndex>=1 && personFieldIndex<=8 )
        {
          // Σύνδεση (ή επανασύνδεση) στο WiFi
          ConnectToWiFi();
    
          // διάβασε την τελευταία τιμή από το ThingSpeak, από το αντίστοιχο field
          int previousScore = ThingSpeak.readIntField(myChannelNumber, personFieldIndex);

          // Check the status of the read operation to see if it was successful
          int statusCode = ThingSpeak.getLastReadStatus();
          if(statusCode == 200)     // επιτυχής ανάγνωση από το ThingSpeak!
          {
            Serial.println("Successfully Reading ThingSpeak Score: " + String(previousScore));
            int x = ThingSpeak.writeField(myChannelNumber, personFieldIndex, score+previousScore, myWriteAPIKey);
            if(x == 200)      // επιτυχές γράψιμο στο ThingSpeak!  
            {
              Serial.println("Channel writing successful.");
              // μηδενισμός όλων των μετρητών και του σκορ
              score = metal = plastic = glass = 0;
            }
            else
            {
              Serial.println("Problem writing channel. HTTP error code " + String(x));
            }
          }
          else
          {
            Serial.println("Problem reading channel... HTTP error code " + String(statusCode));
          }
        }
        else
        {
          Serial.println("Ο παίκτης είναι εκτός ορίων του thingspeak");
          delay(1500);
          lcd.setCursor(0,1);
          lcd.print(" Not in players ");
          delay(2500);
        }
      }
  
      previousTime = currentTime;    
    }
  }
}

// Διάβασμα νέας κάρτας RFID
boolean getID()
{
  // Getting ready for Reading PICCs
  if( !mfrc522.PICC_IsNewCardPresent() )
  {
    return false;
  }
  if( !mfrc522.PICC_ReadCardSerial() )
  {
    return false;
  }
 
  tagID = "";
  for( int i = 0; i < mfrc522.uid.size; i++ )
  {
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the 4 bytes in a single String variable
  }
  tagID.toUpperCase();
  mfrc522.PICC_HaltA(); // Stop reading
  return true;
}


void ConnectToWiFi()
{
  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }
}
