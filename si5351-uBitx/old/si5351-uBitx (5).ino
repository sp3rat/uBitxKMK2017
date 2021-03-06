/*
Revision 1.0 - 	Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - 	November 6th, 2013...  ever so slight revision by  VK8BN for AD9851 chip Feb 24 2014
Revision 3.0 - 	April, 2016 - AD9851 + ARDUINO PRO NANO + integrate cw decoder (by LZ1DPN) (uncontinued version)
Revision 4.0 - 	May 31, 2016  - deintegrate cw decoder and add button for band change (by LZ1DPN)
Revision 5.0 - 	July 20, 2016  - change LCD with OLED display + IF --> ready to control transceiver RFT SEG-100 (by LZ1DPN)
Revision 6.0 - 	August 16, 2016  - serial control buttons from computer with USB serial (by LZ1DPN) (1 up freq, 2 down freq, 3 step increment change, 4 print state)
									for no_display work with DDS generator
Revision 7.0 - 	November 30, 2016  - added some things from Ashhar Farhan's Minima TRX sketch to control transceiver, keyer, relays and other ...									
Revision 7.5 - 	December 12, 2016  - for Minima and Bingo Transceivers (LZ1DPN mod).
Revision 8.0 - 	February 15, 2017  - for Minima, BitX and Bingo Transceivers (LZ1DPN mod).
Revision 9.0 - 	March 06, 2017  - Si5351 + Arduino + OLED - for Minima and Bingo Transceivers (LZ1DPN mod).
				Parts of this program is taken from Jason Mildrum, NT7S and Przemek Sadowski, SQ9NJE.
				http://nt7s.com/
				http://sq9nje.pl/
				http://ak2b.blogspot.com/
				+ example from:
				si5351_vcxo.ino - Example for using the Si5351B VCXO functions
				with Si5351Arduino library Copyright (C) 2016 Jason Milldrum <milldrum@gmail.com>
Revision 10.0 - March 08, 2017  - Si5351 + Arduino + OLED - for LZ1DPN CW Transceiver (LZ1DPN mod).
Revision 11.0 - May 18, 2017  - add +/- RIT
Revision 12.0 - August 17, 2017  - CM2KMK 40m SSB trx
Revision 13.0 - August 28, 2017  - LZ1DPN 20m SSB trx
Revision 14.0 - September 07, 2017  - uBitx SSB trx
Revision 14.0 - Octomber 07, 2017  - uBitx SSB trx

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Include the library code
#include <Wire.h>
#include <SPI.h>
#include <si5351.h>
Si5351 si5351;
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <rotary.h>
#define OLED_RESET 5
Adafruit_SSD1306 display(OLED_RESET);

#define SECOND_OSC (57000000l)  // 57000000l
//<<<<<<< HEAD
int_fast32_t INIT_USB_FREQ = 11997700l;   //6500, 11996500l, 11997700l
int_fast32_t INIT_LSB_FREQ = 11998500l;   //8500, 11998500l, 11999700l
//=======
//int_fast32_t INIT_USB_FREQ = 11996300l;   //6500, 11996500l, 11997700l, 11996300l;
//int_fast32_t INIT_LSB_FREQ = 11998300l;   //8500, 11998500l, 11999700l, 11998300l;
//>>>>>>> origin/master

#define CW_TIMEOUT (600l) // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs
unsigned long cwTimeout = 0;     //keyer var - dead operator control
#define TX_RX (12)   // (2 sided 2 possition relay) - for Farhan minima +5V to Receive 0V to Transmit !!! (see Your schema and change if need)
#define CW_KEY (4)   // KEY output pin - in Q7 transistor colector (+5V when keyer down for RF signal modulation) (in Minima to enable sidetone generator on)
#define PTT_ON (6)   // PTT button 
#define ANALOG_KEYER (A1)  // KEYER input - for analog straight key
char inTx = 0;       // trx in transmit mode temp var
char isUSB = 0;       // 
char keyDown = 0;    // keyer down temp var
int var_i = 0;
int byteRead = 0;    // for serial comunication
#define BTNDEC (A2)  // BAND CHANGE BUTTON from 1,8 to 29 MHz - 11 bands
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
Rotary r = Rotary(2,3); // sets the pins for rotary encoder uses.  Must be interrupt pins.
  
int_fast32_t rx=7000000; // Starting frequency of VFO freq
int_fast32_t rx2=1;  // temp variable to hold the updated frequency
//<<<<<<< HEAD
int_fast32_t rxofset=0; 
//=======
//int_fast32_t rxofset=-1700;   //-300 155200
//>>>>>>> origin/master
int_fast32_t rxbfo=INIT_LSB_FREQ;  //BFO osc 11999904  11998800
int_fast32_t rxRIT=0;

int RITon=0;
int_fast32_t rx600hz=700;  // cw offset 700
long cal=1;
int_fast32_t increment = 500; // starting VFO update increment in HZ. tuning step
int buttonstate = 0;   // temp var
String hertz = "500 Hz";
int  hertzPosition = 0;
int_fast32_t timepassed = millis(); // int to hold the arduino miilis since startup

// buttons temp var
int BTNdecodeON = 0;   
int BTNlaststate = 0;
int BTNcheck = 0;
int BTNcheck2 = 0;
int BTNinc = 3; // set number of default band ---> (for 7MHz = 3)

/**************************************/
/* PTT Check                          */
/**************************************/
void checkTX(){
//we don't check for ptt when transmitting cw  
//will enable in future when/if add cw functions
//  if (cwTimeout > 0)      
//    return;

  if (digitalRead(PTT_ON) == 1 && inTx == 0){
    // put the transceiver to transmit
    inTx = 1;
    delay(150);    // need tests for time
  }

  if (digitalRead(PTT_ON) == 1 && inTx == 1){
    //  put the transceiver to receive
    inTx = 0;
    delay(150);   // need tests for time
  }

}
// end checkTX()

