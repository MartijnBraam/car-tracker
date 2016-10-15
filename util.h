#include <Arduino.h>


unsigned int calculateChecksum(char* input);
void blink(int times);

void fmtDouble(double val, byte precision, char *buf, unsigned bufLen = 0xffff);
unsigned fmtUnsigned(unsigned long val, char *buf, unsigned bufLen = 0xffff, byte width = 0);
