#include <Arduino.h>

const uint8_t BTN_PIN=4, LED_OUT_PIN=7;
const uint8_t REC_PIN=10, PLAY_PIN=11, CLR_PIN=12, STOP_PIN=3, DUMP_PIN=29;
const uint8_t TGL_PINS[5]={13,14,15,26,27};
const uint8_t STATUS_LED=5;

const uint8_t NUM_SLOTS=32;
const uint16_t MAX_PARTS=1300;
const uint32_t FILE_MAX_MS=90000;
const uint32_t AUTO_STOP_MS=30000;
const uint16_t DEBOUNCE_MS=15;
const uint32_t BTN_GLITCH_US=300;

const char* CSV_PREFIX="@RZ1CSV:";

uint16_t parts[NUM_SLOTS][MAX_PARTS];
uint16_t partCount[NUM_SLOTS]={0};
uint32_t totalMs[NUM_SLOTS]={0};

bool recording=false, playing=false;
uint8_t activeSlot=0;

bool started=false, curState=false;
uint32_t lastChangeUs=0, lastActivityMs=0;

uint16_t playIndex=0;
uint8_t playPhase=0;
uint32_t phaseStartMs=0;

uint32_t statLastMs=0;
bool statLevel=false;

struct Deb{bool stable; bool prev; uint32_t t;};
Deb dbRec,dbPlay,dbClr,dbStop,dbDump;

bool btn_stable=HIGH, btn_prev=HIGH, btn_last_raw=HIGH;
uint32_t btn_last_raw_change_us=0;

void setStatusIdle(){ if(!recording && !playing){ digitalWrite(STATUS_LED,LOW); statLevel=false; } }
inline uint16_t round5(uint32_t ms){ return (uint16_t)(((ms+2)/5)*5); }

bool upd(Deb& d,bool raw,uint32_t now){
  if(raw!=d.stable){ if(now-d.t>=DEBOUNCE_MS){ d.prev=d.stable; d.stable=raw; d.t=now; return true; } }
  else d.t=now;
  return false;
}

uint8_t readSlot(){ uint8_t v=0; for(uint8_t i=0;i<5;++i) if(digitalRead(TGL_PINS[i])==LOW) v|=(1<<i); return v; }

void resetPlay(){ playing=false; digitalWrite(LED_OUT_PIN,LOW); setStatusIdle(); }

void endPart(uint32_t now){
  if(!started) return;
  uint32_t durMs = (micros()-lastChangeUs + 500)/1000;
  uint16_t r = round5(durMs);
  if(r==0){ lastActivityMs=now; lastChangeUs=micros(); return; }
  uint32_t rem = (FILE_MAX_MS>totalMs[activeSlot])?(FILE_MAX_MS-totalMs[activeSlot]):0;
  if(rem==0){ recording=false; digitalWrite(LED_OUT_PIN,LOW); started=false; setStatusIdle(); return; }
  if(r>rem) r=(uint16_t)rem;
  if(partCount[activeSlot]<MAX_PARTS && r>0){
    parts[activeSlot][partCount[activeSlot]++]=r;
    totalMs[activeSlot]+=r;
  }
  lastActivityMs=now; lastChangeUs=micros();
  if(totalMs[activeSlot]>=FILE_MAX_MS || partCount[activeSlot]>=MAX_PARTS){
    recording=false; digitalWrite(LED_OUT_PIN,LOW); started=false; setStatusIdle();
  }
}

void stopRec(uint32_t now){
  if(recording){ endPart(now); recording=false; digitalWrite(LED_OUT_PIN,LOW); started=false; setStatusIdle(); }
}

void clearSlot(uint8_t s){
  partCount[s]=0; totalMs[s]=0;
  if(recording && activeSlot==s) stopRec(millis());
  if(playing && activeSlot==s) resetPlay();
}

void dumpSlot(uint8_t s){
  stopRec(millis()); resetPlay();
  uint16_t count=partCount[s];
  uint32_t tot=totalMs[s];
  Serial.print(CSV_PREFIX);
  Serial.print("1,"); Serial.print(s); Serial.print(","); Serial.print(count); Serial.print(","); Serial.print(tot);
  for(uint16_t i=0;i<count;++i){ Serial.print(","); Serial.print(parts[s][i]); }
  Serial.println();
}

