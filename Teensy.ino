#include <PulsePosition.h>
#include <SPI.h>
#include <SD.h>
#include <Audio.h>
#include <max7456.h>

PulsePositionOutput PPM;
File myFile;
Max7456 osd;

//defining pins
#define PPP 23 //PPM out pin
#define analogJoy 4 //number of analog channel
#define audio DAC0
#define telemetryIn 0
#define telemetryOut 1
const byte rotaryTrim[5]={2,3,4,5,6};
const byte rotaryBut[3]={44,45,46};
const byte joystick[analogJoy]={16,17,14,15};
const byte switches[5]={28,29,30,31,32};

//PPM and joystick parameters
const int potMin = 1000;
const int potMax = 2000;
const int defaultPot = 1500;
int calibration[analogJoy][2];
int Trim[analogJoy];
int lastTrim[analogJoy]; 
int defTrim[analogJoy];
int endPoints[analogJoy][2];

//Joystick smoothing
byte numReadings;                 // Analog Smoothing stuff number of averaged smoothing
int readings[15][analogJoy];      // the readings from the analog input 
byte readIndex = 0;               // the index of the current reading
int total[analogJoy]={0,0,0,0};             // the running total
int average[analogJoy]={0,0,0,0};

//Model
int model;
char modelName[9];
char loc[18];
byte modPos;
const int chipSelect = BUILTIN_SDCARD;
byte key;

//Trim Rotarry Encoders

//RGB LED

//Menu
byte page = 1;
byte pos = 1;
bool onClick = 0;
bool onClick2= 0;
bool butClick=0;
double bounce;
double bounce1;
double bounce2;
double delay1;
int debCount = 50;
bool rotUp = 0;
bool rotDown = 0;
char buf[4];
int joyAnim[4]={1,1,1,1};
byte limit[4]={1,1,1,1};

void setup() {
  //Setting up osd stuffs
  
  
  //play intro sound here
  delay(2000);
  int ii = millis();
  
  //Setting up models
  if (!SD.begin(chipSelect)) {
    osd.print("SD Card not detected",10,6);
    delay(2000);
    return;
  }
  myFile = SD.open("cal.txt", FILE_READ);
  if (myFile){
    model = SDReadInt(0);
    for (byte i = 1;i<=4;i++){
      calibration[i-1][0] = SDReadInt(i);
      calibration[i-1][1] = SDReadInt(i+4);
    }    
  }
  else{
    osd.print("error reading calibration",1,7);
    delay(2000);
  }
  sprintf(loc,"models/model%d.txt",model);
  myFile.close();
  myFile = SD.open(loc,FILE_READ);
  numReadings = SDReadInt(0);
  for (byte i=1; i<=4 ; i++){
    defTrim[i-1]=SDReadInt(i);
    Trim[i-1]=defTrim[i-1];
    endPoints[i-1][0]=SDReadInt(i+4)-1000;
    endPoints[i-1][1]=SDReadInt(i+8)-1000;
  }
  myFile.seek(26);
  for (byte i=0 ; i<8 ; i++){
    modelName[i]=myFile.read();
  }
  myFile.close();
  
  
  //Pinmodes
  for (byte i=0;i<5;i++){
    pinMode(switches[i],INPUT_PULLUP);
  } 
  for (byte i=0;i<5;i++){
    pinMode(rotaryTrim[i],INPUT_PULLUP);
  }
  for (byte i=0;i<3;i++){
    pinMode(rotaryBut[i],INPUT_PULLUP);
  }
  pinMode(47,OUTPUT); //VRX Mosfet
  pinMode(48,OUTPUT); //DVR Mosfet
  
  //Setting up PPM and Default PPM values
  for (byte i = 0 ; i< 4 ; i++){
    for (byte ii= 0 ; i< 15 ; i++){
      readings[i][ii]=0;
    }
  }
  PPM.begin(PPP);
  for (byte i=1;i<=8;i++){
    PPM.write(i,defaultPot);
  }
  
  //
  SPI.begin();
  osd.init(10);
  osd.setDisplayOffsets(45,60);
  osd.setBlinkParams(_8fields, _BT_BT);
  osd.activateOSD();
  osd.print("NikoTx V0.2",7,5);
  
  while (millis()-ii <=2000){
    //do nothing (delay)
  }
  bounce = millis();
  bounce1= millis();
  bounce2= millis();
  delay1=  millis();
  homescreen();
}

