#include <SoftwareSerial.h>
#include <Arduino.h>

class Ublox
{
  public:
    Ublox(SoftwareSerial *port);
    void setupGPS();
    void sendUBX(byte *UBXmsg, byte msgLength);
    void sleepyGPS();
  private:
    SoftwareSerial * _gps;
};