/*CW is generated by keying the bias of a side-tone oscillator.
nonzero cwTimeout denotes that we are in cw transmit mode.
*/

void checkCW(){
  pinMode(TX_RX, OUTPUT);
  if (keyDown == 0 && analogRead(ANALOG_KEYER) < 50){
    //switch to transmit mode if we are not already in it
    if (inTx == 0){
      //put the TX_RX line to transmit
      digitalWrite(TX_RX, 1);
      si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA);
      si5351.output_enable(SI5351_CLK1, 0);
      si5351.set_freq(((rx*100L) - (rx600hz*100LL)), SI5351_CLK1);
      si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA);
    }
    inTx = 1;
    keyDown = 1;
    digitalWrite(CW_KEY, 1); //start the side-tone
  }

  //reset the timer as long as the key is down
  if (keyDown == 1){
    si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA);
    si5351.output_enable(SI5351_CLK1, 1);
    si5351.set_freq(((rx*100L) - (rx600hz*100LL)), SI5351_CLK1);
    si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA);
    cwTimeout = CW_TIMEOUT + millis();
  }

  //if we have a keyup
  if (keyDown == 1 && analogRead(ANALOG_KEYER) > 150){
     keyDown = 0;
     inTx = 0; 
     digitalWrite(CW_KEY, 0);  // stop the side-tone
     digitalWrite(TX_RX, 0);
     si5351.output_enable(SI5351_CLK1, 0);
     si5351.set_freq((1000000L), SI5351_CLK1);
     si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA);
     cwTimeout = millis() + CW_TIMEOUT;
  }

  //if we have keyuup for a longish time while in cw rx mode
  if (inTx == 1 && cwTimeout < millis()){
     //move the radio back to receive
	digitalWrite(CW_KEY, 0);
    digitalWrite(TX_RX, 0);
    si5351.output_enable(SI5351_CLK1, 0);
    si5351.set_freq((100000L), SI5351_CLK1);
    si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA);
    inTx = 0;
    cwTimeout = 0;
  }
}

// start variable SETUP

void setup() {

Wire.begin();
 
// Start serial and initialize the Si5351
 Serial.begin(115200);

// rotary
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  
// new
  //initialize the Si5351
  Serial.println("*Initialize Si5351\n");
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000L, cal); 
  
  //If you're using a 27Mhz crystal, put in 27000000 instead of 0
  // 0 is the default crystal frequency of 25Mhz.

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  Serial.println("*Fix PLL\n");
  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 1);
//BFO
  si5351.set_freq(rxbfo * 100ULL,  SI5351_CLK0);  //rxbfo
