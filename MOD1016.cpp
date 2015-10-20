#include "Arduino.h"
#include "Wire.h"
#include "MOD1016.h"

volatile bool displayingFrequency;
volatile sgn32 pulse;
sgn32 cap_frequencies[16];

void pulseDetected() {
	if (displayingFrequency)
		pulse++;
}

void autoTuneCaps(int irq) {
	int fdiv = mod1016.getDivisionRatio();
	//Serial.print("Multiply IRQ frequency by ");
	//Serial.println(fdiv);
	Serial.println("Measuring frequency. Please wait...\n");
	freqPerTuneCaps(fdiv, irq);
	recommendTuning();
	mod1016.calibrateRCO();
	delay(1000);
}

sgn32 getFrequency(int irq) {
	mod1016.writeRegister(DISP_LCO, 0x80);
	displayingFrequency = true;
	pulse = 0;
	attachInterrupt(digitalPinToInterrupt(irq), pulseDetected, RISING);
	delay(200);
	displayingFrequency = false;
	detachInterrupt(irq);
	mod1016.writeRegister(DISP_LCO, 0x00);
	return (pulse * 5);
}

void freqPerTuneCaps(int fdiv, int irq) {
	for (int i = 0; i < 16; i++) {
		mod1016.setTuneCaps(i);
		delay(2);
		/*Measure frequency 5x. Get average.
		If you want to use a slower aproach that measures frequency over a longer period of time, uncomment this section, 
		and comment out the other sgn32 freq line.
		
		sgn32 freq = 0;
		for (int i = 0; i < 5; i++) {
		  freq += getFrequency(irq);
		}
		freq = ((freq / 5) * fdiv) - 500000;
		
		/*Measure frequency in 1/5th of a second, multiply by 500000*/
		sgn32 freq = (getFrequency(irq) * fdiv) - 500000;

		cap_frequencies[i] = (freq < 0) ? -freq : freq;
		//Uncomment to print out frequnecy calculations
		/*
		Serial.print("TUNE CAPS = ");
		Serial.print(i);
		Serial.print("\tFrequency - 500k = ");
		Serial.println(cap_frequencies[i]);
		Serial.println();*/
	}
}

void recommendTuning() {
	int best = 0, next, current;
	for (int i = 1; i < 16; i++) {
		current = best;
		next = i;
		best = (cap_frequencies[next] < cap_frequencies[current]) ? next : best; 
	}
	//Serial.print("Best value for TUNE_CAPS - ");
	//Serial.println(best);
	Serial.println("Setting TUNE_CAPS...");
	mod1016.setTuneCaps(best);
}

/*--------------------------------------------------------*/

void MOD1016Class::init(int IRQ_pin) {
	calibrateRCO();
	pinMode(IRQ_pin, INPUT);
}

void MOD1016Class::calibrateRCO() {
	Wire.beginTransmission(MOD1016_ADDR);
	Wire.write(0x3D);
	Wire.write(0x96);
	Wire.endTransmission();

	writeRegister(DISP_TRCO, (0x01 << 5));
	delay(2);
	writeRegister(DISP_TRCO, (0x00 << 5));
}

uns8 MOD1016Class::readRegisterRaw(uns8 reg) {
	Wire.beginTransmission(MOD1016_ADDR);
	Wire.write(reg);
	Wire.endTransmission(false);
	uns8 result;
	Wire.requestFrom(MOD1016_ADDR, 1);
	if (Wire.available()) {
		result = Wire.read();
	}
	return result;
}

uns8 MOD1016Class::readRegister(uns8 reg, uns8 mask) {
	uns8 data = readRegisterRaw(reg);
	return (data & mask);
}

void MOD1016Class::writeRegister(uns8 reg, uns8 mask, uns8 data) {
	uns8 currentReg = readRegisterRaw(reg);
	currentReg = currentReg & (~mask);
	data |= currentReg;
	Wire.beginTransmission(MOD1016_ADDR);
	Wire.write(reg);
	Wire.write(data);
	Wire.endTransmission();
}

void MOD1016Class::setIndoors() {
	writeRegister(AFE_GB, INDOORS);
}

void MOD1016Class::setOutdoors() {
	writeRegister(AFE_GB, OUTDOORS);
}

void MOD1016Class::setNoiseFloor(uns8 noise) {
	writeRegister(NOISE_FLOOR, (noise << 4));
}

void MOD1016Class::setTuneCaps(uns8 tune) {
	writeRegister(TUNE_CAPS, tune);
	delay(2);
}

uns8 MOD1016Class::getNoiseFloor() {
	return (readRegister(NOISE_FLOOR) >> 4);
}

uns8 MOD1016Class::getAFE() {
	return (readRegister(AFE_GB) >> 1);
}

uns8 MOD1016Class::getTuneCaps() {
	return readRegister(TUNE_CAPS);
}

uns8 MOD1016Class::getIRQ() {
	delay(2);
	return readRegister(IRQ_TBL);
}

uns8 MOD1016Class::getLightDistance() {
	return readRegister(LGHT_DIST);
}

int MOD1016Class::calculateDistance() {
	uns8 dist = getLightDistance();
	int km;		
	switch (dist) {
		case 0x3F:
			km = -1;
			break;
		case 0x28:
			km = 40;
			break;
		case 0x25:
			km = 37;
			break;
		case 0x22:
			km = 34;
			break;
		case 0x1F:
			km = 31;
			break;
		case 0x1B:
			km = 27;
			break;
		case 0x18:
			km = 24;
			break;
		case 0x14:
			km = 20;
			break;
		case 0x11:
			km = 17;
			break;
		case 0x0E:
			km = 14;
			break;
		case 0x0C:
			km = 12;
			break;
		case 0x0A:
			km = 10;
			break;
		case 0x08:
			km = 8;
			break;
		case 0x06:
			km = 6;
			break;
		case 0x05:
			km = 5;
			break;
		case 0x01:
			km = 0;
			break;
		default:
			km = 1;
	}
	return km;
}

int MOD1016Class::getDivisionRatio() {
	uns8 fdiv = (readRegister(LCO_FDIV) >> 6);
	if (fdiv == 0)
		return 16;
	else if (fdiv == 1)
		return 32;
	else if (fdiv == 2) 
		return 64;
	else	
		return 128;
}

MOD1016Class mod1016;