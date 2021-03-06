///////////////////////////////////////////////////////////////////////////////
/*
 * 	Malcolm Ma
 * 	UT Dallas
 * 	Dec 2015
 * 	Built with CCSv6.1.1
 *
 * 	Version: v0.1
 *
 * 	Description:
 * 	This is a program to evaluate the TI's sensor hub boards.Due to the tight
 * 	schedule,not all the functions of sensors are developed.For example,there
 * 	are several modes in ISL29023 but it's only set to detect the ambient
 * 	light.However,there main difference of each mode is only sending the
 * 	different commands to different registers.
 *
 * 	A simple user interface is used for selecting different sensors to
 * 	evaluate.Two buttons on the MSP430 are used to select sensors and to switch
 * 	between low power mode and working mode.All the data are sent to PC and
 * 	plotted in SerialChart.
 *
 * 	Note:All the functions with prefix 'read' return the raw data of a sensor,
 * 	and all the functions with prefix 'get' return the converted data.
 */
///////////////////////////////////////////////////////////////////////////////


#include <msp430.h>
#include "I2C.h"
#include "UART.h"
#include "ISL29023.h"
#include "TEMP006.h"
#include "SHT21.h"
#include "MPU9150.h"
#include "BMP180.h"
#include "filter.h"
#include "math.h"

float tempMPU9150_f;
float tempBMP180_f;
float tempTMP006_f;
float tempSHT21_f;
float tempfiltered;

float humSHT21_f;
float ambientLight=0;
float humidityfiltered;
float lightfiltered;

float acc[3]={0};
float gyro[3]={0};
float mgn[3]={0};
float angleFusion=0;
float angleAcc=0;
float aTest=0;

float absolutePressure;
float sealvlPressure;
float altitude;

float test=12345.7890;
char buff[20]="123";

char sensorMode=0;
char lastMode=4;
char lowPowerMode=0;
int buttonFilter=5;

void initDebugging()
{
	  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
	  P1DIR |= BIT0;//P1.0,LED1 for indicating sensors are gathering data
	  P4DIR |= BIT7;//P4.7,LED2 for sending data to PC

	  P2DIR &= ~BIT1;//P2.1 for wake-up/sleep mode
	  P2REN |= BIT1;// Enable internal resistance
	  P2OUT |= BIT1;// Set  as pull-Up resistance
	  P2IES &= ~BIT1;//  Hi/Lo edge
	  P2IFG &= ~BIT1;// IFG cleared
	  P2IE |= BIT1;// interrupt enabled

	  P1DIR &= ~BIT1;//P1.1 for selecting what sensors to evaluate
	  P1REN |= BIT1;// Enable internal resistance
	  P1OUT |= BIT1;// Set  as pull-Up resistance
	  P1IES &= ~BIT1;//  Hi/Lo edge
	  P1IFG &= ~BIT1;// IFG cleared
	  P1IE |= BIT1;// interrupt enabled
}
void delayMs(unsigned int n)
{
	unsigned int i;
	for(i=0;i<n;i++)
		__delay_cycles(1000);//1ms at 1Mhz
}
inline void enblePINInt()
{
	P1IFG &= ~BIT1;                          // P1.4 IFG cleared
	P2IFG &= ~BIT1;                          // P1.4 IFG cleared
	P2IE |= BIT1;// interrupt enabled
	P1IE |= BIT1;// interrupt enabled
	}

inline void disablePINInt()
{
	P1IFG &= ~BIT1;                          // P1.4 IFG cleared
	P2IFG &= ~BIT1;                          // P1.4 IFG cleared
	P2IE &= ~BIT1;// interrupt disabled
	P1IE &= ~BIT1;// interrupt disabled
}

inline void blinkLED1(char n)
{
	int i;
	for(i=0;i<n;i++)
	{
		LED1on();
		delayMs(300);
		LED1off();
		delayMs(300);
	}

}

inline void LED1on()
{
	P1OUT|=BIT0;
}

inline void LED1off()
{
	P1OUT&=~BIT0;
}
inline void LED2on()
{
	P4OUT|=BIT7;
}

inline void LED2off()
{
	P4OUT&=~BIT7;
}

void initSensors()
{
	initISL29023();
	__no_operation();
	initTEMP006();
	__no_operation();
	initSHT21();
	__no_operation();
	initMPU9150();
	__no_operation();
	initBMP180();
	__no_operation();
}

void readSensors()
{
	ambientLight=getISL29023AMB();
	__no_operation();

	tempTMP006_f=getTMP006AMB();
	__no_operation();

	humSHT21_f=getSHT21HUM();
	tempSHT21_f=getSHT21TEMP();
	__no_operation();

	tempMPU9150_f=readMPU6050Temp();
	getMPU6050Acc(acc,&acc[1],&acc[2]);
	getMPU6050Gyro(gyro,&gyro[1],&gyro[2]);
	getMPU6050Mgn(mgn,&mgn[1],&mgn[2]);
	__no_operation();

	tempBMP180_f=getBMP180Temp();
	absolutePressure=getBMP180Prs(tempBMP180_f);
	sealvlPressure=getSealevel();
	altitude=getAltitude();
	__no_operation();
}

