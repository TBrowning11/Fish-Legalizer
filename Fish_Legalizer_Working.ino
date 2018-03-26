/* FishLegalizer.ino
/
/  An Arduino UNO application to enable fisherman to remain in compliance with local fishing laws.
/  Capabilities include a fish weight function, a list of local fishing limit regulations, and
/  supporting instruction sets.
/
////
/
/  v0.1 - initial code routine to test out primary capabilities - Tyler Browning - 2018-MAR-01
/  v0.2 - added monitor output text for all button presses to diagnose button performance;
/         added updateMenu() to each button...it was only on one previously
/  v0.3 - changed button press statements to LOW since we are using INPUT_PULLUPs...which take buttons
/         to HIGH when they are UNpressed.  They should go LOW when pressed.
/  v0.4 - added new cases for sub menus inside of limits menu and added a way to get back to home screen
/         from each one. Also changed scaleLoopBreak to 0 (temporary)
/  v0.5 - updated switch/case statement for menu control of the LIMITS sub-menus;  modified the re-draw statements for
/         the scale amount showing on the SCALE screen
/ .v0.6 - First working version. Fixed screen load cell update problem and added new text for menus.
/
////
/
/  Code support:  Tyler Browning
/
*/

// Arduino INCLUDES
//    SPI interface handlers
#include <SPI.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
//    Graphics TFT display handlers
#include <Adafruit_ST7735.h>          // Hardware-specific library
#include <Adafruit_GFX.h>             // Core graphics library
#include <gfxfont.h>
//    Weight sensor handlers
#include <HX711_ADC.h>                // Load cell

// CONSTANTS
//     TFT Display inputs assigned to specific UNO pins
#define TFT_CS     10
#define TFT_RST    9    // you can also connect this to the Arduino reset...in which case, set this #define pin to 0!
#define TFT_DC     8
#define TFT_SCLK   13   // set these to be whatever pins you are using
#define TFT_MOSI   11   // set these to be whatever pins you are using
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
//    Menu selection button sensors assigned to specific UNO pins
#define UP_PIN     5
#define DOWN_PIN   7
#define MIDDLE_PIN 6
#define HOME_PIN   3
//    Name references for primary colors for TFT
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x0FF0
#define BLUE  0x000F
#define RED   0xF000
#define PINK  0xF00F
//    HX711 constructor (dout pin, sck pin)
// ***TYLER*** - does the statement on the next line work?  Typically, the pins have to be referenced by their digital number.
// A0 is typically 14 and A1 is typically 15.  The line below might work...I'm not certain with these analog inputs.
HX711_ADC LoadCell(A1, A0);


// VARIABLES
float weight;  // holds the current weight value from the load cell
long t = 0;        // holds timer value used in weight load cell calculations
float loadCellCalibrationFactor = 102;  // used to calibrate the specific setup of this load cell.  Should be adjusted to achieve precise, controlled weight.
int scaleLoopBreak = 0;  // forces the display screen to update after weightLoopCount cycles when on the SCALE menu even if no buttons have been pressed
int scaleLoopCounter = 0; // holds the number of current cycles through the loop function since last screen update

volatile boolean up = false;
volatile boolean down = false;
volatile boolean middle = false;
volatile boolean hom = false;

// ***TYLER*** - I changed the name to "MIDDLE" on these to be consistent with your naming
int upButtonState = 0;
int downButtonState = 0;
int middleButtonState = 0;
int homeButtonState = 0;
int lastUpButtonState = 0;
int lastDownButtonState = 0;
int lastMiddleButtonState = 0;
int lastHomeButtonState = 0;

// ***TYLER*** - Added new variable to hold the menu page that we want to display
int currentMenuPage = 0;  // holds the current menu page to be displayed;
                          // page structure for this integer will be...
                          // 1   = Menu page 1
                          // 11  = Sub menu 1 of page 1
                          // 12  = Sub menu 2 of page 1
                          // 121 = Sub menu 1 of sub menu 2 of page 1
int currentMenuItem = 0;  // holds the currently selected item "cursor" within the chosen menu page


