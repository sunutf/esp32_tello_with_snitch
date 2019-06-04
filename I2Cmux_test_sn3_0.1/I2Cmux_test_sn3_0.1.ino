

 /*Hardware setup:
 snitch --------- ESP32
 VDD ---------------------- 3.3V
 SDA ----------------------- SDA(23)
 SCL ----------------------- SCL(22)
 GND ---------------------- GND*/
#include "Wire.h"
#include "Adafruit_TCS34725.h"
#include "leviosa_lkup.h"
#include "EEPROM.h"
#define EEPROM_SIZE 256

#define ID 6
/*========================================================================*/
//Snitch part
/*========================================================================*/
//
//[Quadrant] = [FrontRight,FrontLeft,BackLeft,BackRight]
//[UpDown] = [Center, Up, Down]
//[Degree] = [0, 45, 90, 135]
#define FR 0
#define FL 1
#define BR 2
#define BL 3

#define MID 0
#define TOP   1
#define BOTTOM 2

#define DEG0   0
#define DEG45  1
#define DEG90  2
#define DEG135 3

typedef struct{
    float raw_lux[4];
    float ambi_lux;
    float polar_lux;
    
}snitch_surf_t;

typedef struct{
  snitch_surf_t top, mid, bottom;
 
}snitch_t;

snitch_t snitch_dir[4];
float raw_id[4];
float surf_src[12];
float surf_amb[12];

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
    updown = MID;
  }
  else{
    if(0 == mux_id)      updown = TOP;
    else if(1 == mux_id) updown = BOTTOM;
  }

  //Deg
  //0,1,2,3
  if(TOP == updown | BOTTOM == updown){
    deg = mux_ch;
  }
  else{
    if(0 == mux_id)      deg = 7 - mux_ch; //mux ch = 6 or 7
    else if(1 == mux_id) deg = 9 - mux_ch;
  }

  if(updown == MID)         snitch_dir[quad].mid.raw_lux[deg]     = lux;
  else if(updown == TOP)    snitch_dir[quad].top.raw_lux[deg]     = lux;
  else if(updown == BOTTOM) snitch_dir[quad].bottom.raw_lux[deg]  = lux;
}

/*========================================================================*/
//Leviosa part
/*========================================================================*/
int extractCaliLuxCalc(snitch_surf_t* surf_set, int addr){
  int16_t diff_luxA;
  int16_t diff_luxB;
  float src_lux[4];
  float amb_lux[4];
  float A;
  float B;
  float theta[4];

  theta[0] = EEPROM.readFloat(addr);
  addr += 4;
  theta[1] = EEPROM.readFloat(addr);
  addr += 4;
  theta[2] = EEPROM.readFloat(addr);
  addr += 4;
  theta[3] = EEPROM.readFloat(addr);
  addr += 4;

//  Serial.println("=======");
//  for(int i =0; i<4; i++){
//    Serial.print(i);
//    Serial.print(" : ");
//    Serial.println(theta[i]);
//  }
//  Serial.println();

  A = (1-theta[3])/theta[1] - (1-theta[2])/theta[0];
  B = theta[0]/(1-theta[2]) - theta[1]/(1-theta[3]);
  for(int deg =0; deg<1; deg++){
    diff_luxA = surf_set->raw_lux[deg%4] - surf_set->raw_lux[(deg+1)%4];
    diff_luxB = surf_set->raw_lux[deg%4] - surf_set->raw_lux[(deg+2)%4];

    A = 2*(diff_luxB/theta[1]-diff_luxA/theta[0])/A;
    B = 2*(diff_luxB/(1-theta[3])-diff_luxA/(1-theta[2]))/B;
    
    surf_set->polar_lux = sqrtf(A*A + B*B);
    surf_set->ambi_lux = (2*surf_set->raw_lux[deg%4]-surf_set->polar_lux-A)/2.0f;
   
  }
  return addr;
}


typedef struct{
  float value;
  uint8_t index;
}rank_lux_t;

typedef struct{
  rank_lux_t src;
  rank_lux_t amb;
}rank_t;

rank_t rank_lux_top3[3] = {0,};

