//#define DEBUG 1

#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//declare virtual serial ports
SoftwareSerial pcSerial = SoftwareSerial(11,12);
SoftwareSerial pumpSerial = SoftwareSerial(14,15);
SoftwareSerial chillerSerial = SoftwareSerial(16,17);
SoftwareSerial valveSerial = SoftwareSerial(9,10);

//initialize the library with:
//I2C address, E, R/W, RS, D4, D5, D6, D7, backlight pin, backlight polarity
//uses pins 18 & 19 (A4 & A5)
LiquidCrystal_I2C lcd(0x3f, 2, 1, 0, 4, 5, 6, 7, 3, 0);

const String TIMEOUT_PUMP = "timeout pump";
const String TIMEOUT_VALVE = "timeout valve";
const String TIMEOUT_CHILLER = "timeout chiller";
const bool TARE = true;
const bool NO_TARE = false;

struct pressure
{
  int aPressure;
  int rPressure;
};

unsigned long displayTimer;

void setup()
{
  lcd.begin(16, 2); //start wire and set up the LCD's number of columns and rows:

  pinMode(2, INPUT_PULLUP); //pump bubble detector
  pinMode(3, INPUT_PULLUP); //valve bubble detector
  
  pinMode(4, OUTPUT); //pump solenoid valve 
  pinMode(5, OUTPUT); //valve solenid valve
  
  pinMode(11, INPUT);
  pinMode(12, OUTPUT);
  pcSerial.begin(9600);

  pinMode(14, INPUT);
  pinMode(15, OUTPUT);
  pumpSerial.begin(9600);

  pinMode(16, INPUT);
  pinMode(17, OUTPUT);
  chillerSerial.begin(9600);

  pinMode(9, INPUT);
  pinMode(10, OUTPUT);
  valveSerial.begin(19200);

  //for debug only
  //#if defined(DEBUG)
    Serial.begin(9600);
    Serial.println("ready");
  //#endif

  //terminate pump motor move just in case ...
  pumpWithWait("T");

  //listen to pc
  pcSerial.listen();
  while (pcSerial.isListening() == false){}

  //initialize both lines of lcd display
  lcdDisplay("0,0,V  P S mBar     ");
  lcdDisplay("1,0,                ");

  displayTimer = millis();
}

//wait for ASCII chars from PC terminated with <CR><LF> then call parser
void loop()
{
  String commandString;
  char receivedChar;
  char device;
  static String pcString;

  //get update from pressure transducer every 1/2 second
  if ((millis() - displayTimer) > 500)
  {
    sensor(NO_TARE);
    displayTimer = millis();
  }

  //character available
  if (pcSerial.available() > 0)
  {
    //read character and append to string
    receivedChar = pcSerial.read();
    pcString = pcString + receivedChar;
    
    //for debug only
    #if defined(DEBUG)
      switch(receivedChar)
      {
        case 0x0D:
          Serial.print("<CR>");
          break;
        case 0x0A:
          Serial.print("<LF>");
          break;
        default:
          Serial.print(receivedChar);
          break;
      }  
    #endif
    
    //done when <LF> arrives
    if (receivedChar == 0x0A)
    {
      //for debug only
      #if defined (DEBUG)
        Serial.println("  received from PC");
      #endif
      
      //first character specifies the device
      device = pcString.charAt(0);
      
      //the command starts at the second character - strip off <CR><LF> at end
      commandString = pcString.substring(1,pcString.length() - 2);
      
      //clear input string for next time
      pcString = "";
      
      //execute command
      parsePCString(device, commandString);
    }
  }
}

