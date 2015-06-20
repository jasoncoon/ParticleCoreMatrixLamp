#include "FastLED/FastLED.h"
FASTLED_USING_NAMESPACE;

#include "application.h"
#include <math.h>

#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)

#define LED_PIN     1
#define CLOCK_PIN   0
#define COLOR_ORDER GBR // 1 = GBR; 2 = BGR;
#define CHIPSET     APA102

// Params for width and height
#define MatrixWidth  6
#define MatrixHeight 15

#define NUM_LEDS    MatrixWidth * MatrixHeight
CRGB leds[NUM_LEDS];

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

const bool    MatrixSerpentineLayout = true;
 
// List of patterns to cycle through.  Each is defined as a separate function below.

typedef uint8_t (*SimplePattern)();
typedef SimplePattern SimplePatternList[];
typedef struct { SimplePattern drawFrame;  char name[32]; } PatternAndName;
typedef PatternAndName PatternAndNameList[];
 
// These times are in seconds, but could be changed to milliseconds if desired;
// there's some discussion further below.
 
const PatternAndNameList patterns = { 
  { test,               "Test" },
  { rainbow,            "Rainbow" },
  { rainbowWithGlitter, "Rainbow With Glitter" },
  { confetti,           "Confetti" },
  { sinelon,            "Sinelon" },
  { bpm,                "Beat" },
  { juggle,             "Juggle" },
  { fire,               "Fire" },
  { water,              "Water" },
  { wave,               "Wave" },
  { analogClock,        "Analog Clock" },
  { fastAnalogClock,    "Fast Analog Clock Test" },
  { showSolidColor,     "Solid Color" }
};

uint8_t brightness = 32;

int patternCount = ARRAY_SIZE(patterns);
int patternIndex = 0;
char patternName[32] = "Rainbow";
int power = 1;
int flipClock = 0;

int timezone = -6;
unsigned long lastTimeSync = millis();

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGBPalette16 palette = RainbowColors_p;
  
CRGB solidColor = CRGB::Blue;
int r = 0;
int g = 0;
int b = 255;

SYSTEM_MODE(SEMI_AUTOMATIC);

int offlinePin = D7;

void setup() {
    FastLED.addLeds<CHIPSET, LED_PIN, CLOCK_PIN, COLOR_ORDER, DATA_RATE_MHZ(12)>(leds, NUM_LEDS);
    FastLED.setCorrection(CRGB( 160, 255, 255));
    FastLED.setBrightness(brightness);
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    
    pinMode(offlinePin, INPUT_PULLUP);
    
    if(digitalRead(offlinePin) == HIGH) {
        Spark.connect();
    }
    
    // Serial.begin(9600);
    // load settings from EEPROM
    brightness = EEPROM.read(0);
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;
    
    FastLED.setBrightness(brightness);
    
    uint8_t timezoneSign = EEPROM.read(1);
    if(timezoneSign < 1)
        timezone = -EEPROM.read(2);
    else
        timezone = EEPROM.read(2);
    
    if(timezone < -12)
        power = -12;
    else if (power > 13)
        power = 13;
    
    patternIndex = EEPROM.read(3);
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;
    
    flipClock = EEPROM.read(4);
    if(flipClock < 0)
        flipClock = 0;
    else if (flipClock > 1)
        flipClock = 1;
        
    r = EEPROM.read(5);
    g = EEPROM.read(6);
    b = EEPROM.read(7);
    
    if(r == 0 && g == 0 && b == 0) {
        r = 0;
        g = 0;
        b = 255;
    }
    
    solidColor = CRGB(r, b, g);
    
    Spark.function("variable", setVariable);
    Spark.function("patternIndex", setPatternIndex);
    Spark.function("patternName", setPatternName);
    
    Spark.variable("power", &power, INT);
    Spark.variable("brightness", &brightness, INT);
    Spark.variable("timezone", &timezone, INT);
    Spark.variable("flipClock", &flipClock, INT);
    Spark.variable("patternCount", &patternCount, INT);
    Spark.variable("patternIndex", &patternIndex, INT);
    Spark.variable("patternName", patternName, STRING);
    Spark.variable("r", &r, INT);
    Spark.variable("g", &g, INT);
    Spark.variable("b", &b, INT);
    
    Time.zone(timezone);
}

