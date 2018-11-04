#include <Adafruit_NeoPixel.h> // Include the adafruit Neopixel Library
#include <MQTTClient.h>
#include <SPI.h>
#include <WiFi101.h>
#include "arduino_secrets.h" 

#define PIN        12
#define LED_COUNT  14

// Fade ok.
// Sparkle ok - needs less on 0-6 & more delay.
// rain broke
// carousel is too epilectic/stroby - set all > 6 to random color & make 1-6 spin in a rainbow, turn off 0
// snake is broke after ~ #9


const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
unsigned long lastMillis = 0;
const int brightness = 50;
bool rainbowOn = true;
int colorR = 0;
int colorG = 0;
int colorB = 0;
uint16_t TotalSteps = 255;  // total number of steps in the pattern
uint16_t Index;  // current step within the pattern
uint32_t Color1, Color2;  // What colors are in use (used by Fade)

WiFiClient net;
MQTTClient client;

void connect();  // <- predefine connect() for setup()

enum mode {modeFade, modeRain, modeSnake, modeSparkle, modeCarousel};
mode currentMode = modeFade;

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strips = Adafruit_NeoPixel(LED_COUNT, PIN, NEO_GRB + NEO_KHZ800);


void setup() {
  WiFi.setPins(8,7,4,2);
  strips.begin();
  strips.setBrightness(brightness);
  strips.show();
  Serial.begin(115200);
  WiFi.begin(ssid, pass);

  client.begin(IP_ADDRESS, PORT, net);
  client.onMessage(messageReceived);

  connect();
}

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect("led-harness", "", "")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");
  client.subscribe("lights");
}

void messageReceived(String &topic, String &payload) {
  const char* delimiter = ",";
  String incomingMode = payload.substring(0,payload.indexOf(delimiter));
  String colorValue   = payload.substring(incomingMode.length()+2,payload.length());
  Serial.println("topic: " + topic);
  Serial.println("payload: " + incomingMode);
  
  if(colorValue == "rainbow") {
    rainbowOn = true;
    colorR = 0;
    colorG = 0;
    colorB = 0;
    Serial.println("color: " + colorValue);
  }
  else {
    rainbowOn = false;  
    // rainbow not used, get color values
    String colorStringR = colorValue.substring(0, colorValue.indexOf(delimiter));
    colorValue.remove(0,colorStringR.length()+2);
    String colorStringG = colorValue.substring(0, colorValue.indexOf(delimiter));
    colorValue.remove(0,colorStringG.length()+2);
    String colorStringB = colorValue.substring(0, colorValue.indexOf(delimiter));
    Serial.println("colorR: " + colorStringR);
    Serial.println("colorG: " + colorStringG);
    Serial.println("colorB: " + colorStringB);
    colorR = colorStringR.toInt();
    colorG = colorStringG.toInt();
    colorB = colorStringB.toInt();
  }
  trigger(incomingMode.c_str());
}

void trigger(const char* event) {
  if (strcmp(event, "fade") == 0){
     TotalSteps = 400;
     currentMode = modeFade;
  } else if (strcmp(event, "rain") == 0){
     currentMode = modeRain;
  } else if (strcmp(event, "sparkle") == 0){
     currentMode = modeSparkle;
  } else if (strcmp(event, "snake") == 0){
   currentMode = modeSnake;
  } else if (strcmp(event, "carousel") == 0){
   TotalSteps = 255;
   currentMode = modeFade;
  }
}

void loop() {
  switch(currentMode)
  {
    case modeFade:
      Serial.print("fade\n");
      runFade(10);
      break;
     case modeRain:
      Serial.print("rain\n");
      rain(25);
      break;
    case modeSparkle:
      Serial.print("sparkle\n");
      sparkle(80);
      break;
    case modeSnake:
      Serial.print("snake\n");
      snakeLoop(50);
      break;
    default:
      break;
  }
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }
}

// Fill the dots one after the other with a color
void runFade(uint8_t wait) {
  if(rainbowOn == true) {
    uint16_t i, j;
    for(j=0; j<256; j++) {
      for(i=0; i<strips.numPixels(); i++) {
        strips.setPixelColor(i, Wheel((i+j) & 255));
      }
      strips.show();
      delay(wait);
    }
  }
  else {
    Index = 0;
    Color1 = strips.Color(colorR, colorG, colorB);
    Color2 = strips.Color(1,1,1);
    while(Index+50 <= TotalSteps) {
      fadeCycle();  // Fading darker
      Index++;
    }
    while(Index > 50) {
      fadeCycle();  // Fading brighter
      Index--;
    }
  }
}
void fadeCycle() {
  uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
  uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
  uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;
  for(uint16_t i=0; i<strips.numPixels(); i++) {
    strips.setPixelColor(i, strips.Color(red, green, blue));        
  }
  strips.show();
}

// Array of pixels in order of snake animation - 10 in total:
int sineLoop[] = {
   0,1,2,3,4,5,6,7,8,9}; 
   