//call appropriate routine based on first char of pcString (device)
void parsePCString(char device, String commandString)
{
  String returnString;
  
  switch(device)
  {
    case 'P': //syringe pump
      returnString = pump(commandString);
      break;

    case 'V': //selector valve
      returnString = valve(commandString);
      break;

    case 'C': //chiller
      returnString = chiller(commandString);
      break;

    case 'B': //bubble detectors
      returnString = bubble(commandString);
      break;
      
    case 'S': //solenoids
      returnString = solenoid(commandString);
      break;
    
    case 'T': //pressure transducer
      returnString = transducer(commandString);
      break;
    
    case 'L': //LCD display
      returnString = lcdDisplay(commandString);
      break;
 
     case 'M': //macro
      returnString = macro(commandString);
      break;
    
    default:
      break;
  }
  
  //send return string  + <CR><LF> to pc
  pcSerial.print(returnString + "\r\n");
  
  //for debug only
  #if defined(DEBUG)
    Serial.println(returnString + "<CR><LF>  sent to PC");
    Serial.println("ready");
  #endif
  
  //listen to pc again
  pcSerial.listen();
  while (pcSerial.isListening() == false){}
}

//turn solenoids on or off -
//1/2 sec delay between when turning on for inrush
String solenoid(String commandString)
{
  
  if (commandString.charAt(0) == '0')
  {
    digitalWrite(4 , LOW);
    digitalWrite(5 , LOW);
  }
  else
  {
    digitalWrite(4 , HIGH);
    
    //delay to wait for inrush current to end
    delay(500);
    
    digitalWrite(5 , HIGH);
  }

  //write solenoid valve state to LCD line 0, column 6
  lcdDisplay("0,6," + commandString.substring(0));
  
  return("ok");
}

//read and return both bubble detectors -
//0 = light on = dry, 1 = light off = wet
String bubble(String commandString)
{
  return(String(digitalRead(commandString.toInt() + 2)));
}

//write to a line of the LCD display l,c,s -
String lcdDisplay(String commandString)
{
  int indexOfComma;
  int line;
  int column;
  
  //store line
  line = commandString.substring(0,1).toInt();

  //gobble up parameter
  commandString = commandString.substring(2);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store column
  column = commandString.substring(0 , indexOfComma).toInt();
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  lcd.setCursor(column, line);
  lcd.print(commandString);

  delay(100);

  return("ok");
}

//read or tare pressure transducer -
//a struct is used so sensor() can return 2 values
String transducer(String commandString)
{
  struct pressure values;

  switch(commandString.charAt(0))
  {
    case('a'):
      values = sensor(NO_TARE);
      return(String(values.aPressure));
      break;
      
    case('r'):
      values = sensor(NO_TARE);
      return(String(values.rPressure));
      break;
      
    case('t'):
      sensor(TARE);
      return("ok");
      break;
      
    default:
      break;
  }
}

//read 4 bytes from pressure transducer, convert, apply tare, and display to LCD -
//tare if zero == true -
//a struct is used so sensor() can return 2 values
struct pressure sensor(bool zero)
{
  byte sensor[4];
  unsigned long pressure;
  struct pressure values;
  static int tare = 1014; //static to remember tare between readings
    
  Wire.requestFrom(0x5A, 4); //request 4 bytes from slave device 0x5A

  // read in 4 bytes
  for (int x = 0; x < 4; x++)
  {
    while (Wire.available() == false) {} //wait for byte
    sensor[x] = Wire.read(); //read in the byte
  }

  //do math and apply tare
  pressure = ((unsigned long)(sensor[0]) << 24) +
             ((unsigned long)(sensor[1]) << 16) +
             ((unsigned long)(sensor[2]) << 8) +
              (unsigned long)(sensor[3]);
  pressure = pressure / 0x10; //lose a byte of resolution - keeps pressure from getting too long
  pressure = pressure * 0x3E8;
  values.aPressure = (int)(pressure / 0x40000);

  //tare if necessary
  if (zero == true) tare = values.aPressure;

  //calculate relative pressure
  values.rPressure = values.aPressure - tare;
  
  //clear line zero, column 11 for 5 characters before write
  lcdDisplay("0,11,     ");

  //adjust start column for sign
  if (values.rPressure < 0)
  {
    lcdDisplay("0,11," + String(values.rPressure));
  }
  else
  {
    lcdDisplay("0,12," + String(values.rPressure));
  }

