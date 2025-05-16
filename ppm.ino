#include <SPI.h>
#include <RF24.h>

//////////////////////CONFIGURATION///////////////////////////////
#define CHANNEL_NUMBER 8                 //set the number of chanels
#define CHANNEL_DEFAULT_VALUE 1000       //set the default servo value
#define FRAME_LENGTH 22500               //set the PPM frame length in microseconds (1ms = 1000µs)
#define PULSE_LENGTH 300                 //set the pulse length
#define onState 1                        //set polarity of the pulses: 1 is positive, 0 is negative
#define sigPin 8                         //set PPM signal output pin on the arduino

#define SIGNAL_TIMEOUT 100  // This is signal timeout in milli seconds.
#define LEDPin 2  // This is signal timeout in milli seconds.

/*this array holds the servo values for the ppm signal
 change theese values in your code (usually servo values move between 1000 and 2000)*/

int ppm[CHANNEL_NUMBER];
unsigned long lastRecvTime = 0;

RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00002";

struct PacketData
{
  byte ch1_0;  byte ch1_1;
  byte ch2_0;  byte ch2_1;
  byte ch3_0;  byte ch3_1;
  byte ch4_0;  byte ch4_1;
  byte ch5_0;  byte ch5_1;
  byte ch6_0;  byte ch6_1;
  byte ch7_0;  byte ch7_1;
  byte ch8_0;  byte ch8_1;
};
PacketData receiverData;


void setup(){  
  Serial.begin(115200);

  //initiallize default ppm values
  for(int i=0; i<CHANNEL_NUMBER; i++){
      ppm[i]= CHANNEL_DEFAULT_VALUE;
  }

  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MAX); 
  radio.startListening();

  pinMode(LEDPin, OUTPUT);  
  pinMode(sigPin, OUTPUT);
  digitalWrite(sigPin, !onState);  //set the PPM signal pin to the default state (off)
  
  cli();
  TCCR1A = 0; // set entire TCCR1 register to 0
  TCCR1B = 0;
  
  OCR1A = 100;  // compare match register, change this
  TCCR1B |= (1 << WGM12);  // turn on CTC mode
  TCCR1B |= (1 << CS11);  // 8 prescaler: 0,5 microseconds at 16mhz
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  sei();
   
}

void loop(){
 
   if(radio.available()){
      radio.read(&receiverData, sizeof(PacketData)); 
      ppm[0] = receiverData.ch1_0*256 + receiverData.ch1_1;
      ppm[1] = receiverData.ch2_0*256 + receiverData.ch2_1;
      ppm[2] = receiverData.ch3_0*256 + receiverData.ch3_1;
      ppm[3] = receiverData.ch4_0*256 + receiverData.ch4_1;
      ppm[4] = receiverData.ch5_0*256 + receiverData.ch5_1;
      ppm[5] = receiverData.ch6_0*256 + receiverData.ch6_1;
      ppm[6] = receiverData.ch7_0*256 + receiverData.ch7_1;
      ppm[7] = receiverData.ch8_0*256 + receiverData.ch8_1;

      char inputValuesString[200];
      sprintf(inputValuesString, "%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d", ppm[0],ppm[1],ppm[2],ppm[3],ppm[4],ppm[5],ppm[6],ppm[7]);
      Serial.println(inputValuesString); 

      digitalWrite(LEDPin, HIGH);
      lastRecvTime = millis(); 
  }

  unsigned long now = millis();
  if ( now - lastRecvTime > SIGNAL_TIMEOUT ) 
  {
      digitalWrite(LEDPin, LOW);
      Serial.println("Disconnected"); 
      
      for(int i=0; i<CHANNEL_NUMBER; i++){
        ppm[i]= 900;
      }
  }

}

ISR(TIMER1_COMPA_vect){  //leave this alone
  static boolean state = true;
  
  TCNT1 = 0;
  
  if (state) {  //start pulse
    digitalWrite(sigPin, onState);
    OCR1A = PULSE_LENGTH * 2;
    state = false;
  } else {  //end pulse and calculate when to start the next pulse
    static byte cur_chan_numb;
    static unsigned int calc_rest;
  
    digitalWrite(sigPin, !onState);
    state = true;

    if(cur_chan_numb >= CHANNEL_NUMBER){
      cur_chan_numb = 0;
      calc_rest = calc_rest + PULSE_LENGTH;// 
      OCR1A = (FRAME_LENGTH - calc_rest) * 2;
      calc_rest = 0;
    }
    else{
      OCR1A = (ppm[cur_chan_numb] - PULSE_LENGTH) * 2;
      calc_rest = calc_rest + ppm[cur_chan_numb];
      cur_chan_numb++;
    }     
  }
}