void sendSensorsData()
{
	print(tempMPU9150_f);//aqua
	print(tempBMP180_f);
	print(tempTMP006_f);
	print(tempSHT21_f);

	print(humSHT21_f);
	print(ambientLight);

	print(absolutePressure);

	print(acc[0]);print(acc[1]);print(acc[2]);
	print(gyro[0]);print(gyro[1]);print(gyro[2]);
	print(mgn[0]);print(mgn[1]);print(mgn[2]);

	printchar('\n');
	__no_operation();
}

void sendTemperature()
{
	print(tempMPU9150_f);//aqua
	print(tempBMP180_f);
	print(tempTMP006_f);
	print(tempSHT21_f);
	print(tempSHT21_f);

	printchar('\n');
	__no_operation();
}


void checkState()
{
	if((--buttonFilter)<0)
		buttonFilter=0;

	if(lowPowerMode!=0)
	{
		P2IFG &= ~BIT1;                          // P1.4 IFG cleared
		printstring("low power mode\n",15);
		blinkLED1(4);
		enblePINInt();
		__bis_SR_register(LPM4_bits + GIE);       // Enter LPM4 w/interrupt
		__no_operation();
		P2IFG &= ~BIT1;                          // P1.4 IFG cleared
		printstring("working mode\n",13);
		blinkLED1(4);
		lastMode=4;
	}
	if((sensorMode!=lastMode))
	{
		if(sensorMode==0)
			printstring("sensorMode0\n",12);
		else if(sensorMode==1)
			printstring("sensorMode1\n",12);
		else if(sensorMode==2)
			printstring("sensorMode2\n",12);
		lastMode=sensorMode;
	}
	enblePINInt();				//enable selecting mode,pin interrupt disabled after ISR
}
int main(void)
{
	initDebugging();
	clearI2CPort();
	__no_operation();
	initI2C();
	__no_operation();
	initUART();
	__no_operation();

	initSensors();

/*
	//Testing LED
	while(1)
	{
		//P1OUT ^= BIT0;                            // P1.0 = toggle
		blinkLED1(3);
	}
*/
	printstring("initialize success\n",19);
	printstring("start working\n",14);
	while(1)
	{
		checkState();

		switch(sensorMode)
		{
			case 0:					//detecting temperature only
				LED1on();
				tempTMP006_f=getTMP006AMB();
				tempSHT21_f=getSHT21TEMP();
				tempMPU9150_f=readMPU6050Temp();
				tempBMP180_f=getBMP180Temp();
				__no_operation();
				LED1off();

				tempfiltered=averageFilter((tempTMP006_f+tempSHT21_f+tempBMP180_f)/3.0);

				LED2on();
				print(tempMPU9150_f+3);//aqua
				print(tempBMP180_f);
				print(tempTMP006_f);
				print(tempSHT21_f);
				print(tempfiltered);
				printchar('\n\n');
				__no_operation();
				LED2off();

				break;
			case 1:				//detecting humidity,ambient light,air pressure
				LED1on();
				ambientLight=getISL29023AMB();
				humSHT21_f=getSHT21HUM();
				//tempSHT21_f=getSHT21TEMP();//SHT21 is faster then BMP180
				//absolutePressure=getBMP180Prs(tempSHT21_f);
				tempBMP180_f=getBMP180Temp();
				absolutePressure=getBMP180Prs(tempBMP180_f);
				__no_operation();
				LED1off();

				//humidityfiltered=FIR(humSHT21_f);
				lightfiltered=medianFilter(ambientLight);

				LED2on();
				print(humSHT21_f);
				print(ambientLight);
				print(absolutePressure);
				print(lightfiltered);
				printchar('\n');
				__no_operation();
				LED2off();

				break;
			case 2:				//gyro and accel
				LED1on();
				getMPU6050Acc(acc,&acc[1],&acc[2]);
				getMPU6050Gyro(gyro,&gyro[1],&gyro[2]);
				//getMPU6050Mgn(mgn,&mgn[1],&mgn[2]);
				__no_operation();
				LED1off();

				aTest-atan2(acc[0],acc[2]);
				angleAcc=-atan2(acc[0],acc[2])*180/3.1415926;
				angleFusion=AngleCalculate (angleFusion,gyro[2],acc[0],acc[2]);

				LED2on();
				print(gyro[2]);
				print(angleAcc);
				print(angleFusion);

				printchar('\n');
				__no_operation();
				LED2off();

				break;
		}
		__no_operation();

	}
}


// Port 1 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT1_VECTOR))) Port_1 (void)
#else
#error Compiler not supported!
#endif
{
	//prevent multiple entry
	P1IFG &= ~BIT1; // P1.1 IFG cleared
	P1IE &= ~BIT1;// interrupt disabled

	if(lowPowerMode==0&&buttonFilter==0)
	{
		sensorMode=(++sensorMode%3);
		buttonFilter=5;
	}

}

// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT1_VECTOR))) Port_1 (void)
#else
#error Compiler not supported!
#endif
{
	//prevent multiple entry
	if(lowPowerMode==0&&buttonFilter==0)
	{
		lowPowerMode=1;
	}
	else if(lowPowerMode==1&&buttonFilter==0)
	{
		lowPowerMode=0;
		buttonFilter=5;
	}

	if(lowPowerMode==0)
		__bic_SR_register_on_exit(LPM4_bits);   // Exit LPM4

	P2IFG &= ~BIT1;                          // P1.4 IFG cleared
}