void loop() {
  if (Spark.connected() && millis() - lastTimeSync > ONE_DAY_MILLIS) {
    // Request time synchronization from the Spark Cloud
    Spark.syncTime();
    lastTimeSync = millis();
  }
  
  if(power < 1) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      FastLED.delay(250);
      return;
  }
  
  uint8_t delay = patterns[patternIndex].drawFrame();
  
  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(delay); 
  
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

uint8_t XY( uint8_t x, uint8_t y)
{
    uint8_t i;
  
    if( MatrixSerpentineLayout == false) {
        i = (y * MatrixWidth) + x;
    }
    else if( MatrixSerpentineLayout == true) {
        // my custom matrix is rotated serpentine, it runs top to bottom, then counter-clockwise, then bottom to top, etc.
        if( x & 0x01) {
            // Odd columns run backwards
            uint8_t reverseY = (MatrixHeight - 1) - y;
            i = (x * MatrixHeight) + reverseY;
        } else {
            // Even columns run forwards
            i = (x * MatrixHeight) + y;
        }
    }
    
    if( i > NUM_LEDS )
        i = 0;
    
    return i;
}

int setVariable(String args) {
    if(args.startsWith("pwr:")) {
        return setPower(args.substring(4));
    }
    else if (args.startsWith("brt:")) {
        return setBrightness(args.substring(4));
    }
    else if (args.startsWith("tz:")) {
        return setTimezone(args.substring(3));
    }
    else if (args.startsWith("flpclk:")) {
        return setFlipClock(args.substring(7));
    }
    else if (args.startsWith("r:")) {
        r = parseByte(args.substring(2));
        solidColor.r = r;
        EEPROM.write(5, r);
        patternIndex = patternCount - 1;
        return r;
    }
    else if (args.startsWith("g:")) {
        g = parseByte(args.substring(2));
        solidColor.g = g;
        EEPROM.write(6, g);
        patternIndex = patternCount - 1;
        return g;
    }
    else if (args.startsWith("b:")) {
        b = parseByte(args.substring(2));
        solidColor.b = b;
        EEPROM.write(7, b);
        patternIndex = patternCount - 1;
        return b;
    }
    
    return -1;
}

int setPower(String args) {
    power = args.toInt();
    if(power < 0)
        power = 0;
    else if (power > 1)
        power = 1;
    
    return power;
}

int setBrightness(String args)
{
    brightness = args.toInt();
    if(brightness < 1)
        brightness = 1;
    else if(brightness > 255)
        brightness = 255;
        
    FastLED.setBrightness(brightness);
    
    EEPROM.write(0, brightness);
    
    return brightness;
}

int setTimezone(String args) {
    timezone = args.toInt();
    if(timezone < -12)
        power = -12;
    else if (power > 13)
        power = 13;
    
    Time.zone(timezone);
    
    if(timezone < 0)
        EEPROM.write(1, 0);
    else
        EEPROM.write(1, 1);
    
    EEPROM.write(2, abs(timezone));
    
    return power;
}

int setFlipClock(String args) {
    flipClock = args.toInt();
    if(flipClock < 0)
        flipClock = 0;
    else if (flipClock > 1)
        flipClock = 1;
    
    EEPROM.write(4, flipClock);
    
    return flipClock;
}

byte parseByte(String args) {
    int c = args.toInt();
    if(c < 0)
        c = 0;
    else if (c > 255)
        c = 255;
    
    return c;
}

int setPatternIndex(String args)
{
    patternIndex = args.toInt();
    if(patternIndex < 0)
        patternIndex = 0;
    else if (patternIndex >= patternCount)
        patternIndex = patternCount - 1;
        
    EEPROM.write(3, patternIndex);
    
    return patternIndex;
}

int setPatternName(String args)
{
    int index = args.toInt();
    if(index < 0)
        index = 0;
    else if (index >= patternCount)
        index = patternCount - 1;
    
    strcpy(patternName, patterns[index].name);
    
    return index;
}

uint8_t testX = 0;
uint8_t testY = 0;

uint8_t test() 
{
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    leds[XY(testX, testY)] = CHSV(gHue, 255, 255);
    
    // EVERY_N_MILLIS(250) {
        testX++;
        
        if(testX >= MatrixWidth) {
            testX = 0;
            
            testY++;
            
            if(testY >= MatrixHeight)
                testY = 0;
        }
    // }
    
    return 8;
}