  //return absolute and relative pressures
  return(values);
}

//control torrey pines scientific dry bath chiller
String chiller(String commandString)
{
  unsigned long startTime;
  const unsigned long timeLimit = 5000;
  String deviceReturnString;
  char receivedChar;
  
  startTime = millis(); //start watchdog timer
  
  //listen to chiller
  chillerSerial.listen();
  while (chillerSerial.isListening() == false){}
  
  //send command + <CR> to chiller
  chillerSerial.print(commandString + "\r");
  
  //for debug only
  #if defined(DEBUG)
    Serial.print(commandString + "<CR>");
    Serial.println("  sent to chiller");
  #endif

  //wait with watchdog for response from chiller
  while(1)
  {
  
    //bail if timed out
    if (millis() - startTime > timeLimit)
    {
      deviceReturnString = TIMEOUT_CHILLER + "  ";
      break;
    }
    
    //character is available
    if (chillerSerial.available() > 0)
    {
      //read received character and append to string
      receivedChar = chillerSerial.read();
      deviceReturnString = deviceReturnString + receivedChar;
      
      //for debug only
      #if defined(DEBUG)
        switch(receivedChar)
        {
          case 0x0D:
            Serial.print("<CR>");
            break;
          case 0x0A:
            Serial.print("<LF>");
            break;
          default:
            Serial.print(receivedChar);
            break;
        }
      #endif  
     
      //done when <LF> arrives
      if (receivedChar == 0x0A)
      {
        //for debug only
        #if defined(DEBUG)
          Serial.println("  received from chiller");
        #endif
        
        //break out of while(1) loop
        break;
      }
    }
  }

  //strip protocol off of response and return
  return(deviceReturnString.substring(0,deviceReturnString.length() - 2));
}

//control idex 24 position selector valve
String valve(String commandString)
{
  unsigned long startTime;
  const unsigned long timeLimit = 5000;
  String deviceReturnString;
  char receivedChar;
  
  //if command is "P", write new port number to LCD line 0, column 1
  if (commandString.charAt(0) == 'P') lcdDisplay("0,1," + commandString.substring(1));
  
  startTime = millis(); //start watchdog timer
  
  //listen to valve
  valveSerial.listen();
  while (valveSerial.isListening() == false){}

  //send command + <CR> to valve
  valveSerial.print(commandString + "\r");
  
  //for debug only
  #if defined(DEBUG)
    Serial.print(commandString + "<CR>");
    Serial.println("  sent to valve");
  #endif
  
  //wait with watchdog for response from valve
  while(1)
  {
  
    //bail if timed out
    if (millis() - startTime > timeLimit)
    {
      deviceReturnString = TIMEOUT_VALVE + " ";
      break;
    }
    
    //character available
    if (valveSerial.available() > 0)
    {
      //read received character and append to string
      receivedChar = valveSerial.read();
      deviceReturnString = deviceReturnString + receivedChar;
      
      //for debug only
      #if defined(DEBUG)
        switch(receivedChar)
        {
          case 0x0D:
            Serial.print("<CR>");
            break;
          default:
            Serial.print(receivedChar);
            break;
        } 
      #endif 
  
      //done when <CR> arrives
      if (receivedChar == 0x0D)
      {
        //for debug only
        #if defined(DEBUG)
          Serial.println("  received from valve");
        #endif
        
        //break out of while(1) loop
        break;
      }
    }
  }

  if (deviceReturnString.length() > 1)
  {
    //strip protocol off of response and return
    return(deviceReturnString.substring(0,deviceReturnString.length() - 1));
  }
  else
  {
    //valve sometimes returns only control characters so add something human readable
    return("ok");
  }
}

