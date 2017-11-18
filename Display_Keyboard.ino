

// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// 
// File:    PILIGHT-KONSOLE
// Zweck:   Interface mit LCD Display und Keyboard zum Pilight
//
// Autor:   Marc Ahlgrim (frenchie71)
// 
// ////////////////////////////////////////////////////////////
//
// Versionshistorie:
//
//     2017-11-09 Initiale Version
// 
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////


// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// Libraries
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////

 
//#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
//#include<EEPROM.h>
#include <elapsedMillis.h>

// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// Globale Settings
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////
// Die Tastatur-Layouts sind unterschiedlich je Maschine RASPIUG, PRIMAVIP oder PROTOTYPE
// ////////////////////////////////////////////////////////////

#define RASPIUG
//#define PROTOTYPE
//#define PRIMAVIP

// ////////////////////////////////////////////////////////////
// Die unterschiedlichen SEVERITY Einstellungen 
// ////////////////////////////////////////////////////////////

#define LOWSEVERITY   0
#define NORMALSEVERITY   1
#define HIGHSEVERITY  2
#define ULTRASEVERITY 3

// SEVERITY  BACKLIGHT      BUZZER
// ------------------------------------------
// LOW       always off     no
// NORMAL    on DELAY       no
// HIGH      on DELAY       yes
// ULTRA     always on      yes



static int lastSeverity = LOWSEVERITY;

// ////////////////////////////////////////////////////////////
// Serial Settings
// ////////////////////////////////////////////////////////////

#define BAUDRATE 57600
String serialInputString = "";
boolean serialStringComplete = false;

// ////////////////////////////////////////////////////////////
// LCD Display - i2c Adresse und Layout
// ////////////////////////////////////////////////////////////

#define I2CADDRESS 0x3F   
#define LCDROWS 4
#define LCDCOLS 20

LiquidCrystal_I2C lcd(I2CADDRESS, LCDCOLS, LCDROWS, LCD_5x8DOTS);

// ////////////////////////////////////////////////////////////
// Tastaturmatrix
// ////////////////////////////////////////////////////////////

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns

#ifdef PRIMAVIP

char hexaKeys[ROWS][COLS] = {
   {'1','2','3','4'},
   {'5','6','7','8'},
   {'9','0','*','#'},
   {'W','H','D','A'}
 };

byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {9, 8, 7, 6}; //connect to the column pinouts of the keypad

#endif

#ifdef PROTOTYPE

char hexaKeys[ROWS][COLS] = {
   {'1','2','3','A'},
   {'4','5','6','B'},
   {'7','8','9','C'},
   {'*','0','#','D'}
 };

byte rowPins[ROWS] = {2, 3, 4, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8, 9}; //connect to the column pinouts of the keypad

#endif

#ifdef RASPIUG

char hexaKeys[ROWS][COLS] = {
   {'1','2','3','A'},
   {'4','5','6','B'},
   {'7','8','9','C'},
   {'*','0','#','D'}
 };

byte rowPins[ROWS] = {9,8,7,6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5,4,3,2}; //connect to the column pinouts of the keypad

#endif

Keypad xKeyPad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 

// ////////////////////////////////////////////////////////////
// other global variables
// ////////////////////////////////////////////////////////////

elapsedMillis millisSinceEvent = 0;
static int millisToSwitchOffBacklight = 10000;  // wie lange LCD Licht anlassen nach einem Event
static bool lcdbacklight = true;  // wird analog zum LCD backlight gestzt
static bool noEventAck = false; // Falls lange nix war - vermeiden, dass die Routine oft aufgerufen wird 

String sKeyPadInput ="";

// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// SETUP
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////

void setup()
{
	// initialize the LCD

	lcd.begin();
  lcd.backlight();
  lcd.home ();
  lcd.print("PILIGHT booting...");  

// for(int j=0;j<4;j++)  EEPROM.write(j, j+49);
// for(int j=0;j<4;j++)  password[j]=EEPROM.read(j);

  Serial.begin(BAUDRATE);
  serialInputString.reserve(200);
  
}

// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// serialEvent - taken from the arduino examples
// ////////////////////////////////////////////////////////////
// SerialEvent occurs whenever a new data comes in the
// hardware serial RX.  This routine is run between each
// time loop() runs, so using delay inside loop can delay
// response.  Multiple bytes of data may be available.
// ////////////////////////////////////////////////////////////


void serialEvent() 
{
  while (Serial.available()) 
  {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    serialInputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if ( (inChar == '\n') || (inChar == '\r') )
    {
      serialStringComplete = true;
    }
  }
}


// ////////////////////////////////////////////////////////////
// eventOcurred
// ////////////////////////////////////////////////////////////
// Eine Taste wurde gedrückt oder ein Kommando wurde empfangen
// -> LCD Hintergrundlicht an und Zähler zurücksetzen
// ////////////////////////////////////////////////////////////

void eventOcurred(int severity, int x, int y, String xMessage)
{
  millisSinceEvent = 0;

  // ggf. LCD Licht anmachen
  
  if ((lcdbacklight == false) && (severity > LOWSEVERITY)) 
  {
    Serial.print("ONLINE\n");
    lcd.backlight();
    lcdbacklight=true;
  }

  // Den Eventschalter setzen
  
  noEventAck = false;

  // Falls die Koordinaten gültig sind, Nachricht ausgeben

  if ( (x>=0) && (x<LCDCOLS) && (y>=0) && (y<LCDROWS) && (xMessage.length()>0) )
  {
    lcd.setCursor(x,y);
    lcd.print(xMessage); 
  }

  // Severity aktualisieren

  if (severity > lastSeverity) lastSeverity = severity;
//  if (severity >= HIGHSEVERITY) lcd.blink();
}

