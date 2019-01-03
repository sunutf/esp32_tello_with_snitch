 
 /*Hardware setup:
 snitch --------- ESP32
 VDD ---------------------- 3.3V
 SDA ----------------------- SDA(23)
 SCL ----------------------- SCL(22)
 GND ---------------------- GND*/
#include "Wire.h"
#include "Adafruit_TCS34725.h"

/*========================================================================*/
//Snitch part
/*========================================================================*/
//snitch_lux_t[Quadrant][UpDown][Degree]
//[Quadrant] = [FrontRight,FrontLeft,BackLeft,BackRight]
//[UpDown] = [Center, Up, Down]
//[Degree] = [0, 45, 90, 135]
#define FR 0
#define FL 1
#define BR 2
#define BL 3

#define CENT 0
#define UP   1
#define DOWN 2

#define DEG0   0
#define DEG45  1
#define DEG90  2
#define DEG135 3

float snitch_lux_t[4][3][4] = {0};

void snitchConMux2Table(int mux_id, int mux_ch, float lux){
  if(mux_id > 7) return ;
  int quad = 0;
  int updown = 0;
  int deg = 0;
  
  //Quadrant
  quad = mux_id/2;

  //Upside
  mux_id = mux_id%2;
  if(6 == mux_ch | 7 == mux_ch){
    updown = CENT;
  }
  else{
    if(0 == mux_id)      updown = UP;
    else if(1 == mux_id) updown = DOWN;
  }

  //Deg
  //0,1,2,3
  if(UP == updown | DOWN == updown){
    deg = mux_ch;
  }
  else{
    if(0 == mux_id)      deg = 7 - mux_ch; //mux ch = 6 or 7
    else if(1 == mux_id) deg = 9 - mux_ch;
  }

  snitch_lux_t[quad][updown][deg] = lux;
}


/*========================================================================*/
//TCS34725 - Lux Sensor
/*========================================================================*/

//
// An experimental wrapper class that implements the improved lux and color temperature from 
// TAOS and a basic autorange mechanism.
//
// Written by ductsoup, public domain
//

// RGB Color Sensor with IR filter and White LED - TCS34725
// I2C 7-bit address 0x29, 8-bit address 0x52
//
// http://www.adafruit.com/product/1334
// http://learn.adafruit.com/adafruit-color-sensors/overview
// http://www.adafruit.com/datasheets/TCS34725.pdf
// http://www.ams.com/eng/Products/Light-Sensors/Color-Sensor/TCS34725
// http://www.ams.com/eng/content/view/download/265215 <- DN40, calculations
// http://www.ams.com/eng/content/view/download/181895 <- DN39, some thoughts on autogain
// http://www.ams.com/eng/content/view/download/145158 <- DN25 (original Adafruit calculations)
//
// connect LED to digital 4 or GROUND for ambient light sensing
// connect SCL to analog 5
// connect SDA to analog 4
// connect Vin to 3.3-5V DC
// connect GROUND to common ground

// some magic numbers for this device from the DN40 application note
#define TCS34725_R_Coef 0.136 
#define TCS34725_G_Coef 1.000
#define TCS34725_B_Coef -0.444
#define TCS34725_GA 1.0
#define TCS34725_DF 310.0
#define TCS34725_CT_Coef 3810.0
#define TCS34725_CT_Offset 1391.0


//#define AUTO_RANGE_ON 1

// Autorange class for TCS34725
class tcs34725 {
public:
  tcs34725(void);

  boolean begin(void);
  void getData(void);  

  boolean isAvailable, isSaturated;
  uint16_t againx, atime, atime_ms;
  uint16_t r, g, b, c;
  uint16_t ir; 
  uint16_t r_comp, g_comp, b_comp, c_comp;
  uint16_t saturation, saturation75;
  float cratio, cpl, ct, lux, maxlux;
  
private:
  struct tcs_agc {
    tcs34725Gain_t ag;
    tcs34725IntegrationTime_t at;
    uint16_t mincnt;
    uint16_t maxcnt;
  };
  static const tcs_agc agc_lst[];
  uint16_t agc_cur;

  void setGainTime(void);  
  Adafruit_TCS34725 tcs;    
};
//
// Gain/time combinations to use and the min/max limits for hysteresis 
// that avoid saturation. They should be in order from dim to bright. 
//
// Also set the first min count and the last max count to 0 to indicate 
// the start and end of the list. 
//
const tcs34725::tcs_agc tcs34725::agc_lst[] = {
  #ifndef AUTO_RANGE_ON
  { TCS34725_GAIN_16X, TCS34725_INTEGRATIONTIME_2_4MS,     0, 20000 },
  #endif
  { TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_700MS,     0, 20000 },
  { TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_154MS,  4990, 63000 },
  { TCS34725_GAIN_16X, TCS34725_INTEGRATIONTIME_154MS, 16790, 63000 },
  { TCS34725_GAIN_4X,  TCS34725_INTEGRATIONTIME_154MS, 15740, 63000 },
  { TCS34725_GAIN_1X,  TCS34725_INTEGRATIONTIME_154MS, 15740, 0 }
};
tcs34725::tcs34725() : agc_cur(0), isAvailable(0), isSaturated(0) {
}
/*sampling time OPTION
    case TCS34725_INTEGRATIONTIME_2_4MS:

    case TCS34725_INTEGRATIONTIME_24MS:
  
    case TCS34725_INTEGRATIONTIME_50MS:
    
    case TCS34725_INTEGRATIONTIME_101MS:
 
    case TCS34725_INTEGRATIONTIME_154MS:
 
    case TCS34725_INTEGRATIONTIME_700MS:
*/