//SECOND OSCILATOR
  si5351.set_freq(SECOND_OSC * 100ULL, SI5351_CLK1); 
//VFO
  si5351.set_freq(((45000000 + rx - rxofset) * 100ULL), SI5351_CLK2); //4500000000l
  delay(2000);
  
  // Set CLK levels
  si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_8MA); //you can set this to 2MA, 4MA, 6MA or 8MA //6 8
  si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_8MA); //be careful though - measure into 50ohms  //4
  si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_8MA); //8
 
  Serial.println("*Enable PLL Output\n");
  Serial.println("*Si5351 ON\n");
  delay(10);
  
// new
  
//set up the pins in/out and logic levels
pinMode(TX_RX, OUTPUT);
digitalWrite(TX_RX, LOW);  

pinMode(CW_KEY, OUTPUT);
digitalWrite(CW_KEY, LOW);

pinMode(BTNDEC,INPUT);		// band change button
digitalWrite(BTNDEC,HIGH);    // level

pinMode(A0,INPUT); // Connect to a button that goes to GND on push - rotary encoder push button - for FREQ STEP change
digitalWrite(A0,HIGH);  //level

// Initialize the Serial port so that we can use it for debugging
  Serial.begin(115200);
  Serial.println("Start VFO ver 14.0 ssb");

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C address 0x3C (for oled 128x32)
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();

  // Clear the buffer.
	display.clearDisplay();	
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.println(rx);
	display.setTextSize(1);
	display.setCursor(0,16);
	display.print("St:");display.print(hertz);
	display.setCursor(64,16);
	display.print("rit:");display.print(rxRIT);
	display.display();
  
}

///// START LOOP - MAIN LOOP

void loop() {
//	checkCW();   		// when pres keyer
//	checkBTNdecode(); 	// BAND change
//	checkTX();
  
// freq change 
  if ((rx != rx2) || (RITon == 1)){
		  showFreq();
      sendFrequency(rx);
      rx2 = rx;
      }

//  step freq change + RIT ON/OFF  
  buttonstate = digitalRead(A0);
  if(buttonstate == LOW) {
        setincrement();        
    };

///	  SERIAL COMMUNICATION - remote computer control for DDS - 1, 2, 3, 4, 5, 6 - worked 
   /*  check if data has been sent from the computer: */
if (Serial.available()) {
    /* read the most recent byte */
    byteRead = Serial.read();
	if(byteRead == 49){     // 1 - up freq
		rx = rx + increment;
		sendFrequency(rx);
    Serial.println(rx);
		}
	if(byteRead == 50){		// 2 - down freq
		rx = rx - increment;
		sendFrequency(rx);
    Serial.println(rx);
		}
	if(byteRead == 51){		// 3 - up increment
		setincrement();
    Serial.println(increment);
		}
	if(byteRead == 52){		// 4 - print VFO state in serial console
		Serial.println("VFO_VERSION 14.0");
		Serial.println(rx);
		Serial.println(rxbfo);
		Serial.println(increment);
		Serial.println(hertz);
		}
  if(byteRead == 53){		// 5 - scan freq forvard 40kHz 
             var_i=0;           
             while(var_i<=40000){
                var_i++;
                rx = rx + 10;
                sendFrequency(rx);
                Serial.println(rx);
                showFreq();
                if (Serial.available()) {
					if(byteRead == 53){
					    break;						           
					}
				}
             }        
   }

   if(byteRead == 54){   // 6 - scan freq back 40kHz  
             var_i=0;           
             while(var_i<=40000){
                var_i++;
                rx = rx - 10;
                sendFrequency(rx);
                Serial.println(rx);
                showFreq();
                if (Serial.available()) {
                    if(byteRead == 54){
                        break;                       
                    }
                }
             }        
   }
   if(byteRead == 55){     // 1 - up freq
//<<<<<<< HEAD
    rxbfo = rxbfo + 1;
//=======
//    rxbfo = rxbfo + 10;
//>>>>>>> origin/master
	  INIT_LSB_FREQ = rxbfo;
    sendFrequency(rx);
    Serial.println(rxbfo);
   }
  if(byteRead == 56){   // 2 - down freq
//<<<<<<< HEAD
    rxbfo = rxbfo - 1;
//=======
//    rxbfo = rxbfo - 10;
//>>>>>>> origin/master
	  INIT_LSB_FREQ = rxbfo;
    sendFrequency(rx);
    Serial.println(rxbfo);
  }
}
	
}	  
/// END of main loop ///
/// ===================================================== END ============================================