const uint8_t huesPerColumn = 255 / MatrixWidth;

uint8_t rainbow() 
{
  for(int x = 0; x < MatrixWidth; x++) {
      CHSV color = CHSV(gHue + (x * huesPerColumn), 255, 255);
      
      for(int y = 0; y < MatrixHeight; y++) {
          leds[XY(x, y)] = color;
      }
  }
  
  return 8;
}

uint8_t rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  return 8;
}

uint8_t confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  return 8;
}

uint8_t sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
  return 8;
}

uint8_t bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
  
  return 8;
}

uint8_t juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  
  return 8;
}

uint8_t fire() {
    heatMap(HeatColors_p, true);
    
    return 8;
}

CRGBPalette16 icePalette = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::Aqua, CRGB::White);

uint8_t water() {
    heatMap(icePalette, false);
    
    return 8;
}

uint8_t analogClock() {
    dimAll(220);
    
    drawAnalogClock(Time.second(), Time.minute(), Time.hourFormat12(), true, true);
    
    return 8;
}

byte fastSecond = 0;
byte fastMinute = 0;
byte fastHour = 1;

uint8_t fastAnalogClock() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    drawAnalogClock(fastSecond, fastMinute, fastHour, false, false);
        
    fastMinute++;
    
    // fastSecond++;
    
    // if(fastSecond >= 60) {
    //     fastSecond = 0;
    //     fastMinute++;
    // }
     
    if(fastMinute >= 60) {
        fastMinute = 0;
        fastHour++;
    }
    
    if(fastHour >= 13) {
        fastHour = 1;
    }
        
    return 125;
}

uint8_t showSolidColor() {
    fill_solid(leds, NUM_LEDS, solidColor);
    
    return 30;
}

byte thetaUpdate = 0;
byte thetaUpdateFrequency = 0;
byte theta = 0;

byte hueUpdate = 0;
byte hueUpdateFrequency = 5;
byte hue = 0;

byte rotation = 3;

uint8_t scale = 256 / MatrixHeight;

uint8_t maxX = MatrixWidth - 1;
uint8_t maxY = MatrixHeight - 1;

uint8_t waveCount = 0;

uint8_t wave() {
    EVERY_N_SECONDS(10) {
        rotation++;
        if(rotation > 3) rotation = 0;
    }

    int n = 0;

    switch (rotation) {
        case 0:
            scale = 256 / MatrixHeight;
            
            for (int x = 0; x < MatrixWidth; x++) {
                n = quadwave8(x * 2 + theta) / scale;
                leds[XY(x, n)] = ColorFromPalette(palette, x + hue);
                if (waveCount == 2)
                    leds[XY(x, maxY - n)] = ColorFromPalette(palette, x + hue);
            }
            break;

        case 1:
            scale = 256 / MatrixWidth;
            
            for (int y = 0; y < MatrixHeight; y++) {
                n = quadwave8(y * 2 + theta) / scale;
                leds[XY(n, y)] = ColorFromPalette(palette, y + hue);
                if (waveCount == 2)
                    leds[XY(maxX - n, y)] = ColorFromPalette(palette, y + hue);
            }
            break;

        case 2:
            scale = 256 / MatrixHeight;
            
            for (int x = 0; x < MatrixWidth; x++) {
                n = quadwave8(x * 2 - theta) / scale;
                leds[XY(x, n)] = ColorFromPalette(palette, x + hue);
                if (waveCount == 2)
                    leds[XY(x, maxY - n)] = ColorFromPalette(palette, x + hue);
            }
            break;

        case 3:
            scale = 256 / MatrixWidth;
            
            for (int y = 0; y < MatrixHeight; y++) {
                n = quadwave8(y * 2 - theta) / scale;
                leds[XY(n, y)] = ColorFromPalette(palette, y + hue);
                if (waveCount == 2)
                    leds[XY(maxX - n, y)] = ColorFromPalette(palette, y + hue);
            }
            break;
    }

    dimAll(254);

    if (thetaUpdate >= thetaUpdateFrequency) {
        thetaUpdate = 0;
        theta++;
    }
    else {
        thetaUpdate++;
    }

    if (hueUpdate >= hueUpdateFrequency) {
        hueUpdate = 0;
        hue++;
    }
    else {
        hueUpdate++;
    }
    
    return 0;
}