// initialize the sensor
boolean tcs34725::begin(void) {
  tcs = Adafruit_TCS34725(agc_lst[agc_cur].at, agc_lst[agc_cur].ag);
  if ((isAvailable = tcs.begin())) 
    setGainTime();
  return(isAvailable);
}

// Set the gain and integration time
void tcs34725::setGainTime(void) {
  tcs.setGain(agc_lst[agc_cur].ag);
  tcs.setIntegrationTime(agc_lst[agc_cur].at);
  atime = int(agc_lst[agc_cur].at);
  atime_ms = ((256 - atime) * 2.4);  
  switch(agc_lst[agc_cur].ag) {
  case TCS34725_GAIN_1X: 
    againx = 1; 
    break;
  case TCS34725_GAIN_4X: 
    againx = 4; 
    break;
  case TCS34725_GAIN_16X: 
    againx = 16; 
    break;
  case TCS34725_GAIN_60X: 
    againx = 60; 
    break;
  }        
}

// Retrieve data from the sensor and do the calculations
void tcs34725::getData(void) {
  // read the sensor and autorange if necessary
  tcs.getRawData(&r, &g, &b, &c);
  while(1) {
    #ifdef AUTO_RANGE_ON
    if (agc_lst[agc_cur].maxcnt && c > agc_lst[agc_cur].maxcnt) 
      agc_cur++;
    else if (agc_lst[agc_cur].mincnt && c < agc_lst[agc_cur].mincnt)
      agc_cur--;
    else break;
    #endif

    setGainTime(); 
    delay((256 - atime) * 2.4 * 2); // shock absorber
    tcs.getRawData(&r, &g, &b, &c);
    break;    
  }

  // DN40 calculations
  ir = (r + g + b > c) ? (r + g + b - c) / 2 : 0;
  r_comp = r - ir;
  g_comp = g - ir;
  b_comp = b - ir;
  c_comp = c - ir;   
  cratio = float(ir) / float(c);

  saturation = ((256 - atime) > 63) ? 65535 : 1024 * (256 - atime);
  saturation75 = (atime_ms < 150) ? (saturation - saturation / 4) : saturation;
  isSaturated = (atime_ms < 150 && c > saturation75) ? 1 : 0;
  cpl = (atime_ms * againx) / (TCS34725_GA * TCS34725_DF); 
  maxlux = 65535 / (cpl * 3);

  lux = (TCS34725_R_Coef * float(r_comp) + TCS34725_G_Coef * float(g_comp) + TCS34725_B_Coef * float(b_comp)) / cpl;
  ct = TCS34725_CT_Coef * float(b_comp) / float(r_comp) + TCS34725_CT_Offset;
}

tcs34725 rgb_sensor;

///////////tca part

//extern "C" { 
//  #include "utility/twi.h"  // from Wire library, so we can do bus scanning
//}

#define TCAADDR 0x70
//
//void tcaAllSelect(void){
//  tcaSelect();
//}

void tcaSelectAllCh(uint8_t id) {
  if (id > 7) return;
 
  Wire.beginTransmission(TCAADDR+id);
  Wire.write(255);
  Wire.endTransmission();  
}
 
void tcaSelect(uint8_t id, uint8_t ch) {
  if (ch > 7) return;
 
  Wire.beginTransmission(TCAADDR+id);
  Wire.write(1 << ch);
  Wire.endTransmission();  
}

void tcaDeSelect(uint8_t id){
  Wire.beginTransmission(TCAADDR+id);
  Wire.write(0);  // no channel selected
  Wire.endTransmission(); 
}
// standard Arduino setup()
void setup()
{
  while (!Serial);
  delay(1000);

  //Wire.begin();
  Wire.begin(23, 22, 400000);
   
  Serial.begin(115200);
  Serial.println("\nTCAScanner ready!");

  sendToAllSet();
//
//  tcaDeSelect(4);
//  tcaDeSelect(5);
//  tcaSelectAllCh(5);
//  delay(5);
//  rgb_sensor.begin();
//  delay(5);
}
 
void loop() 
{ 
  delay(1000);
   Serial.println("------------------");
//   for(int id = 0 ; id<8; id++){
//    readLuxFromMux(id);
//   }
//   
     for(int id =6 ; id<8; id++){
       Serial.printf("---------%d---------", id);

       for(int ch = 0 ; ch<8; ch++){
  
         //we don't have sense in ch = 4,5, don't need to spend time here
     
         if(ch == 4 | ch ==5) continue;
         tcaSelect(id,ch);
  //       delay(5);
         
         rgb_sensor.getData();
    
         Serial.print(("Ch:")); 
         Serial.print(ch); 
         Serial.print((" Lux:")); 
         Serial.println(rgb_sensor.lux);
             
       }
       tcaDeSelect(id);

   }
}

void sendToAllSet(void)
{
  for(int id=0; id<8; id++){
    tcaDeSelect(id);
  }
  
  for(int id=0; id<8; id++){
    tcaSelectAllCh(id);
//    delay(1);
    rgb_sensor.begin();
    
    tcaDeSelect(id);
//    delay(1);
  }
}

void readLuxFromMux(int id){
   for(int ch = 0 ; ch<8; ch++){
     //we don't have sense in ch = 4,5, don't need to spend time here
    if(ch == 4 | ch ==5) continue;
    
    tcaSelect(id,ch);
//    delay(1);
    rgb_sensor.getData();
    snitchConMux2Table(id, ch, rgb_sensor.lux);
   }
  tcaDeSelect(id);
//  delay(5);
}



