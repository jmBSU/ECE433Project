/*
 * pt2.c
 *
 * Author : Joe Ma
 *
 * Color Sensor References: https://github.com/ControlEverythingCommunity/TCS34725
 * TFT Display Libraries: https://github.com/Matiasus/ST7735
 * Color code conversion: https://forum.arduino.cc/index.php?topic=174989.0
 */ 

#define F_CPU 8000000UL //Define used for delay and UART
#define BAUDRATE 9600UL //9600 Baud for UART
#define MYUBRR (F_CPU/8/BAUDRATE-1) 

//I2C COLOR SENSOR DEFINES//
#define SLAVE_ADDR (0x29<<1) //Slave Address at 0x29
#define SENSOR_ER 0x80 //Enable Register
#define SENSOR_TR 0x81 //Timing
#define SENSOR_WR 0x83 //Wait Time
#define SENSOR_CR 0x8F //Control
#define SENSOR_IDR 0x92 //ID
#define SENSOR_SR 0x93 //Status

#define SENSOR_CLEARL 0x94 //Low byte CLEAR
#define SENSOR_CLEARH 0x95 //High byte CLEAR
#define SENSOR_REDL 0x96 //Low byte RED
#define SENSOR_REDH 0x97 //High byte RED
#define SENSOR_GREENL 0x98 //Low byte GREEN
#define SENSOR_GREENH 0x99 //High byte GREEN
#define SENSOR_BLUEL 0x9A //Low byte BLUE
#define SENSOR_BLUEH 0x9B //High byte BLUE
/////////////////////////////////////////////////

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>
#include <util/delay.h>
#include "st7735.c" //TFT LCF Library

//Function Declartion
//UART used during initial debug and sensor communication
void UARTinit();
uint8_t UARTreceive();
void UARTsend(uint8_t);

//I2C/TWI
void TWIinit();
void TWIstart();
void TWIwrite(uint8_t);
uint8_t TWIread_ack();
void TWIstop();

//Consolidated Read and Write for I2C
void writeData(uint8_t registerWrite,uint8_t data);
uint8_t readData(uint8_t registerRead);
void colorRead();

//Global variables
char string[30];
uint16_t clear,red,green,blue,hexColor;
uint32_t hexColor24;

int main(void)
{

//	UARTinit();
	TWIinit(); //Start I2C
	St7735Init(); //Start TFT
	ClearScreen(0xffff); //Clear TFT
	
	//Sensor init and start
	writeData(SENSOR_TR,0x00);//Integration time = 700ms
	writeData(SENSOR_WR,0xFF);//Wait time = 2.4ms
	writeData(SENSOR_CR,0x00);//1x gain	
	writeData(SENSOR_ER,0x03);//Power on , ENABLE RGBC
	
// 	
	//Reading sensor device ID and writes to TFT
	TWIstart();
	TWIwrite(SLAVE_ADDR|TW_WRITE);
	TWIwrite(SENSOR_IDR);
	TWIstart();
	TWIwrite(SLAVE_ADDR|TW_READ);
	SetPosition(20,10);
	char data[10];
	sprintf(data,"Device ID: 0x%X",TWIread_ack());
	DrawString(data,0x0000,X1);
	DrawLine(15,115,20,20,0x04af);
	_delay_ms(500);
	TWIstop();
//
	//Reads colors and writes to TFT
	colorRead();
	
	while(1){
		//Polls ever second
		_delay_ms(1000);
		
		ClearScreen(0xffff); //Clears TFT
		
		//Reading sensor device ID and writes to TFT
		TWIstart();
		TWIwrite(SLAVE_ADDR|TW_WRITE);
		TWIwrite(SENSOR_IDR);
		TWIstart();
		TWIwrite(SLAVE_ADDR|TW_READ);
		SetPosition(20,10);
		char data[10];
		sprintf(data,"Device ID:0x%X",TWIread_ack());
		DrawString(data,0x0000,X1);
		DrawLine(15,115,20,20,0x04af);
		_delay_ms(500);
		TWIstop();
		
		//Reads colors and writes to TFT
		colorRead();
		
	}
}


void UARTinit(){
	UBRR0 = MYUBRR; //Set 9600 baud
	UCSR0A = 1<<U2X0; //2x speed
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0); //Rx TX RXinterrupt ON
	UCSR0C = 3<<UCSZ00; //Frame 8bit 1sb
}

uint8_t UARTreceive(){
	return UDR0; //Read from reg
}