//control hamilton psd6 syringe pump
String pump(String commandString)
{
  unsigned long startTime;
  const unsigned long timeLimit = 5000;
  String deviceReturnString;
  char receivedChar;
  
  //if command "I" write new valve position to LCD line 0, column 4
  if (commandString.charAt(0) == 'I') lcdDisplay("0,4," + commandString.substring(1));
  
  startTime = millis(); //start watchdog timer
  
  //listen to pump
  pumpSerial.listen();
  while (pumpSerial.isListening() == false){}
  
  //send command + protocol characters to pump
  pumpSerial.print("/1" + commandString + "R\r");
  
  //for debug only
  #if defined(DEBUG)
    Serial.print("/1" + commandString + "R<CR>");
    Serial.println("  sent to pump");
  #endif
  
  //wait with watchdog for response from pump
  while(1)
  {
  
    //bail if timed out
    if (millis() - startTime > timeLimit)
    {
      deviceReturnString = "  " + TIMEOUT_PUMP + "   ";
      break;
    }
    
    //character available
    if (pumpSerial.available() > 0)
    {
      //read received character and append to string
      receivedChar = pumpSerial.read();
      deviceReturnString = deviceReturnString + receivedChar;
      
      //for debug only
      #if defined(DEBUG)
        switch(receivedChar)
        {
          case 0x0D:
            Serial.print("<CR>");
            break;
          case 0x0A:
            Serial.print("<LF>");
            break;
          case 0x03:
            Serial.print("<ETX>");
            break;
          default:
            Serial.print(receivedChar);
            break;
        }
      #endif 
  
      //done when <LF> arrives
      if (receivedChar == 0x0A)
      {
        //for debug only
        #if defined(DEBUG)
          Serial.println("  received from pump");
        #endif
        
        //break out of while(1) loop
        break;
      }
    }
  }

  //strip protocol off of response and return
  return(deviceReturnString.substring(2,deviceReturnString.length() - 3));
}

//command syringe pump and wait for it to become ready
String pumpWithWait(String commandString)
{
  String returnString;
  
  //send command to pump
  returnString = pump(commandString);
  if (returnString.charAt(0) == 't') return(returnString);
  
  //query pump until status = ready
  do
  {
    //update pressure transducer display
    sensor(NO_TARE);
      
    //send query command to pump
    returnString = pump("Q");
    if (returnString.charAt(0) == 't') return(returnString);
    
  } while (returnString.charAt(0) == '@'); //done when query status character == '@'

  return(returnString);
}

//macro parser
String macro(String commandString)
{
  String returnString;  
  int indexOfComma;
  int macroNum;
    
  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //parse out macro number
  macroNum = commandString.substring(0 , indexOfComma).toInt();
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //first parameter is macro number to execute
  switch(macroNum)
  {
    case 0:
      returnString = macro0(commandString);
      break;

    case 1:
      returnString = macro1(commandString);
      break;

    case 2:
      returnString = macro2(commandString);
      break;

    case 3:
      returnString = macro3(commandString);
      break;

    case 4:
      returnString = macro4(commandString);
      break;

    case 5:
      returnString = macro5(commandString);
      break;

    case 6:
      returnString = macro6(commandString);
      break;

    default:
      break;
  }

  return(returnString);
}