void loop() {
  readPots();
  readRot();
  readSwitch();
    
  //All the menu stuff
  if (rotUp == 1){
    switch (page){
      case 1:{
        if (pos>1){
          osd.printMax7456Char(0x00,4,pos+1);
          osd.printMax7456Char(0xD9,4,pos);
          pos--;
        }
      }
      break;
      case 2:{
        if (pos == 2 && onClick && model >1){
          model--;
          osd.printMax7456Char(char(16+model),14,11,true);
        }
        else if (onClick == 0){
          if (pos>1){
            osd.printMax7456Char(0x00,3,pos+9);
            osd.printMax7456Char(0xD9,3,pos+8);
            pos--;
          }
        }
        else if(onClick ==1 && pos ==1){
          if (key == 27){
            osd.printMax7456Char(0x00,10,7);
            osd.printMax7456Char(0xDB,key-1,6);
            key--;
          }
          else if (key>1){
            osd.printMax7456Char(0x00,key,6);
            osd.printMax7456Char(0xDB,key-1,6);
            key--;
          }
        }
      }
      break;
      case 3:{
        if (!onClick && pos > 1){
          osd.printMax7456Char(0x00,3,pos+7);
          osd.printMax7456Char(0xD9,3,pos+6);
          pos--;
        }
        else if (onClick && pos == 4 && numReadings >1){
          numReadings--;
          sprintf(buf,"%d",numReadings);
          osd.print(buf,15,11,true);
        }
        else if (onClick && pos ==2){
          if (!onClick2 && key>1){
            osd.printMax7456Char(0x00,(key>=5)*7+13,3+key-(key>=5)*4);
            key--;
            osd.printMax7456Char(0xD9,(key>=5)*7+13,3+key-(key>=5)*4);
          }
          else if(onClick2 && endPoints[key-1-(key>=5)*4][key >=5] > -99 ){
            endPoints[key-1-(key>=5)*4][key >=5]--;
            sprintf(buf,"%d",endPoints[key-1-(key>=5)*4][key >=5]);
            osd.print("   ",17+(key>=5)*7,3+key-(key>=5)*4);
            osd.print(buf,17+(key>=5)*7,3+key-(key>=5)*4,true);
          }
        }
      }
      break;
    }
    rotUp = 0;
  }
   
  if (rotDown == 1){
    switch (page){
      case 1:{
        if (pos<8){
          osd.printMax7456Char(0x00,4,pos+1);
          osd.printMax7456Char(0xD9,4,pos+2);
          pos++;   
        }
      }
      break;
      case 2:{
        if(pos ==2 && onClick && model<5){
          model++;
          osd.printMax7456Char(char(16+model),14,11,true);
          
        }
        else if (onClick==0){
          if (pos<3){
            osd.printMax7456Char(0x00,3,pos+9);
            osd.printMax7456Char(0xD9,3,pos+10);
            pos++;
          }
        }
        else if(onClick && pos ==1){
          if (key<26){
            osd.printMax7456Char(0x00,key,6);
            osd.printMax7456Char(0xDB,key+1,6);
            key++;
          }
          else if (key == 26 && pos ==1){
            osd.printMax7456Char(0x00,key,6);
            osd.printMax7456Char(0xD9,10,7);
            key++;
          }
        }
      }
      break;
      case 3:{
        if (!onClick && pos < 5){
          osd.printMax7456Char(0x00,3,pos+7);
          osd.printMax7456Char(0xD9,3,pos+8);
          pos++;
        }
        else if (onClick && pos == 4 && numReadings <15){
          numReadings++;
          sprintf(buf,"%d",numReadings);
          osd.print(buf,15,11,true);
        }
        else if (onClick && pos ==2){
          if (!onClick2 && key<8){
            osd.printMax7456Char(0x00,(key>=5)*7+13,3+key-(key>=5)*4);
            key++;
            osd.printMax7456Char(0xD9,(key>=5)*7+13,3+key-(key>=5)*4);
          }
          else if(!onClick2 && key ==8){
            osd.printMax7456Char(0x00,(key>=5)*7+13,3+key-(key>=5)*4);
            osd.printMax7456Char(0xD9,3,9);
            onClick=0;
            pos = 2;
            key =1;
          }
          else if(onClick2 && endPoints[key-1-(key>=5)*4][key >=5] < 99){
            endPoints[key-1-(key>=5)*4][key >=5]++;
            sprintf(buf,"%d",endPoints[key-1-(key>=5)*4][key >=5]);
            osd.print("   ",17+(key>=5)*7,3+key-(key>=5)*4);
            osd.print(buf,17+(key>=5)*7,3+key-(key>=5)*4,true);
          }
        }
      }
      break;
    }
    rotDown=0;
  }
    
  

  if(digitalRead(rotaryBut[2]) == 0 && millis()-bounce1>300){
    switch (page){
      case 1:{
        switch (pos){
          case 1:{
            page = 2;
            pos = 1;
            onClick=0;
            clearS(); 
            osd.print("ABCDEFGHIJKLMNOPQRSTUVWXYZ",1,5);
            osd.print("Model Name: ",4,10);
            osd.print(modelName,16,10);
            osd.print("Model no: ",4,11);
            osd.print("Exit",4,12);
            osd.printMax7456Char(0xD9,3,10);            
            osd.printMax7456Char(char(16+model),14,11);
            osd.print("done",11,7);
          }
          break;
          case 2:{
            page = 3;
            pos = 1;
            onClick = 0;
            clearS();
            osd.print("Joystick",1,0);
            for (byte i = 2; i <=11 ; i=i+3){
              osd.printMax7456Char(0x5C,i,7);
              osd.printMax7456Char(0x5D,i,1);
              osd.printMax7456Char(0x99,i-1,1);
              osd.printMax7456Char(0x9A,i+1,1);
              osd.printMax7456Char(0x9B,i+1,7);
              osd.printMax7456Char(0x9C,i-1,7);
              for (byte j = 1; j<=5 ; j++){
                osd.printMax7456Char(0x5B,i+1,j+1);
                osd.printMax7456Char(0X5E,i-1,j+1);
              }
            }
            osd.print("Save Trim",4,8);
            osd.print("End Points",4,9);
            osd.print("Calibrate",4,10);
            osd.print("Smoothing:",4,11);
            osd.print("Exit",4,12);
            osd.printMax7456Char(0xD9,3,8);
            sprintf(buf,"%d",numReadings);
            osd.print(buf,15,11);
            osd.print("Trim T:",16,0);
            osd.print("Trim R:",16,1);
            osd.print("Trim E:",16,2);
            osd.print("Trim A:",16,3);
            osd.print("T :",14,4);
            osd.printMax7456Char(0xCF,15,4);
            osd.print("R :",14,5);
            osd.printMax7456Char(0xCF,15,5);
            osd.print("E :",14,6);
            osd.printMax7456Char(0xCF,15,6);
            osd.print("A :",14,7);
            osd.printMax7456Char(0xCF,15,7);
            osd.print("T :",21,4);
            osd.printMax7456Char(0xCD,22,4);
            osd.print("R :",21,5);
            osd.printMax7456Char(0xCD,22,5);
            osd.print("E :",21,6);
            osd.printMax7456Char(0xCD,22,6);
            osd.print("A :",21,7);
            osd.printMax7456Char(0xCD,22,7);
            for (byte i = 0; i<4;i++){
              sprintf(buf,"%d",endPoints[i][0]);
              osd.print(buf,17,4+i);
              sprintf(buf,"%d",endPoints[i][1]);
              osd.print(buf,24,4+i);
              sprintf(buf,"%d",Trim[i]);
              osd.print(buf,23,i);
              lastTrim[i]=Trim[i];
            }
          }
          break;
          case 3:{
            page=4;
            clearS();
            digitalWrite(47,1);
          }
          break;
          case 5:{
            page=6;
            clearS();
            digitalWrite(48,1);
          }
          break;
        }
        break;
      }
      case 2:{
        switch (pos){
          case 1:{
            if (!onClick){
              onClick = 1;
              key = 1;
              for (byte i=0;i<8;i++){
                modelName[i]=' ';
                osd.printMax7456Char(0x00,16+i,10);
              }
              osd.printMax7456Char(0x67,16,10,true);
              osd.printMax7456Char(0x00,3,10);
              osd.printMax7456Char(0xDB,1,6);
              modPos=0;
            }
            else if (key==27){
              osd.printMax7456Char(0x00,16+modPos,10);
              osd.printMax7456Char(0x00,10,7);
              osd.printMax7456Char(0xD9,3,10);
              myFile = SD.open(loc,FILE_WRITE);
              myFile.seek(26);
              myFile.print(modelName);
              myFile.close();
              pos=1;
              onClick=0;
            }
            else if (modPos<7){
              modelName[modPos]=64+key;
              modPos++;
              osd.printMax7456Char(0x67,16+modPos,10,true);
              osd.printMax7456Char(char(32+key),15+modPos,10);            
            }
            else{
              modelName[modPos]=64+key;
              osd.printMax7456Char(char(32+key),15+modPos,10);
              osd.printMax7456Char(0x00,key,6);
              osd.printMax7456Char(0xD9,3,10);
              myFile = SD.open(loc,FILE_WRITE);
              myFile.seek(26);
              myFile.print(modelName);
              myFile.close();
              pos=1;
              onClick=0;
            }
          }
          break;
          case 2:{
            if (!onClick){
              onClick=1;
              osd.printMax7456Char(char(16+model),14,11,true);
            }
            else{
              osd.printMax7456Char(char(16+model),14,11);
              myFile = SD.open("cal.txt",FILE_WRITE);
              SDWriteInt(model,0);
              myFile.close();
              
              sprintf(loc,"models/model%d.txt",model);
              myFile = SD.open(loc,FILE_READ);
              numReadings = SDReadInt(0);
              for (byte i=1; i<=4 ; i++){
                defTrim[i-1]=SDReadInt(i);
                Trim[i-1]=defTrim[i-1];
                endPoints[i-1][0]=SDReadInt(i+4)-1000;
                endPoints[i-1][1]=SDReadInt(i+8)-1000;
              }
              myFile.seek(26);
              for (byte i=0 ; i<8 ; i++){
                modelName[i]=myFile.read();
              }
              myFile.close();
              osd.print(modelName,16,10);
              onClick=0;
              pos =2;
            }
          }
          break;
          case 3:{
            pos = 1;
            page = 1;
            homescreen();
          }
          break;
        }
      }
      break;
      case 3:{
        switch (pos){
          case 1:{
            osd.print("Saved!",15,8);
            myFile = SD.open(loc,FILE_READ);
            for (byte i =0;i<=4 ;i++){
              defTrim[i]=Trim[i];
              SDWriteInt(Trim[i],i+1);              
            }
            myFile.close();
          }
          break;
          case 2:{
            if (!onClick){
              onClick = 1;
              key = 1;
              osd.printMax7456Char(0x00,3,9);
              osd.printMax7456Char(0xD9,13,4);
              onClick2= 0;
            }
            else if (onClick){
              if (!onClick2){
                onClick2=1;
                sprintf(buf,"%d",endPoints[key-1-(key>=5)*4][key >=5]);
                osd.print(buf,17+(key>=5)*7,3+key-(key>=5)*4,true);
              }
              else{
                sprintf(buf,"%d",endPoints[key-1-(key>=5)*4][key >=5]);
                osd.print(buf,17+(key>=5)*7,3+key-(key>=5)*4);
                myFile = SD.open(loc,FILE_WRITE);
                SDWriteInt(endPoints[key-1-(key>=5)*4][key >=5]+1000,key+4);
                myFile.close();
                onClick2=0;
              }
            }
          }
          break;
          case 3:{
            if(!onClick){
              onClick = 1;
              osd.print("Move Sticks!", 14,10,true);
              for (byte i = 0;i<4;i++){
                calibration[i][0]=500;
                calibration[i][1]=500;
              }
            }
            else{
              onClick = 0;
              osd.print("Calibrated! ",14,10);
              myFile=SD.open("cal.txt",FILE_WRITE);
              for (byte i = 1; i<=4 ; i++){
                SDWriteInt(calibration[i-1][0],i);
                SDWriteInt(calibration[i-1][1],i+4);
              }
              myFile.close();
              
            }
          }
          break;
          case 4:{
            if (!onClick){
              onClick = 1;
              sprintf(buf,"%d",numReadings);
              osd.print(buf,15,11,true);
            }
            else{
              onClick = 0;
              pos = 4;
              myFile= SD.open(loc,FILE_WRITE);
              SDWriteInt(numReadings,0);
              myFile.close();
              sprintf(buf,"%d",numReadings);
              osd.print(buf,15,11);
            }
          }
          break;
          case 5:{
            pos = 1;
            page = 1;
            homescreen();
          }
          break;
        }
      }
      break;
      case 4:{
        page=1;
        pos=1;
        homescreen();
        digitalWrite(47,0);
      }
      break;
      case 6:{
        page=1;
        pos=1;
        homescreen();
        digitalWrite(48,0);
      }
      break;
    }
    bounce1= millis();
  }

  //Page 3 Calibration Animation
  if (page == 3 && millis()-delay1 >= 200){
    for (int i = 0; i<4;i++){
      if(i==0||i==3){
        joyAnim[i]=40-map(analogRead(joystick[i])+Trim[i],calibration[i][0]+endPoints[i][0],calibration[i][1]+endPoints[i][1],1,40);
      }
      else{
        joyAnim[i]=map(analogRead(joystick[i])+Trim[i],calibration[i][0]+endPoints[i][0],calibration[i][1]+endPoints[i][1],1,40);
      }
      if(joyAnim[i] >=1 && joyAnim[i] <=40 && limit[i] != joyAnim[i]/8){
        osd.printMax7456Char(0x00,i*3+2,6-limit[i]);
        limit[i] = joyAnim[i]/8;         
      }
      osd.printMax7456Char(char(103-joyAnim[i]%8),i*3+2,6-limit[i]);
      if (lastTrim[i] != Trim[i]){
        sprintf(buf,"%d",Trim[i]);
        osd.print("    ",23,i);
        osd.print(buf,23,i);
        lastTrim[i]=Trim[i];
      }
    }
    if (pos == 3 && onClick){
      for (byte i = 0 ;i<4;i++){
        int aa = analogRead(joystick[i]);
        if (calibration[i][0]>aa){
          calibration[i][0]=aa;        
        }
        else if (calibration[i][1]<aa){
          calibration[i][1]=aa;
        }
      }
    }
    delay1=millis();  
  }
  
}