void setup(){
  pinMode(BTN_PIN,INPUT_PULLUP);
  pinMode(LED_OUT_PIN,OUTPUT); digitalWrite(LED_OUT_PIN,LOW);
  pinMode(REC_PIN,INPUT_PULLUP); pinMode(PLAY_PIN,INPUT_PULLUP);
  pinMode(CLR_PIN,INPUT_PULLUP); pinMode(STOP_PIN,INPUT_PULLUP);
  pinMode(DUMP_PIN,INPUT_PULLUP);
  for(uint8_t i=0;i<5;++i) pinMode(TGL_PINS[i],INPUT_PULLUP);
  pinMode(STATUS_LED,OUTPUT); digitalWrite(STATUS_LED,LOW);
  uint32_t now=millis();
  dbRec.stable=dbRec.prev=HIGH; dbRec.t=now;
  dbPlay.stable=dbPlay.prev=HIGH; dbPlay.t=now;
  dbClr.stable=dbClr.prev=HIGH; dbClr.t=now;
  dbStop.stable=dbStop.prev=HIGH; dbStop.t=now;
  dbDump.stable=dbDump.prev=HIGH; dbDump.t=now;
  btn_stable=btn_prev=btn_last_raw=HIGH; btn_last_raw_change_us=micros();
  Serial.begin(115200);
}

void loop(){
  uint32_t now=millis();
  uint32_t nowUs=micros();

  bool rawBTN=digitalRead(BTN_PIN);
  if(rawBTN!=btn_last_raw){ btn_last_raw=rawBTN; btn_last_raw_change_us=nowUs; }
  bool eBTN=false;
  if((rawBTN!=btn_stable) && (nowUs - btn_last_raw_change_us >= BTN_GLITCH_US)){ btn_prev=btn_stable; btn_stable=rawBTN; eBTN=true; }

  bool rREC=digitalRead(REC_PIN), rPLAY=digitalRead(PLAY_PIN), rCLR=digitalRead(CLR_PIN), rSTOP=digitalRead(STOP_PIN), rDUMP=digitalRead(DUMP_PIN);
  bool eREC=upd(dbRec,rREC,now), ePLAY=upd(dbPlay,rPLAY,now), eCLR=upd(dbClr,rCLR,now), eSTOP=upd(dbStop,rSTOP,now), eDUMP=upd(dbDump,rDUMP,now);
  uint8_t sel=readSlot();
  bool recP=(eREC && dbRec.prev==HIGH && dbRec.stable==LOW);
  bool playP=(ePLAY&& dbPlay.prev==HIGH&& dbPlay.stable==LOW);
  bool clrP=(eCLR && dbClr.prev==HIGH && dbClr.stable==LOW);
  bool stopP=(eSTOP&& dbStop.prev==HIGH&& dbStop.stable==LOW);
  bool dumpP=(eDUMP&& dbDump.prev==HIGH&& dbDump.stable==LOW);

  if(recP){
    resetPlay();
    if(!recording){ activeSlot=sel; partCount[activeSlot]=0; totalMs[activeSlot]=0; recording=true; started=false; curState=false; lastChangeUs=nowUs; lastActivityMs=now; }
    else stopRec(now);
  }

  if(stopP){ if(recording) stopRec(now); else if(playing) resetPlay(); }

  if(playP && partCount[sel]>0){
    stopRec(now); activeSlot=sel; playing=true; playIndex=0; playPhase=0; phaseStartMs=now;
  }

  if(clrP) clearSlot(sel);
  if(dumpP) dumpSlot(sel);

  if(recording){
    if(!started){
      if(eBTN && btn_prev==HIGH && btn_stable==LOW){
        started=true; curState=true; lastChangeUs=nowUs; lastActivityMs=now; digitalWrite(LED_OUT_PIN,HIGH);
      }else{
        if(now-lastActivityMs>=AUTO_STOP_MS) stopRec(now);
      }
    }else{
      if(eBTN){
        if(curState && btn_prev==LOW && btn_stable==HIGH){ endPart(now); curState=false; digitalWrite(LED_OUT_PIN,LOW); }
        else if(!curState && btn_prev==HIGH && btn_stable==LOW){ endPart(now); curState=true; digitalWrite(LED_OUT_PIN,HIGH); }
      }
      if(now-lastActivityMs>=AUTO_STOP_MS) stopRec(now);
      if(totalMs[activeSlot]>=FILE_MAX_MS || partCount[activeSlot]>=MAX_PARTS) stopRec(now);
    }
  }

  if(playing && partCount[activeSlot]>0){
    if(playIndex>=partCount[activeSlot]){ resetPlay(); }
    else{
      bool on=(playIndex%2==0);
      uint32_t dur=parts[activeSlot][playIndex];
      if(playPhase==0){
        digitalWrite(LED_OUT_PIN,on?HIGH:LOW);
        if(now-phaseStartMs>=dur){ playPhase=1; phaseStartMs=now; }
      }else{
        playIndex++; playPhase=0; phaseStartMs=now;
      }
    }
  }

  if(recording){
    if(now - statLastMs >= 120){ statLastMs=now; statLevel=!statLevel; digitalWrite(STATUS_LED, statLevel?HIGH:LOW); }
  }else if(playing){
    if(!statLevel){ statLevel=true; digitalWrite(STATUS_LED,HIGH); }
  }else{
    if(statLevel){ statLevel=false; digitalWrite(STATUS_LED,LOW); }
  }
}