void heatMap(CRGBPalette16 palette, bool up) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(256));
    
    uint8_t cooling = 100;
    uint8_t sparking = 50;
    
    // Array of temperature readings at each simulation cell
    static byte heat[NUM_LEDS];
    
      for (int x = 0; x < MatrixWidth; x++) {
        // Step 1.  Cool down every cell a little
        for (int y = 0; y < MatrixHeight; y++) {
          int xy = XY(x, y);
          heat[xy] = qsub8(heat[xy], random8(0, ((cooling * 10) / MatrixHeight) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int y = 0; y < MatrixHeight; y++) {
          heat[XY(x, y)] = (heat[XY(x, y + 1)] + heat[XY(x, y + 2)] + heat[XY(x, y + 2)]) / 3;
        }

        // Step 2.  Randomly ignite new 'sparks' of heat
        if (random8() < sparking) {
          // int x = (p[0] + p[1] + p[2]) / 3;

          int xy = XY(x, MatrixHeight - 1);
          heat[xy] = qadd8(heat[xy], random8(160, 255));
        }

        // Step 4.  Map from heat cells to LED colors
        for (int y = 0; y < MatrixHeight; y++) {
          int xy = XY(x, y);
          byte colorIndex = heat[xy];

          // Recommend that you use values 0-240 rather than
          // the usual 0-255, as the last 15 colors will be
          // 'wrapping around' from the hot end to the cold end,
          // which looks wrong.
          colorIndex = scale8(colorIndex, 240);

          // override color 0 to ensure a black background?
          if (colorIndex != 0)
            leds[xy] = ColorFromPalette(palette, colorIndex);
        }
      }
}

int oldSecTime = 0;
int oldSec = 0;

void drawAnalogClock(byte second, byte minute, byte hour, boolean drawMillis, boolean drawSecond) {
    if(Time.second() != oldSec){
        oldSecTime = millis();
        oldSec = Time.second();
    }
    
    int millisecond = millis() - oldSecTime;
    
    int secondIndex = map(second, 0, 59, 0, NUM_LEDS);
    int minuteIndex = map(minute, 0, 59, 0, NUM_LEDS);
    int hourIndex = map(hour * 5, 5, 60, 0, NUM_LEDS);
    int millisecondIndex = map(secondIndex + millisecond * .06, 0, 60, 0, NUM_LEDS);
    
    if(millisecondIndex >= NUM_LEDS)
        millisecondIndex -= NUM_LEDS;
    
    hourIndex += minuteIndex / 12;
    
    if(hourIndex >= NUM_LEDS)
        hourIndex -= NUM_LEDS;
    
    // see if we need to reverse the order of the LEDS
    if(flipClock == 1) {
        int max = NUM_LEDS - 1;
        secondIndex = max - secondIndex;
        minuteIndex = max - minuteIndex;
        hourIndex = max - hourIndex;
        millisecondIndex = max - millisecondIndex;
    }
    
    if(secondIndex >= NUM_LEDS)
        secondIndex = NUM_LEDS - 1;
    else if(secondIndex < 0)
        secondIndex = 0;
    
    if(minuteIndex >= NUM_LEDS)
        minuteIndex = NUM_LEDS - 1;
    else if(minuteIndex < 0)
        minuteIndex = 0;
        
    if(hourIndex >= NUM_LEDS)
        hourIndex = NUM_LEDS - 1;
    else if(hourIndex < 0)
        hourIndex = 0;
        
    if(millisecondIndex >= NUM_LEDS)
        millisecondIndex = NUM_LEDS - 1;
    else if(millisecondIndex < 0)
        millisecondIndex = 0;
    
    if(drawMillis)
        leds[millisecondIndex] += CRGB(0, 0, 127); // Blue
        
    if(drawSecond)
        leds[secondIndex] += CRGB(0, 0, 127); // Blue
        
    leds[minuteIndex] += CRGB::Green;
    leds[hourIndex] += CRGB::Red;
}

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++){
    leds[i].nscale8(value);
  }
}

void addGlitter( uint8_t chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}