// ////////////////////////////////////////////////////////////
// nothingHappened
// ////////////////////////////////////////////////////////////
// Es ist lange nichts geschehen
// -> LCD Hintergrundlicht aus
// ////////////////////////////////////////////////////////////


void nothingHappened()
{
  // falls bereits aufgerufen, nicht nochmal durchlaufen

  if (noEventAck == true) return;

  // bei Severity=Ultra das Licht nicht ausmachen

  if (lastSeverity == ULTRASEVERITY) return;

  // Im normalfall Licht ausmachen
  
  if (lcdbacklight == true) 
  {
    lcd.noBacklight();
    lcdbacklight=false;
    lcd.setCursor(LCDCOLS-5,LCDROWS-1);
    lcd.print("     "); 
  }

  // Event und Keypad Input zurücksetzen
  
  noEventAck = true;
  sKeyPadInput="";
  Serial.print("OFFLINE\n");
}

// ////////////////////////////////////////////////////////////
// reduceSeverity
// ////////////////////////////////////////////////////////////
// setze die neue Severity - falls z.B. ein Ereignis hoher
// Priorität mit einem Tastendruck bestätigt wurde
// oder das Hauptprogramm dazu anweist
// ////////////////////////////////////////////////////////////


void reduceSeverity(int newSeverity)
{

   if (newSeverity < HIGHSEVERITY)
     if (lastSeverity >= HIGHSEVERITY)
       lcd.noBlink();

   lastSeverity = newSeverity;
}

// ////////////////////////////////////////////////////////////
// parseSerialCommand
// ////////////////////////////////////////////////////////////
// Kommandos, die über die serielle Schnittstelle empfangen 
// werden, interpretieren
// ////////////////////////////////////////////////////////////

void parseSerialCommand()
{


  
  String theCommand = serialInputString.substring(0,serialInputString.length()-1); // Line Feed am Ende der Zeile wegmachen

  // Variablen zurücksetzen
  
  serialInputString = "";
  serialStringComplete = false;

  //theCommand.toUpperCase();

  String CommandArray[] = {"", "", "", "" };

  // CLEAR löscht den LCD Screen

  if (theCommand == "CLEAR") lcd.clear();

  // Andere Kommandos können bis zu 4 Parameter übergeben
  // die durch Leerzeichen getrennt sind

  int xPos = theCommand.indexOf(' ')+1;
  int i = 0;

  while (xPos < theCommand.length())
  {
    char c = theCommand.charAt(xPos++);
    if ( (c != ' ') || (i == 3) ) 
      CommandArray[i] += c;
    else
      i++;
  }

  // MESSAGE gibt eine Nachricht auf dem LCD aus und setzt die Severity
  // Parameter :
  // 0 - Severity
  // 1 - x-Wert (COLUMN)
  // 2 - y-Wert (ROW)
  // 3 - Nachricht

  if (theCommand.substring(0,8) == "MESSAGE ")
    lastSeverity = CommandArray[0].toInt();
    eventOcurred(CommandArray[0].toInt(),CommandArray[1].toInt(),CommandArray[2].toInt(),CommandArray[3]);
  
}


// ////////////////////////////////////////////////////////////
// sendKeyPadInput
// schickt Keypad Eingabe an Seriell
// ////////////////////////////////////////////////////////////

void sendKeyPadInput()
{
  sKeyPadInput+="\n";
  Serial.print(sKeyPadInput);
  sKeyPadInput="";
}

// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////
// MAIN LOOP
// ////////////////////////////////////////////////////////////
// ////////////////////////////////////////////////////////////


void loop()
 {

   // prüfe ob Taste gedrückt

   char customKey = xKeyPad.getKey();
   if (customKey)
   {
      reduceSeverity(LOWSEVERITY);
      if  (noEventAck == true)  // es ist der erste Tastendruck - mache nur das Licht an     
      {
        eventOcurred(NORMALSEVERITY,0,0,"");
      }
      else                      // ansonsten einen Stern unten im Eingabefeld anzeigen 
      {
        eventOcurred(NORMALSEVERITY,sKeyPadInput.length()+LCDCOLS-5,LCDROWS-1,"*");

        // * can be used to clear input; also clear input after it has been sent
        
        if ( (customKey == '*') || (customKey == '#') )
        {
          eventOcurred(NORMALSEVERITY,LCDCOLS-5,LCDROWS-1,"     ");

         // # is our enter key

          if (customKey == '#')
            sendKeyPadInput();

          sKeyPadInput = "";
        }

        // otherwise just add the char to the input string
      
        else
          sKeyPadInput += customKey;
      }
   }

  // prüfe ob serieller Befehl vorliegt

   if (serialStringComplete) 
   {
      parseSerialCommand();
   }

   // ggf. Zeitschalter zurücksetzen - lange nix passiert 

   if (millisSinceEvent > millisToSwitchOffBacklight) nothingHappened();
 
 }


// digitalWrite(led, LOW);
//  EEPROM.write(j,key);