void snakeLoop(uint8_t wait) {
  uint32_t color = strips.Color(colorR, colorG, colorB);
  if (rainbowOn == true) {
      for(uint16_t i=0; i<strips.numPixels(); i++) {
        strips.setPixelColor(sineLoop[i], 0); // Erase 'tail'
        strips.setPixelColor(sineLoop[(i + 10) % LED_COUNT], Wheel(Index));
        Index++;
        if(Index > 255) {
          Index = 0;
        }
        strips.show();
        delay(wait);
      }
  }
  else {
    for(int i=0; i<LED_COUNT; i++) {
      strips.setPixelColor(sineLoop[i], 0); // Erase 'tail'
      strips.setPixelColor(sineLoop[(i + 10) % LED_COUNT], color); // Draw 'head' pixel
      strips.show();
      delay(wait);
    }
  }
}

// Draw 2 random leds, dim all leds, repeat
void sparkle(uint8_t wait) {
  uint32_t color;
  if (rainbowOn == true) {
    color = strips.Color(random(255), random(255), random(255));
  }
  else {
    color = strips.Color(colorR, colorG, colorB);
  }
  strips.setPixelColor(random(LED_COUNT), color);
  strips.setPixelColor(random(LED_COUNT), color);

  for (int x=0; x<LED_COUNT; x++) {
    strips.setPixelColor(x, DimColor(strips.getPixelColor(x), .80));  
  }
  strips.show();
  delay(wait);
}

uint32_t colors[LED_COUNT];
int strip1[] = {0,1,2,3};
int strip2[] = {4,5,6};
int strip3[] = {7,8};
int strip4[] = {9};

// Moves a raindrop down 1 step
void stepRaindrop(int arrayLength, int ringArray[], float fadeAmount) {
  //first move any ON lights down one on each strip
   for(int x=arrayLength; x > 0 ; x--) {
    if (colors[ringArray[x-1]] != 0) {
       colors[ringArray[x]] = colors[ringArray[x-1]];
       colors[ringArray[x-1]] = DimColor(colors[ringArray[x-1]], fadeAmount);
    }
   }
  // each row: special case, turn off all first lights
  colors[ringArray[0]] = 0;  
}

// Rain Program
void rain(uint8_t wait) {
  uint32_t color;
  if (rainbowOn == true) {
    color = strips.Color(random(255), random(255), random(255));
  }
  else {
    color = strips.Color(colorR, colorG, colorB);
  }
  stepRaindrop(4,strip1, .30);
  stepRaindrop(3,strip2, .30);
  stepRaindrop(2,strip3, .40);
  stepRaindrop(1,strip4, .50);

  
  // turn on light at first position of random strip
  // weighted a bit towards the bigger strips
  int randomOn = random(2);
  if (randomOn == 1) {
    int strip = random(4);
    switch(strip){
      case 0:
        colors[strip1[0]] = color;
      break;
      case 1:
        colors[strip2[0]] = color;
      break;
      case 2:
      case 3:
        colors[strip3[0]] = color;
      break;
      case 4:
      case 5:
        colors[strip4[0]] = color;
      break;
      default:
      break;
    }
  }
  updatestrips();
  delay(wait);
}


// update all lights according to colors[] array
void updatestrips() {
  for(int i=0; i<LED_COUNT; i++) {
    strips.setPixelColor(i, colors[i]);
    strips.show();
  }  
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(int WheelPos) {
  if(WheelPos < 85) {
    return strips.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
    return strips.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strips.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

// Returns the Red component of a 32-bit color
uint8_t Red(uint32_t color){
    return (color >> 16) & 0xFF;
}

// Returns the Green component of a 32-bit color
uint8_t Green(uint32_t color){
    return (color >> 8) & 0xFF;
}

// Returns the Blue component of a 32-bit color
uint8_t Blue(uint32_t color){
    return color & 0xFF;
}

// Return color, dimmed by percentage
uint32_t DimColor(uint32_t color, float dimPercent){
  int red = Red(color) * dimPercent;
  int blue = Blue(color) * dimPercent;
  int green = Green(color) * dimPercent;
  uint32_t dimColor = strips.Color(red, green, blue);
  return dimColor;
}

//DEBUG - Determine pixel positions
//strips.setPixelColor(0, strips.Color(255, 0, 0));
//strips.setPixelColor(12, strips.Color(0, 255, 0));
//strips.setPixelColor(23, strips.Color(0, 0, 255));
//strips.setPixelColor(24, strips.Color(255, 0, 0));
//strips.setPixelColor(31, strips.Color(0, 255, 0));
//strips.setPixelColor(39, strips.Color(0, 0, 255));
//strips.setPixelColor(40, strips.Color(255, 0, 0));
//strips.setPixelColor(45, strips.Color(0, 255, 0));
//strips.setPixelColor(51, strips.Color(0, 0, 255));
//strips.show();