// -----------------------------------------------------------------------------------
// INITIAL SETUP ROUTINE - runs ONCE on UNO startup
// -----------------------------------------------------------------------------------
void setup() {

  // initialize the serial interface for logging messages during execution...for debugging
  Serial.begin(9600);
  Serial.println("Starting up the Fish Legalizer...");

  // initialize the TFT display
  tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab
  tft.fillScreen(BLACK);
  tft.setRotation(3);
  Serial.println("...TFT initialized...");

  // initialize the LoadCell
  LoadCell.begin();
  long stabilisingTime = 2000; // tare precision can be improved by adding a few seconds of stabilising time
  LoadCell.start(stabilisingTime);
  LoadCell.setCalFactor(loadCellCalibrationFactor); // user set calibration factor (float)
  Serial.println("...LoadCell tare weight completed...");
    
  // initialize the Menu button sensors
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(MIDDLE_PIN, INPUT_PULLUP);
  pinMode(HOME_PIN, INPUT_PULLUP);
  Serial.println("...menu button pins initialized...");

  // draw the inital screen display
  drawScreen(currentMenuPage, currentMenuItem);
  Serial.println("...drawing the initial Menu screen...");

  // Initialization complete
  Serial.println("...Initialization COMPLETE!");
}


// -----------------------------------------------------------------------------------
// EXECUTION LOOP - runs CONTINUOUSLY
// -----------------------------------------------------------------------------------
void loop() {

  upButtonState = digitalRead(UP_PIN);
  downButtonState = digitalRead(DOWN_PIN);
  middleButtonState = digitalRead(MIDDLE_PIN);
  homeButtonState = digitalRead(HOME_PIN);

  weight = readLoadCellWeight(weight);  // reads the current weight value from the load cell during EACH loop cycle

  checkIfUpButtonIsPressed();
  checkIfDownButtonIsPressed();
  checkIfMiddleButtonIsPressed();
  checkIfHomeButtonIsPressed();

  // NOTE:  with the exception of the SCALE screen routine below, the only time we update the screen is when
  //        something has changed that would require screen update.  This should keep our "loop" as efficient as possible
  //        since we avoid unnecessary screen redraws.

  // if we are trying to display the weight of the item on the load cell and we have selected the SCALE menu, then
  //     we will update the screen periodically to show the new weight.  We do this in case their is a load settling on the
  //     load cell.  Otherwise, it would call only once and not update until buttons were pressed.
  if ((currentMenuPage == 1) && (scaleLoopCounter >= scaleLoopBreak)) {
    // rather than use the drawScreen function (which is used for general screen draws between menu changes, we'll do something
    //     different here to try and help the scale results show up better/faster
    //drawScreen(currentMenuPage, currentMenuItem);
    drawScaleWeightScreen();
    Serial.print("current weight value is: ");
    Serial.println(weight);
    scaleLoopCounter = 0;
  } else {
    scaleLoopCounter++;
  }

}