/// START EXTERNAL FUNCTIONS


ISR(PCINT2_vect) {
  unsigned char result = r.process();
if (result) {  
	if (RITon==0){
		if (result == DIR_CW){rx=rx+increment;}
		else {rx=rx-increment;}
	}

  if (RITon==1){
      if (result == DIR_CW){
        rxRIT=rxRIT+50;
       }
  else {
       rxRIT=rxRIT-50;
       }
  } 
 
}
}

// new
void sendFrequency(double frequency) { 
	rx=frequency;

// BFO CLK	
	if (rx > 10000000L){
		isUSB = 1;
		rxbfo=INIT_USB_FREQ;
//	    digitalWrite(TX_LPF_SEL, 1);
	}
	else{
		isUSB = 0;
//		digitalWrite(TX_LPF_SEL, 0);
		rxbfo=INIT_LSB_FREQ;
	}
	si5351.set_freq(rxbfo * 100ULL, SI5351_CLK0);  //rxbfo 
  
// VFO CLK
    si5351.set_freq(((45000000 + rx - rxofset) * 100ULL), SI5351_CLK2);
 	Serial.println(rx);   // for serial console debuging
}
// new

// step increments for rotary encoder button
void setincrement(){
  if(increment == 0){increment = 1; hertz = "1Hz"; hertzPosition=0;RITon=0;} 
  else if(increment == 1){increment = 10; hertz = "10Hz"; hertzPosition=0;RITon=0;}
  else if(increment == 10){increment = 50; hertz = "50Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 50){increment = 100;  hertz = "100Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 100){increment = 500; hertz="500Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 500){increment = 1000; hertz="1Khz"; hertzPosition=0;RITon=0;}
  else if (increment == 1000){increment = 2500; hertz="2.5Khz"; hertzPosition=0;RITon=0;}
  else if (increment == 2500){increment = 5000; hertz="5Khz"; hertzPosition=0;RITon=0;}
  else if (increment == 5000){increment = 10000; hertz="10Khz"; hertzPosition=0;RITon=0;}
  else if (increment == 10000){increment = 100000; hertz="100Khz"; hertzPosition=0;RITon=0;}
  else if (increment == 100000){increment = 1000000; hertz="1Mhz"; hertzPosition=0;RITon=0;} 
  else{increment = 0; hertz = "ritON"; hertzPosition=0; RITon=1;};  
  showFreq();
  delay(250); // Adjust this delay to speed up/slow down the button menu scroll speed.
}

// oled display functions
void showFreq(){
	display.clearDisplay();	
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	//display.print("F:");
	display.println(rx);
	display.setTextSize(1);
	display.setCursor(0,16);
	display.print("St:");display.print(hertz);
	display.setCursor(64,16);
	display.print("rit:");display.print(rxRIT);
	display.display();
}


//  BAND CHANGE !!! band plan - change if need 
void checkBTNdecode(){
  
BTNdecodeON = digitalRead(BTNDEC);
if(BTNdecodeON != BTNlaststate){
    
    if(BTNdecodeON == HIGH){
         delay(250);
         BTNcheck2 = 1;
         BTNinc = BTNinc + 1;
         if(BTNinc > 6){
              BTNinc = 2;
              }
    }
    
    if(BTNdecodeON == LOW){
         BTNcheck2 = 0; 
    }
    
    BTNlaststate = BTNcheck2;
    
    switch (BTNinc) {
          case 1:
            rx=1810750;
            break;
          case 2:
            rx=3500750;
            break;
          case 3:
            rx=5250750;
            break;
          case 4:
            rx=7199000;
            break;
          case 5:
            rx=10100750;
            break;
          case 6:
            rx=14000000;
            break;
          case 7:
            rx=18068750;
            break;    
          case 8:
            rx=21000750;
            break;    
          case 9:
            rx=24890750;
            break;    
          case 10:
            rx=28000750;
            break;
          case 11:
            rx=29100750;
            break;    	  
          default:             
            break;
        }
    }

}

//// OK END OF PROGRAM