void UARTsend(uint8_t data){
	while(!(UCSR0A & (1<<UDRE0))){}; //Waits for empty buffer
		UDR0 = data;
}

void TWIinit(){
	TWSR = 1<<TWPS1; //Div by 16
	TWBR = 2; //Run I2C at  100khz
	TWCR = 1<<TWEN;
}

void TWIstart(){
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTA); //Starts TWI
	while (!(TWCR & (1<<TWINT))){}; //Waits for TWINT to set
}

void TWIwrite(uint8_t data){//MCU drives the data on SDA
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);
	while (!(TWCR & (1<<TWINT))){};
}

/*
*	Consolidated function for writing to sensor 
*	uint8_t registerWrite	8 bit register to write to
*	uitn8_t data 	8 bit data to write
*/
void writeData(uint8_t registerWrite,uint8_t data){
	TWIstart();
	TWIwrite(SLAVE_ADDR|TW_WRITE);
	TWIwrite(registerWrite);
	TWIwrite(data);
	TWIstop();
}

uint8_t TWIread_ack(){ //Slave drives data on SDA
	TWCR = (1<<TWINT) | (1<<TWEN); //| (1<<TWEA); 
	while (!(TWCR & (1<<TWINT))){};
	uint8_t data = TWDR;
	return data;
}

/*
*	Consolidated function for reading to this particular sensor
*	uint8_t registerRead	8 bit register to read from
*	returns uitn8_t		8 bit data that is returned
*/

uint8_t readData(uint8_t registerRead){
	TWIstart();
	TWIwrite(SLAVE_ADDR|TW_WRITE);
	TWIwrite(registerRead);
	TWIstart();
	TWIwrite(SLAVE_ADDR|TW_READ);
	uint8_t data = TWIread_ack();
	TWIstop();
	return data;
}

void TWIstop(){//Sets I2C to stop state
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	while ((TWCR & (1<<TWSTO))){}; //Waits for STOP to execute
}

void colorRead(){ //Reads all the color sensors and writes to the TFT
	
	memset(string,0,sizeof(string)); //Clears string
	clear = (readData(SENSOR_CLEARH)<<8)|(readData(SENSOR_CLEARL)); //Reads sensor and stores as 16bit value
	red = (readData(SENSOR_REDH)<<8)|(readData(SENSOR_REDL));
	green = (readData(SENSOR_GREENH)<<8)|(readData(SENSOR_GREENL));
	blue = (readData(SENSOR_BLUEH)<<8)|(readData(SENSOR_BLUEL));
	
	_delay_ms(100);//TFT aesthetic choice for cascaded data
	sprintf(string,"CLEAR: 0x%X",clear); //Writes hex value of sensor to TFT 
	SetPosition(20,30);
	DrawString(string,0x0000,X1);
	memset(string,0,sizeof(string));
	
	_delay_ms(100);
	sprintf(string,"RED: 0x%X",red);
	SetPosition(20,40);
	DrawString(string,0x0000,X1);
	memset(string,0,sizeof(string));

	_delay_ms(100);
	sprintf(string,"GREEN: 0x%X",green);
	SetPosition(20,50);
	DrawString(string,0x0000,X1);
	memset(string,0,sizeof(string));

	_delay_ms(100);
	sprintf(string,"BLUE: 0x%X",blue);
	SetPosition(20,60);
	DrawString(string,0x0000,X1);
	memset(string,0,sizeof(string));
	
	//Converts the sensor values to RGB888 for simpler conversion to RGB565 which is what the TFT uses
	hexColor24 = (((red >> 7) & 0xFFFFFF) << 15) | (((green >> 7) & 0xFFFFFF) << 7) | ((blue >> 7) & 0xFFFFFF);
	hexColor = (((hexColor24&0xF80000)>>8) | ((hexColor24&0xfc00)>>5) | ((hexColor24&0xf8)>>3));
	//Source for HexColor: https://forum.arduino.cc/index.php?topic=174989.0

	_delay_ms(100);
	SetPosition(20,80);
	sprintf(string,"Approx. Color: 0x%X",hexColor);//Writes the converted hexColor code - Which is an approximate
	DrawString(string,0x0000,X1);
	memset(string,0,sizeof(string));
	
	_delay_ms(100);
	DrawRectangle(20,115,90,120,hexColor); //Creates a rectangle that is of the color code.
	UpdateScreen(); //refreshes the screen
}