void homescreen(){
  clearS();
  osd.print(modelName,13,2);
  osd.print("Joystick",5,3);
  osd.print("VRX Settings",5,4);
  osd.print("Screen Settings",5,5);
  osd.print("DVR Settings",5,6);
  osd.print("OSD Settings",5,7);
  osd.print("Video On/Off",5,8);
  osd.print("Exit Menu",5,9);
  osd.printMax7456Char(0xD9,4,2);
  osd.print("Model: ",5,2);
}

void readSwitch(){
  if(digitalRead(31)){
    PPM.write(5,1000);
  }
  else{
    PPM.write(5,2000);
  }
  
  if(digitalRead(32)){
    PPM.write(6,1000);
  }
  else{
    PPM.write(6,2000);
  }

  int aa=digitalRead(28);
  int bb=digitalRead(29);
  if(aa*bb){
    PPM.write(7,1500);
  }
  else if(aa){
    PPM.write(7,1000);
  }
  else if(bb){
    PPM.write(7,2000);
  }
  
  if(digitalRead(30)){
    PPM.write(8,1000);
  }
  else{
    PPM.write(8,2000);
  }
}

void readTrim(){
  for (byte i = 0 ; i <4 ; i++){
    if (digitalRead(rotaryTrim[i])==0 && millis()-bounce2 >= 250){
      if (digitalRead(rotaryTrim[4])== 0){
        Trim[i]++;
      }
      else{
        Trim[i]--;
      }
    }
  }
  bounce2=millis();
}