// macro to initialize system
String macro0(String commandString)
{
  String returnString;
  
  //display status on line 1 of LCD
  lcdDisplay("1,0,running macro0  ");

  //turn bypass solenoids on
  solenoid("1");
  
  //home selector valve motor (valve is left at position 01)
  if (valve("M").charAt(0) == 't') return(TIMEOUT_VALVE);

  //display selector valve port (01) and pump port (1) on line 0 of LCD
  lcdDisplay("0,1,01");
  lcdDisplay("0,4,1");
  
  //initialize pump to valve-to-waste and then syringe-to-top
  if (pumpWithWait("Y10").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //set return steps to 0
  if (pumpWithWait("K0").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //set start speed to min = 50 (200 steps/s)
  if (pumpWithWait("v50").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //set final speed to max = 3400 (13,600 steps/s)
  if (pumpWithWait("V3400").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //set stop speed to min = 50 (200 steps/s)
  if (pumpWithWait("c50").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //display status on line 1 of LCD
  lcdDisplay("1,0,macro0 complete ");

  return("ok");
}

//flush system with bleach or air B|A,v,s,p1,p2,...,pn
String macro1(String commandString)
{
  String portNumberString;
  int indexOfComma;
  String bleachOrAirString;
  String volumeString;
  String speedString;
  
  //display status on line 1 of LCD
  lcdDisplay("1,0,running macro1  ");

  bleachOrAirString = commandString.substring(0,1);
  
  //gobble up parameter
  commandString = commandString.substring(2);
  
  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store move length (steps)
  volumeString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store speed
  speedString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //bypass flow cell
  solenoid("1");
  
  //fill syringe M4
  macro4(bleachOrAirString + "," + volumeString);

  //push through vent  M6 to pre-fill
  macro6("H,P,W," + volumeString + "," + speedString + ",01,0");
  
  //loop thru each port in the parameter list
  while(1)
  {
    //search for comma in string
    indexOfComma = commandString.indexOf(',');
    
    if (indexOfComma != -1)
    {
      //if there is a second comma ...
      portNumberString = commandString.substring(0 , indexOfComma);
      
      //gobble up parameter
      commandString = commandString.substring(indexOfComma + 1);
    }
    else
    {
      //if there is no second comma ...
      portNumberString = commandString;
    }

    //fill syringe M4
    macro4(bleachOrAirString + "," + volumeString);
  
    //push through port M6
    macro6("H,P,W," + volumeString + "," + speedString + "," + portNumberString + ",0");
  
    //if no more commas, we're done
    if (indexOfComma == -1) break;
  }

  //turn off bypass solenoid valves
  solenoid("0");
  
  //set selector to vent (port 01)
  if (valve("P01").charAt(0) == 't') return(TIMEOUT_VALVE); //check for timeout
  
  //set pump valve to waste
  if (pumpWithWait("I1").charAt(0) == 't') return(TIMEOUT_PUMP); //check for timeout
  
  //display status to LCD
  lcdDisplay("1,0,macro1 complete ");

  return("ok");
} 

//macro to prime vials bypassing flow cell P|F,s,p1,p2,...,pn
String macro2(String commandString)
{
  String portNumberString;
  String bypassOrFlowcellString;
  int indexOfComma;
  String speedString;
  unsigned long startTime;
  const unsigned long dryTime = 1000;
  const unsigned long wetTime = 1000;
  unsigned long minTime;
  int watchdogCounter;
  String returnString;
  
  //display status to LCD
  lcdDisplay("1,0,running macro2  ");

  bypassOrFlowcellString = commandString.substring(0,1);
  
  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store speed
  speedString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //prime each port in the parameter list
  while(1)
  {
    //search for comma in string
    indexOfComma = commandString.indexOf(',');
    
    if (indexOfComma != -1)
    {
      //if there is a second comma ...
      portNumberString = commandString.substring(0 , indexOfComma);
      
      //gobble up parameter
      commandString = commandString.substring(indexOfComma + 1);
    }
    else
    {
      //if there is no second comma ...
      portNumberString = commandString;
    }

    //pull thru vent without wait M6
    macro6("L," + bypassOrFlowcellString + ",N,100000," + speedString + ",01,0");

    //start dry timer
    startTime = millis();
    minTime = dryTime;
    watchdogCounter = 0;

    //wait for valve bubble detector to be dry
    do
    {
      //update pressure transducer each cycle
      sensor(NO_TARE);
      
      //read bubble sensors
      if (bypassOrFlowcellString == "P")
      {
        returnString = bubble("1");
      }
      else
      {
        returnString = bubble("0");
      }
      
      //for debug only
      #if defined(DEBUG)
        Serial.println(returnString);
      #endif

      //re-start dry timer if valve bubble detector is wet
      if (returnString.charAt(0) == '1') startTime = millis();
      
      //increment watchdog counter and stop pump and bail if stuck
      watchdogCounter = watchdogCounter + 1;
      if (watchdogCounter > 500)
      {
        //stop pump
        if (pump("T").charAt(0) == 't') return(TIMEOUT_PUMP);
    
        return("stuck in dry bubble loop");
      }
      
    } while ((millis() - startTime) < minTime); //quit when dry timer > min time

    //stop pump
    if (pumpWithWait("T").charAt(0) == 't') return(TIMEOUT_PUMP);
  
    //empty syringe
    macro5("W");
    
    //pull thru port without wait M6
    macro6("L," + bypassOrFlowcellString + ",N,100000," + speedString + "," + portNumberString + ",0");

    //start wet timer
    startTime = millis();
    minTime = wetTime;
    watchdogCounter = 0;

    //wait for valve bubble detector to be wet
    do
    {
      //update pressure transducer
      sensor(NO_TARE);
      
      //read bubble sensors
      if (bypassOrFlowcellString == "P")
      {
        returnString = bubble("1");
      }
      else
      {
        returnString = bubble("0");
      }
      
      //for debug only
      #if defined(DEBUG)
        Serial.println(returnString);
      #endif

      //re-start wet timer if valve bubble detector is dry
      if (returnString.charAt(0) == '0') startTime = millis();
      
      //increment watchdog counter and stop pump and bail if stuck
      watchdogCounter = watchdogCounter + 1;
      if (watchdogCounter > 500)
      {
        //stop pump
        if (pump("T").charAt(0) == 't') return(TIMEOUT_PUMP); //check for timeout
    
        return("stuck in wet bubble loop");
      }
      
    } while ((millis() - startTime) < minTime); //quit when wet timer > min time

    //stop pump
    if (pumpWithWait("T").charAt(0) == 't') return(TIMEOUT_PUMP);
  
    //empty syringe
    macro5("W");
    
    //if no more commas, we're done
    if (indexOfComma == -1) break;
  }

  //set selector to vent (port 01)
  if (valve("P01").charAt(0) == 't') return(TIMEOUT_VALVE); //check for timeout
  
  //turn bypass solenoids off
  solenoid("0");
  
  //display status to LCD
  lcdDisplay("1,0,macro2 complete ");

  return("ok");
}

//drain vials to waste v,s,p1,p2,...,pn
String macro3(String commandString)
{
  String portNumberString;
  int indexOfComma;
  String volumeString;
  String speedString;
  
  //display status to LCD
  lcdDisplay("1,0,running macro3  ");

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store move length (steps)
  volumeString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store move length (steps)
  speedString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //turn on bypass solenoid valves
  solenoid("1");
  
  //loop thru each port in the parameter list
  while(1)
  {
    //search for comma in string
    indexOfComma = commandString.indexOf(',');
    
    if (indexOfComma != -1)
    {
      //if there is a second comma ...
      portNumberString = commandString.substring(0 , indexOfComma);
      
      //gobble up parameter
      commandString = commandString.substring(indexOfComma + 1);
    }
    else
    {
      //if there is no second comma ...
      portNumberString = commandString;
    }

    //pull bypass with wait M6
    macro6("L,P,W," + volumeString + "," + speedString + "," + portNumberString + ",0");
    
    //empty syringe to waste M5
    macro5("W");
        
    //if no more commas, we're done
    if (indexOfComma == -1) break;
  }

  //set selector to vent (port 01)
  if (valve("P01").charAt(0) == 't') return(TIMEOUT_VALVE); //check for timeout
  
  //turn off bypass solenoid valves
  solenoid("0");
  
  //display status to LCD
  lcdDisplay("1,0,macro3 complete ");

  return("ok");
}

//macro to fill syringe B|A,v
String macro4(String  commandString)
{
  int indexOfComma;
  String volumeString;
  bool bleach = false; //as opposed to air
  
  //display status to LCD
  lcdDisplay("1,0,running macro4  ");

  if (commandString.charAt(0) == 'B') bleach = true;
  
  //gobble up parameter
  commandString = commandString.substring(2);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store move length (steps)
  volumeString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //set pump valve and speed to bleach or waste
  if (bleach == true)
  {
    if (pumpWithWait("I2").charAt(0) == 't') return(TIMEOUT_PUMP); //bleach
    if (pumpWithWait("V800").charAt(0) == 't') return(TIMEOUT_PUMP); //bleach
  }
  else
  {
    if (pumpWithWait("I1").charAt(0) == 't') return(TIMEOUT_PUMP); //waste (air)
    if (pumpWithWait("V3400").charAt(0) == 't') return(TIMEOUT_PUMP); //bleach
  }
  
  //send plunger down to fill syringe
  if (pumpWithWait("A" + volumeString).charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //display status to LCD
  lcdDisplay("1,0,macro4 complete ");

  return("ok");
}

//macro to empty syringe B|W
String macro5(String  commandString)
{
  //display status to LCD
  lcdDisplay("1,0,running macro5  ");

  //set pump valve to bleach or waste
  if (commandString.charAt(0) == 'B')
  {
    if (pumpWithWait("I2").charAt(0) == 't') return(TIMEOUT_PUMP); //bleach
  }
  else
  {
    if (pumpWithWait("I1").charAt(0) == 't') return(TIMEOUT_PUMP); //waste (air)
  }
  
  //set pump speed
  if (pumpWithWait("V3400").charAt(0) == 't') return(TIMEOUT_PUMP); //bleach
  
  //send plunger to top
  if (pumpWithWait("A0").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //display status to LCD
  lcdDisplay("1,0,macro5 complete ");

  return("ok");
}

//pull or push thru flow cell or bypass to port with or without wait L|H,P|F,W|N,v,s,p
String macro6(String commandString)
{
  String portNumberString;
  int indexOfComma;
  bool bypass = false; //as opposed to flow cell
  bool wait = false; //as opposed to no wait
  bool pull = false; //pull as opposed to push
  String volumeString;
  String speedString;
  
  //display status on line 1 of LCD
  lcdDisplay("1,0,running macro6  ");

  if (commandString.charAt(0) == 'L') pull = true;
  
  //gobble up parameter
  commandString = commandString.substring(2);
  
  if (commandString.charAt(0) == 'P') bypass = true;
  
  //gobble up parameter
  commandString = commandString.substring(2);
  
  if (commandString.charAt(0) == 'W') wait = true;
  
  //gobble up parameter
  commandString = commandString.substring(2);
  
  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store move length (steps)
  volumeString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //search for comma in string
  indexOfComma = commandString.indexOf(',');
    
  //store speed
  speedString = commandString.substring(0 , indexOfComma);
  
  //gobble up parameter
  commandString = commandString.substring(indexOfComma + 1);

  //store delay time (ms)
  portNumberString = commandString;

  //set bypass solenoids
  if (bypass == true)
  {
    solenoid("1");
  }
  else
  {
    solenoid("0");
  }
  
  //set pump speed
  if (pumpWithWait("V" + speedString).charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //set selector valve to port number
  if (valve("P" + portNumberString).charAt(0) == 't') return(TIMEOUT_VALVE);
  
  //set pump valve to valve
  if (pumpWithWait("I3").charAt(0) == 't') return(TIMEOUT_PUMP);
  
  //wait or no wait
  if (wait == true)
  { 
    
    if (pull == true)
    {
      //send plunger down with wait to pull
      if (pumpWithWait("P" + volumeString).charAt(0) == 't') return(TIMEOUT_PUMP);
    }
    else
    {
      //send plunger up with wait to push
      if (pumpWithWait("D" + volumeString).charAt(0) == 't') return(TIMEOUT_PUMP);
    }
  }
  else
  {
    
    if (pull == true)
    {
      //send plunger down to pull
      if (pump("P" + volumeString).charAt(0) == 't') return(TIMEOUT_PUMP);
    }
    else
    {
      //send plunger up to push
      if (pump("D" + volumeString).charAt(0) == 't') return(TIMEOUT_PUMP);
    }
  }
  
  //display status to LCD
  lcdDisplay("1,0,macro6 complete ");

  return("ok");
} 