//
// readLoadCellWeight - reads the load cells current value and converts to weight, in pounds.
//     RETURNS:  float value of weight in lbs
//
float readLoadCellWeight(float previousWeight) {
  //local vars
  float calculatedLoad = previousWeight;                // stores the values read from the load cell for processing
  float weightAdjustment = 0.00220462262;  // float value to adjust the value read from the load cell to pounds

  // update() should be called at least as often as HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS;
  // longer delay in scetch will reduce effective sample rate (be carefull with delay() in loop)
  LoadCell.update();

  //get smoothed value from data set + current calibration factor
  if (millis() > t + 250) {
    float i = LoadCell.getData();
    if(i < 200){
      calculatedLoad = 0;
    } else {
      calculatedLoad = i*weightAdjustment;
    }
    Serial.print("Load_cell output val: ");
    Serial.println(calculatedLoad);
    t = millis();
  }

  //receive from serial terminal
  if (Serial.available() > 0) {
    float i;
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  //check if last tare operation is complete
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

  // return the calculated weight value
  return calculatedLoad;
}


//
// checkIfUpButtonIsPressed
//
void checkIfUpButtonIsPressed()
{
  if (upButtonState != lastUpButtonState) {
    if (upButtonState == LOW) {
      Serial.println("UP button pressed...went LOW.");
      up = true;
      updateMenu();
    } else {
      up = false;
      Serial.println("UP button pressed...went HIGH.");
    }
    delay(50);
  }
  lastUpButtonState = upButtonState;
}


//
// checkIfDownButtonIsPressed
//
void checkIfDownButtonIsPressed()
{
  if (downButtonState != lastDownButtonState) {
    if (downButtonState == LOW) {
      Serial.println("DOWN button pressed...went LOW.");
      down = true;
      updateMenu();
    } else {
      down = false;
      Serial.println("DOWN button pressed...went HIGH.");
    }
    delay(50);
  }
  lastDownButtonState = downButtonState;
}


//
// checkIfMiddleButtonIsPressed
//
void checkIfMiddleButtonIsPressed()
{
  if (middleButtonState != lastMiddleButtonState) {
    if (middleButtonState == LOW) {
      middle = true;
      updateMenu();
      Serial.println("MIDDLE button pressed...went LOW.");
    } else {
      middle = false;
      Serial.println("MIDDLE button pressed...went HIGH.");
    }
    delay(50);
    
  }
  lastMiddleButtonState = middleButtonState;
}

//
// checkIfHomeButtonIsPressed
//
void checkIfHomeButtonIsPressed()
{
  if (homeButtonState != lastHomeButtonState) {
    if (homeButtonState == LOW) {
      hom = true;
      updateMenu();
      Serial.println("HOME button pressed...went LOW.");
    } else {
      hom = false;
      Serial.println("HOME button pressed...went HIGH.");
    }
    delay(50);
    
  }
  lastHomeButtonState = homeButtonState;
}


//
// updateMenu - contains the menu structure to know what actions to take with each button press
//
void updateMenu() {
  
  switch (currentMenuPage) {
    // process presses from MAIN MENU
    case 0:
      // if on Menu Item 0 = SCALE
      if ((currentMenuItem == 0) && (up == true)) {
        currentMenuItem = 3; // move cursor to bottom row
      } else if ((currentMenuItem == 0) && (down == true)) {
        currentMenuItem++; // move cursor down one row
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // go into SUBMENU 1 = SCALE
        currentMenuPage = 1;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 1 = LIMITS
        else if ((currentMenuItem == 1) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 1) && (down == true)) {
          currentMenuItem++; //cursor down
      } else if ((currentMenuItem == 1) && (middle == true)) {
        // go into SUBMENU 2 = LIMITS
        currentMenuPage = 2;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 1) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 2 = HELP
        else if ((currentMenuItem == 2) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 2) && (down == true)) {
          currentMenuItem++; //cursor down
      } else if ((currentMenuItem == 2) && (middle == true)) {
        // go into SUBMENU 3 = HELP
        currentMenuPage = 3;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 2) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 3 = ABOUT
        else if ((currentMenuItem == 3) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 3) && (down == true)) {
          currentMenuItem = 0; //cursor down
      } else if ((currentMenuItem == 3) && (middle == true)) {
        // go into SUBMENU 4 = ABOUT
        currentMenuPage = 4;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 3) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;

    // process presses from SCALE MENU
    case 1:
      // ***TYLER*** - not sure how many menu items you want on the SCALE screen...for now just "HOM" does anything
      if ((currentMenuItem == 0) && (up == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (down == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;

    // process presses from LIMITS MENU 
    case 2:
    // ***TYLER*** - not sure how many menu items you want on the LIMITS screen...for now just "HOM" does anything
       if ((currentMenuItem == 0) && (up == true)) {
        currentMenuItem = 4; // move cursor to bottom row
      } else if ((currentMenuItem == 0) && (down == true)) {
        currentMenuItem++; // move cursor down one row
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // go into SUBMENU 5 = Oconee
        currentMenuPage = 5;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 1 = Lake 2
        else if ((currentMenuItem == 1) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 1) && (down == true)) {
          currentMenuItem++; //cursor down
      } else if ((currentMenuItem == 1) && (middle == true)) {
        // go into SUBMENU 6 = Lake 2
        currentMenuPage = 6;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 1) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 2 = Lake 3
        else if ((currentMenuItem == 2) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 2) && (down == true)) {
          currentMenuItem++; //cursor down
      } else if ((currentMenuItem == 2) && (middle == true)) {
        // go into SUBMENU 7 = Lake 3
        currentMenuPage = 7;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 2) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      } 
      // if on Menu Item 3 = Lake 4
        else if ((currentMenuItem == 3) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 3) && (down == true)) {
          currentMenuItem = 4; //cursor down
      } else if ((currentMenuItem == 3) && (middle == true)) {
        // go into SUBMENU 8 = Lake 4
        currentMenuPage = 8;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 3) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      // if on Menu Item 4 = Lake 5
        else if ((currentMenuItem == 4) && (up == true)) {
          currentMenuItem--; //cursor up
      } else if ((currentMenuItem == 4) && (down == true)) {
          currentMenuItem = 0; //cursor down
      } else if ((currentMenuItem == 4) && (middle == true)) {
        // go into SUBMENU 9 = Lake 5
        currentMenuPage = 9;
        currentMenuItem = 0;
      } else if ((currentMenuItem == 4) && (hom == true)) {
        // return to TOP MENU
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;

    // process presses from HELP MENU
    case 3:
    // ***TYLER*** - not sure how many menu items you want on the HELP screen...for now just "HOM" does anything
      if ((currentMenuItem == 0) && (up == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (down == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;

    // process presses from the ABOUT MENU
    case 4:
    // ***TYLER*** - not sure how many menu items you want on the ABOUT screen...for now just "HOM" does anything
      if ((currentMenuItem == 0) && (up == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (down == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;

    // handle all of the "Lake" menus that hang off of the "LIMITS" menu...all of their functionality is the same
    // 5=Oconee; 6=Lake 2; 7=Lake3; 8=Lake4; 9=Lake5
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      if ((currentMenuItem == 0) && (up == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (down == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (middle == true)) {
        // do nothing
      } else if ((currentMenuItem == 0) && (hom == true)) {
        currentMenuPage = 0;
        currentMenuItem = 0;
      }
      break;
      
  } // end SWITCH

  drawScreen(currentMenuPage, currentMenuItem);
} //end void updateMenu()


//
// drawScreen - handles what should be shown on the screen at any given time
//
void drawScreen(int menuPageShown, int menuItemSelected) {
  
  // ***TYLER*** - I think this is needed to allow the screen to redraw correctly.  You might want to try it wihtout this next line and see what happens
  tft.fillScreen(BLACK);  // blank the screen out before it redraws each time.

  // common settings for our TFT menu display
  tft.setTextWrap(false);    // COMMENT THIS OUT AFTER LOAD CELL IS BACK IN PLAY
  tft.setTextSize(2);

  // MENU PAGE 0 - MAIN MENU
  if (menuPageShown == 0) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("MAIN MENU");

    if (menuItemSelected == 0) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 35);
    tft.print(">Scale");

    if (menuItemSelected == 1) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 55);
    tft.print(">Limits");

    if (menuItemSelected == 2) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 75);
    tft.print(">Help");

    if (menuItemSelected == 3) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 95);
    tft.print(">About");
  }

  // MENU PAGE 1 - SCALE
  if (menuPageShown == 1) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(35, 0);
    tft.print("SCALE");

    // show the weight that is currently read from the scale
    // ***TYLER*** - you may have to adjust the x & y positions of the text below to get the weight to display like you want it
    tft.setCursor(40,90);
    tft.setTextSize(2);
    tft.setTextWrap(false);
    tft.print("lbs.");
    tft.setCursor(4,50);
    tft.setTextSize(5);
    tft.print(weight);
    tft.print(" ");
  }

  // MENU PAGE 2 - Limits
  if (menuPageShown == 2) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Limits");

    if (menuItemSelected == 0) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 35);
    tft.print(">Oconee");

    if (menuItemSelected == 1) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 55);
    tft.print(">Lake 2");

    if (menuItemSelected == 2) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 75);
    tft.print(">Lake 3");

    if (menuItemSelected == 3) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 95);
    tft.print(">Lake 4");

    if (menuItemSelected == 4) {
      tft.setTextColor(BLACK, WHITE);
    } else {
      tft.setTextColor(WHITE, BLACK);
    }
    tft.setCursor(0, 115);
    tft.print(">Lake 5");
  }
  
  // MENU PAGE 3 - Help
  if (menuPageShown == 3) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("HELP");
    tft.setTextWrap(true);
    tft.setCursor(1,20);
    tft.setTextSize(1);
    tft.print("This is some text");
  }

  // MENU PAGE 4 - About
  if (menuPageShown == 4) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("ABOUT");
    tft.setTextWrap(true);
    tft.setCursor(1,20);
    tft.setTextSize(1);
    tft.print("This project was created as part of an andvanced prototyping class at Berry College. If using this product and need help e-mail me at tylerdbornwing11@gmail.com");
  } 
  
  // MENU PAGE 5 - Oconee
  if (menuPageShown == 5) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Oconee");
    tft.setTextWrap(true);
    tft.setCursor(1,20);
    tft.setTextSize(1);
    tft.print("This is some text");  
  } 

  // MENU PAGE 6 - Lake 2
  if (menuPageShown == 6) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Lake 2");   
  } 

  // MENU PAGE 7 - Lake 3
  if (menuPageShown == 7) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Lake 3");   
  } 

  // MENU PAGE 8 - Lake 4
  if (menuPageShown == 8) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Lake 4");   
  } 

  // MENU PAGE 9 - Lake 5
  if (menuPageShown == 9) {
    tft.setTextColor(WHITE, BLACK);
    tft.setCursor(9, 0);
    tft.print("Lake 5");   
  } 

} // end drawScreen()


//
// drawScaleWeightScreen - Special screen update routine that only updates the scale's weight on the screen
//
void drawScaleWeightScreen() {
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(5);
  tft.setCursor(4,50);
  tft.print(weight);
  tft.print(" "); //<-- do we need this extra line here
}
