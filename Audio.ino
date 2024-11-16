
#include "pitch.h"

// the analog reading from the sensor divider
double photocellReading;   
// connect Red LED to pin 11 (PWM pin)
int LEDpin = 11;  
//Used to minimize and maximize how how or low the brightness of the light can go. To know more, you can read the github page: 
    
int maxVal = 0;         
int minVal = 0;                              
int lastVal =0;
int currentVal=0;
//Temp storing of the photocell data
int photocellPin = 0;
//Value read from the photocell when sync is done  
int firstReading = 0;

//Data packet information

//|----------------|----------------|-------------------|----------------------|---------------|---------------|
//| Start 4 bits   | Header 4 bits  |  Duration 4 bits  |     Data 32 bits     |  Next 4 bits  |  End 4 bits   |
//|----------------|----------------|-------------------|----------------------|---------------|---------------|

//1. When 1001 is received, we know we need start the transfer
//2. Header has information about the packet (not really used in this transfer)
//3. Duration inidcates if it's a 4=quarter note / 8=eighth note
//4. Data indicates the note to be played (based on pitch.h)
//5. Next bit inidcates wheather there is another packet to be transferred (0001 yes / 0000 no)
//5. End indicates that the packet transfer is done


//Keep track of the last 4 bits to see if it is a start signal
char ctrlBits[4] = {'0','0','0','0'};
int ctrlBitCounter = 3;

//The next packet to be transfered is indicated using these ctrl bits
char ctrlBitsNext[4] = {'0','0','0','0'};
int ctrlBitNext = 3;

//Header information
char ctrlBitsHeader[4] = {'0','0','0','0'};
int ctrlBitHeader = 3;

//End of transfer signal
char ctrlEndTransfer[4] = {'0','0','0','0'};
int ctrlEndCounter= 3;

//32 bit data
char dataBits[32] = {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};
int dataCounter = 31;

//Duration of the note to be played is stored temporary
char durationBits[4] = {'0','0','0','0'};
int durationCounter = 3;