void leviosa_boardCalcCoord(float* lux_data_set)
{
  float largest_value[3];
  uint8_t largest_index[3];
  float *source_lux = lux_data_set;


  rank_lux_top3[0].src.value = rank_lux_top3[1].src.value = rank_lux_top3[2].src.value = 0;
  rank_lux_top3[0].amb.value = rank_lux_top3[1].amb.value = rank_lux_top3[2].amb.value = 0;
  
  if(source_lux == surf_src)
  {
    for(uint8_t i = 0; i < 3; i++)
    {
      float max = 0;
      uint8_t index = 255;
  
      for(uint8_t id = 0; id < 12; id++)
      {
        float cur_source = source_lux[id];
        if(cur_source < 50 ) cur_source = 0;
        if(max < cur_source && cur_source != rank_lux_top3[0].src.value && cur_source != rank_lux_top3[1].src.value && cur_source != rank_lux_top3[2].src.value)
        {
          max = cur_source;
          index = id;
        }
      }
      rank_lux_top3[i].src.value = max;
      rank_lux_top3[i].src.index = index;
    }
  }
  else if(source_lux == surf_amb) 
  {
    for(uint8_t i = 0; i < 3; i++)
    {
      float max = 0;
      uint8_t index = 255;
  
      for(uint8_t id = 0; id < 12; id++)
      {
        float cur_source = source_lux[id];
        if(cur_source < 50 ) cur_source = 0;
        if(max < cur_source && cur_source != rank_lux_top3[0].amb.value && cur_source != rank_lux_top3[1].amb.value && cur_source != rank_lux_top3[2].amb.value)
        {
          max = cur_source;
          index = id;
        }
      }
      rank_lux_top3[i].amb.value = max;
      rank_lux_top3[i].amb.index = index;
    }
  }

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

#define A_TIME TCS34725_INTEGRATIONTIME_50MS
const tcs34725::tcs_agc tcs34725::agc_lst[] = {
  #ifndef AUTO_RANGE_ON
  { TCS34725_GAIN_1X, A_TIME,     0, 0  },
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
//    delay((256 - atime) * 2.4 * 2); // shock absorber
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


#define TCAADDR 0x70

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
  Serial.begin(115200);
  Serial.print("start\n");

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }
  Wire.begin(23, 22, 400000);
  sendToAllSet();
}
 
 int pre_time, process_time = 0;
 int addr=0;
void loop() 
{ 
  pre_time = millis();
  delay(10);
  for(int id = 0 ; id<8; id++){
    readLuxFromMux(id);
  }

   addr = 4;
   for(int quad=0; quad<4; quad++){
      Serial.printf("----Quad : %d-----\n", quad);
      //    extractLuxCalc(&snitch_dir[quad].top);
      //    extractLuxCalc(&snitch_dir[quad].mid);
      //    extractLuxCalc(&snitch_dir[quad].bottom);
      
      addr = extractCaliLuxCalc(&snitch_dir[quad].mid,addr);
      addr = extractCaliLuxCalc(&snitch_dir[quad].top,addr);
      addr = extractCaliLuxCalc(&snitch_dir[quad].bottom,addr);
      
      Serial.print("TOP SRC: ");
      Serial.print(snitch_dir[quad].top.polar_lux);
      Serial.print("  AMBI: "); 
      Serial.println(snitch_dir[quad].top.ambi_lux);
      Serial.print("MID SRC: ");
      Serial.print(snitch_dir[quad].mid.polar_lux);
      Serial.print("  AMBI: "); 
      Serial.println(snitch_dir[quad].mid.ambi_lux);
      Serial.print("BOT SRC: ");
      Serial.print(snitch_dir[quad].bottom.polar_lux);
      Serial.print("  AMBI: "); 
      Serial.println(snitch_dir[quad].bottom.ambi_lux);
      
      surf_src[quad*3] = snitch_dir[quad].mid.polar_lux;
      surf_src[(quad*3 + 1)] = snitch_dir[quad].top.polar_lux;
      surf_src[(quad*3 + 2)] = snitch_dir[quad].bottom.polar_lux;

      surf_amb[quad*3] = snitch_dir[quad].mid.ambi_lux;
      surf_amb[(quad*3 + 1)] = snitch_dir[quad].top.ambi_lux;
      surf_amb[(quad*3 + 2)] = snitch_dir[quad].bottom.ambi_lux;
   }
   
   leviosa_boardCalcCoord(surf_src);
   for(int i = 0; i<3; i++){
     Serial.printf("  rank %d : ", i);
     Serial.print(rank_lux_top3[i].src.index);
     Serial.printf(" : ");
     Serial.println(rank_lux_top3[i].src.value);
   }
   Serial.println("===================");
   leviosa_boardCalcCoord(surf_amb);
   for(int i = 0; i<3; i++){
     Serial.printf("  rank %d : ", i);
     Serial.print(rank_lux_top3[i].amb.index);
     Serial.printf(" : ");
     Serial.println(rank_lux_top3[i].amb.value);
   }
   Serial.println("\n\n");
   
   process_time = millis()-pre_time;
   Serial.println(process_time);

   
  //DEBUG//
//   for(int quad = 0; quad <4 ; quad++){
//    Serial.print("\n----Quad : ");
//    Serial.print(quad);
//    Serial.println("------");
//    
//    Serial.println("-TOP-");
//    for(int deg = 0; deg <4 ; deg++){
//      Serial.print(deg*45);
//      Serial.print(" : ");
//      Serial.println(snitch_dir[quad].top.raw_lux[deg]);
//    }
//    
//    Serial.println("-MID-");
//    for(int deg = 0; deg <4 ; deg++){
//      Serial.print(deg*45);
//      Serial.print(" : ");
//      Serial.println(snitch_dir[quad].mid.raw_lux[deg]);
//    }
//    
//    Serial.println("-BOTTOM-");
//    for(int deg = 0; deg <4 ; deg++){
//      Serial.print(deg*45);
//      Serial.print(" : ");
//      Serial.println(snitch_dir[quad].bottom.raw_lux[deg]);
//    }
//  }
}