void readRot(){
  if (digitalRead(rotaryBut[0])==0 && millis()-bounce >=200){
    if (digitalRead(rotaryBut[1]) == 0){
      rotUp =1;
    }
    else{
      rotDown = 1;
    }
    bounce = millis();
  }
}

void readPots(){
  for(byte i=0;i < analogJoy;i++)
  {
    total[i] = total[i] - readings[readIndex][i];
    if (i==0||i==3){
      readings[readIndex][i] = 3000-map(analogRead(joystick[i])+Trim[i],calibration[i][0]+endPoints[i][0]*3,calibration[i][1]+endPoints[i][1]*3,potMin,potMax);    
    }
    else{
      readings[readIndex][i] = map(analogRead(joystick[i])+Trim[i],calibration[i][0]+endPoints[i][0]*3,calibration[i][1]+endPoints[i][1]*3,potMin,potMax);
    }
    total[i] = total[i] + readings[readIndex][i];
  }
  readIndex = readIndex + 1;
  if(readIndex >= numReadings) {
    readIndex=0;
  }
  for(byte i=1;i <= analogJoy;i++)
  {
    if (i==2){
      PPM.write(4,total[i-1]/numReadings);
    }
    else if(i==4){
      PPM.write(2,total[i-1]/numReadings);
    }
    else{
      PPM.write(i,total[i-1]/numReadings);
    }
  }
}

void SDWriteInt(int p_value, int p_adress) {
  // write a 16bit value in file
  myFile.seek(p_adress*2);
  byte Byte1 = ((p_value >> 0) & 0xFF);
  byte Byte2 = ((p_value >> 8) & 0xFF);
  myFile.write(Byte1);
  myFile.write(Byte2);
}

int SDReadInt(int p_adress) {
  // read a 16 bit value in file
  myFile.seek(p_adress*2);
  byte Byte1 = myFile.read();
  byte Byte2 = myFile.read();
  long firstTwoBytes = ((Byte1 << 0) & 0xFF) + ((Byte2 << 8) & 0xFF00);
  return (firstTwoBytes);
}

void clearS(){
  for (byte ii = 0 ; ii<15 ; ii++){
    osd.print("                            ",0,ii);
  }
}