//notes in the melody
int melody[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
//count of the notes
int melodyCount = 0;

//note durations. 4=quarter note / 8=eighth note
int noteDurations[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
int noteDurationsCount = 0;

//flag to indicate that the transfer of the melody is about to end
int endMelody = 0;
//start flag
int startTransfer = 0;
//next flag indicates that the information about the next packet is being transfered. 
int startNext = 0;
//start header flog indicates that the information about the header is being transfered. 
int startHeader = 0;
//indicates the transfer of the end of packet bits
int endTransfer = 0;

//Duration bits are transferred when this value is 1
int startDuration = 0;

//Data is being transferred
int startData = 0;

//notes in the melody to send mapped to the pitch file
int melodyToSend[]={NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_C6, NOTE_DS8, NOTE_CS8, NOTE_CS8};

//note durations to send. 4=quarter note / 8=eighth note
int noteDurationsToSend[]={4, 8, 8, 4, 4, 4, 4, 4};


void setup() {
  // serial 9600 baud
   Serial.begin(9600);
}

//initial recording of the led brightness. Used for synchronization
void setUpTransferRates(void){
  //filter out the light in the room
  photocellReading = analogRead(photocellPin)- firstReading;
  //the led can't be brighter than this
  maxVal = photocellReading+(photocellReading/4);
  //the led can't be dimmer than this
  minVal = photocellReading-((photocellReading/4));
  //current value of the led
  currentVal = photocellReading;
}

//transmit a bit
void transmit(int val){    
  if (val == 0){
    //write a 0 to the led
    analogWrite(LEDpin,0);
  }else if (val == 1){
    //write a 255 to the led
    analogWrite(LEDpin, 255);
  }
}

//pow function int
int powint(int x, int y)
{
 int val=x;
 for(int z=0;z<=y;z++)
 {
   if(z==0)
     val=1;
   else
     val=val*x;
 }
 return val;
}

//convert from binary to Int (4 bits)
int binaryToInt(char s[]){
  int value = 0;
  for (int i=3; i>=0; i--) //go through each bit at add the value of the bit to the total
  {
    if (s[i] == '1'){
      value = value + powint(2,((3)-i));
    }
    
  }
  return value;
}

//convert from binary to Int (32 bits)
int binaryToInt32(char s[]){
  int value = 0;
  for (int i=31; i>=0; i--) //go through each bit at add the value of the bit to the total
  {
    if (s[i] == '1'){
      value = value +powint(2,((31)-i));
    }
    
  }
  return value;
}

//reset array to 0's (4 bits)
void resetArray(char s[]){
  for (int i=0; i< 4; i++)  // for every character in the string  strlen(s) returns the length of a char array
  {
    s[i] = 0;
  }
}

//reset array to 0's (32 bits)
void resetArrayInt32(int s[]){
  for (int i=0; i<= 31; i++)  // for every character in the string  strlen(s) returns the length of a char array
  {
    s[i] = 0;
  }
}

//reset array to 1's (32 bits)
void resetArrayNegativeInt32(int s[]){
  for (int i=0; i<= 31; i++)  // for every character in the string  strlen(s) returns the length of a char array
  {
    s[i] = -1;
  }
}

//receiver 
void recieve(){
  //record the current value
  currentVal = analogRead(photocellPin)- firstReading;
  //loop until a value is found
  while(1){
        //if the value of the led is greater or equal to the max value reset it to a value of 150
        //if the value of the led is less than or equal to the min value reset it to a value of 150  
        //if the value of the led is less than the current val by at least 2, a 0 was transferred
        //if the value of the led is more than the current val by at least 2, a 1 was transferred
        photocellReading = analogRead(photocellPin)- firstReading;
        if(photocellReading>=maxVal){
          reset();
        }else if(photocellReading<=minVal){
          reset();
        }else{
          if ((currentVal-photocellReading)>=2){
            //0 transferred
            analogWrite(LEDpin, 150); 
            handleData('0'); 
            break;
          }else if((currentVal-photocellReading)<=-2){
            //1 transferred
            analogWrite(LEDpin,150);
            handleData('1');
            break;
          }
        }
      }
}

//transfer identification logic
void handleData(char data){
    //if the start flag and end flag is 0, look for start ctrl signal
    if (startTransfer==0 && endTransfer==0){
      //store the latest 4 bits and see if it matches with the start bit
      ctrlBits[ctrlBitCounter] = data; 
      if(ctrlBitCounter !=0){
        ctrlBitCounter--;
      }
      //start sequence is 1001 (9)
      if(binaryToInt(ctrlBits)==9){
       Serial.println("START RECIEVED");
        startTransfer = 1;
        startHeader =1;
        resetArray(ctrlBits);
        ctrlBitCounter =3;
      }
      
    //header transfer  
    }else if (startTransfer==1 && endTransfer==0 && startHeader==1){
      //this area can be programmed. For right now, I just transfer a random 4 values
      ctrlBitsHeader[ctrlBitHeader] = data; 
      ctrlBitHeader--;
      
      if (ctrlBitHeader==-1){
        Serial.println("START DURATION");
        startHeader = 0;
        startDuration=1;
        ctrlBitHeader = 3;
      }
      
    //transfer the duration  
    }else if (startTransfer==1 && endTransfer==0 && startDuration==1){
      durationBits[durationCounter] = data; 
      durationCounter--;
      if (durationCounter==-1){
        Serial.println("START DATA");
        startDuration = 0;
        startData=1;
        durationCounter=3;
        noteDurations[noteDurationsCount] = binaryToInt(durationBits);
        noteDurationsCount++;
      }

    //data transfer  
    }else if (startTransfer==1 && endTransfer==0 && startData==1){
      dataBits[dataCounter] = data; 
      dataCounter--;

      if (dataCounter==-1){
        Serial.println("START NEXT");
        startData = 0;
        startNext = 1;
        dataCounter = 31; 
        melody[melodyCount] = binaryToInt32(dataBits);
        melodyCount++;
      }

    //next data
    }else if (startTransfer==1 && endTransfer==0 && startNext==1){
      ctrlBitsNext[ctrlBitNext] = data; 
      ctrlBitNext--;

      if (ctrlBitNext==-1){
        Serial.println("START END");
        if (binaryToInt(ctrlBitsNext)==0){
          endMelody =1;
        }
        ctrlBitNext=3;
        startNext = 0;
        endTransfer = 1;
      }
    //end transfer 
    }else if (startTransfer==1 && endTransfer==1){
      ctrlEndTransfer[ctrlEndCounter] = data; 
      ctrlEndCounter--;

      if (ctrlEndCounter==-1){
        
        if (binaryToInt(ctrlEndTransfer)==6){
          Serial.println("SUCCESSFUL TRANSFER :)");
          if (endMelody==1){
            Serial.println("PLAYING MELODY");
            playMelody();
            
          }
        }else{
          Serial.println("FAILED");
        }
        startTransfer = 0;
        endTransfer = 0;
        endMelody=0;
        ctrlEndCounter=3;
      }
    }
}

void playMelody(){
  int note = 0;

  while(melody[note] != -1){
    //calculate the real duration
    int noteDuration = 1000/noteDurations[note];
    tone(8,melody[note],noteDuration);
    //pause before playing the next note
    int pause = noteDuration*1.3;
    delay(pause);
    noTone(8);
    note++;
  }
   //reset array
   resetArrayNegativeInt32(melody);
   resetArrayNegativeInt32(noteDurations);
   melodyCount = 0;
   noteDurationsCount = 0;
   
   delay(1000);
}

//reset the led and re-transfer the data 
//use the lastVal to resend the data
void reset(void){
   analogWrite(LEDpin, 150);
   delay(30);
   transmit(lastVal);
   currentVal = analogRead(photocellPin)- firstReading;
}

//sending control signals
void sendStart(){
       
   lastVal=1;
   transmit(1);
   recieve();
   
   lastVal=0;
   transmit(0);
   recieve();
   
   lastVal=0;
   transmit(0);
   recieve();
   
   lastVal=1;
   transmit(1);
   recieve();
     
}

void sendEnd(){
   lastVal=0;
   transmit(0);
   recieve();
   
   lastVal=1;
   transmit(1);
   recieve();
   
   lastVal=1;
   transmit(1);
   recieve();
   

   lastVal=0;
   transmit(0);
   recieve();
}

void sendHeader(){
   lastVal=1;
   transmit(1);
   recieve();

   lastVal=1;
   transmit(1);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();
   
}

void sendNext(int data){

  if (data ==0){
    
   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();
   
  }else if(data==1){

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=0;
   transmit(0);
   recieve();

   lastVal=1;
   transmit(1);
   recieve();
  }
}

//send the duration data bit at a time
void sendDurationData(int data){
  int a = 0;
  for(int i =0; i<4;i++){
    a = (data>>i) & 1;
    lastVal = a;
    transmit(a);
    recieve();
  }
}

//send melody data a bit at a time
void sendMelodyData(int data){
  int a = 0;
  for(int i =0; i<32;i++){
    a = (data>>i) & 1;
    lastVal = a;
    transmit(a);
    recieve();
  }
}

void loop() {
 
  //initially set the led
  
   analogWrite(LEDpin, 150);
   delay(200);
   //set up the transfer
   setUpTransferRates();

   //send the data
   for (int note=0;note<8;note++){
     sendStart();
     sendHeader();
     sendDurationData(noteDurationsToSend[note]);
     sendMelodyData(melodyToSend[note]);
     if (note==7){
      sendNext(0);
     }else{
      sendNext(1);
     }
     sendEnd();
     
   }

   //reset light
   analogWrite(LEDpin, 0);
   delay(1000);
}