void sendToAllSet(void)
{
  for(int id=0; id<8; id++){
    tcaDeSelect(id);
  }
  
  for(int id=0; id<8; id++){
    tcaSelectAllCh(id);
    rgb_sensor.begin();
    
    
    tcaDeSelect(id);
  }
  int a_time = A_TIME;
  switch (a_time)
  {
    case TCS34725_INTEGRATIONTIME_2_4MS:
      delay(4);
      break;
    case TCS34725_INTEGRATIONTIME_24MS:
      delay(25);
      break;
    case TCS34725_INTEGRATIONTIME_50MS:
      delay(51);
      break;
    case TCS34725_INTEGRATIONTIME_101MS:
      delay(102);
      break;
    case TCS34725_INTEGRATIONTIME_154MS:
      delay(155);
      break;
    case TCS34725_INTEGRATIONTIME_700MS:
      delay(701);
      break;
   }
}

void readLuxFromMux(int id)
{
   for(int ch = 0 ; ch<8; ch++){
     //we don't have sensors in ch = 4,5, don't need to spend time here
    if(ch == 4 | ch ==5) continue;
    
    tcaSelect(id,ch);
    rgb_sensor.getData();
    snitchConMux2Table(id, ch, rgb_sensor.lux);
   }
   tcaDeSelect(id);
}


void readLuxFromID(int id)
{
   int mux_id = 2*(id/3);
   switch(id%3){
    case 0 :
      tcaSelect(mux_id, 7);
      rgb_sensor.getData();
      snitchConMux2Table(mux_id, 7, rgb_sensor.lux);
      tcaSelect(mux_id,6);
      rgb_sensor.getData();
      snitchConMux2Table(mux_id, 6, rgb_sensor.lux);
      
      tcaDeSelect(mux_id);
      delay(10);
      mux_id += 1;

      tcaSelect(mux_id, 7);
      rgb_sensor.getData();
      snitchConMux2Table(mux_id, 7, rgb_sensor.lux);
      tcaSelect(mux_id,6);
      rgb_sensor.getData();
      snitchConMux2Table(mux_id, 6, rgb_sensor.lux);
      tcaDeSelect(mux_id);

      break;
      
   case  1:
      for(int ch = 0; ch<4; ch++){
        tcaSelect(mux_id,ch);
        rgb_sensor.getData();
        snitchConMux2Table(mux_id, ch, rgb_sensor.lux);
      }
      tcaDeSelect(mux_id);
      break;

    case  2:
      for(int ch = 0; ch<4; ch++){
        tcaSelect(mux_id,ch);
        rgb_sensor.getData();
        snitchConMux2Table(mux_id, ch, rgb_sensor.lux);
      }
      tcaDeSelect(mux_id);
      break;
   }
}

