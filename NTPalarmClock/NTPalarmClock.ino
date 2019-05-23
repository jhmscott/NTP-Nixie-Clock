#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

#define SECONDS_IN_A_DAY        86400ul
#define SECONDS_IN_A_YEAR       365*SECONDS_IN_A_DAY 
#define SECONDS_IN_A_LEAP_YEAR  366*SECONDS_IN_A_DAY 
#define SECONDS_IN_A_HOUR       3600ul
#define SECONDS_IN_A_MINUTE     60
#define TIME_SERVER             "time.nist.gov"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};


const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

enum Day {
  Monday,
  Tuesday,
  Wednesday,
  Thursday,
  Friday,
  Saturday,
  Sunday
};

struct Date {
  unsigned int year;
  Day dayOfTheWeek;
  byte month;
  byte day;
  byte hour;
  byte minute;
  byte seconds;
};

EthernetUDP   myUDP;
unsigned long unixTime;
Date          currentDate;
int           numSeconds;
unsigned long localTimer;

//configuration
int           timeZone     = -8;
boolean       DSTregion   = true;
boolean       time24hour  = false;
void setup() {
 
  Serial.begin(9600);
  
  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    while(1);
  }
  
  numSeconds = 300;
  localTimer = millis();

  //setup UDP port
  myUDP.begin(8888);
}

void loop() {

  //after 5 minutes, check the server again
  if(numSeconds >= 300){
    do {
      unixTime = ntpUnixTime(myUDP);  //update local time to match server
      numSeconds = 0;                 //reset counter
    } while(unixTime == 0);           //ntpUnixTime returns 0 if fails, loop until it doesn't
  }

  //after 1 second
  if((millis()-localTimer) >= 1000) {
    localTimer = millis();            //reset localTimer variable
    unixTime++;                       //increase local time by one second
    numSeconds++;                     //count another second

    //calculate time from unix time and print it
    currentDate = timeToDate(unixTime+(((long)timeZone+1)*SECONDS_IN_A_HOUR));
    serialPrintDate(currentDate);
  }
  
}

void serialPrintDate(Date date){
  clearSerialTerminal();

  //print the date
  Serial.print(date.month);
  Serial.print("/");
  Serial.print(date.day);
  Serial.print("/");
  Serial.print(date.year);
  Serial.print("  ");

  //if 12 hour time format and its past noon
  if(date.hour > 12 && time24hour == false){
    Serial.print(date.hour-12);
  }
  //if 24 hour time format and midnight
  else if(time24hour == true && date.hour == 0){
    Serial.print(24);
  }
  //if 12 hour time format and midnight
  else if (time24hour == false && date.hour == 0){
    Serial.print(12);
  }
  //otherwise just print the hour
  else{
    Serial.print(date.hour);
  }

  
  Serial.print(":");
  if(date.minute < 10){         //if less than 10 add place holder 0
    Serial.print('0');
  }
  Serial.print(date.minute);    //print the minute
  
  Serial.print(":");
  if(date.seconds < 10){        //if less than 10 add place holder 0
    Serial.print('0');
  }
  Serial.println(date.seconds); //print seconds
  
}


void clearSerialTerminal(){
  Serial.write(27);       // ESC command
  Serial.print("[2J");    // clear screen command
  Serial.write(27);
  Serial.print("[H");     // cursor to home command
}



Date timeToDate(unsigned long time){
  Date date;

  //calculate day of the week using modulus
  date.dayOfTheWeek = byteToDay((time/SECONDS_IN_A_DAY)%7);

  //calculate year
  date.year = 1970;
  while(time > secondsInTheYear(date.year)){
    time = time - secondsInTheYear(date.year);
    date.year++;
  }

  //calculate month
  date.month = 1;
  while(time > secondsInTheMonth(date.month, date.year)){
    time = time - secondsInTheMonth(date.month, date.year);
    date.month++;
  }

  //calculate day
  date.day = 1;
  while(time > SECONDS_IN_A_DAY){
    date.day++;
    time = time - SECONDS_IN_A_DAY;
  }

  //calculate hour
  date.hour = time/SECONDS_IN_A_HOUR;
  time = time%SECONDS_IN_A_HOUR;

  //calculate minute
  date.minute = time/SECONDS_IN_A_MINUTE;
  time = time%SECONDS_IN_A_MINUTE;

  //calculate second
  date.seconds = time;

  return date;
}


Day byteToDay(byte dayNum){
  Day returnValue;
  switch(dayNum){
    case 0: {
      //January 1st 1970 was a thursday
      returnValue = Thursday;
      break;
    }
    case 1: {
      returnValue = Friday;
      break;
    }
    case 2: {
      returnValue = Saturday;
      break;
    }
    case 3: {
      returnValue = Sunday;
      break;
    }
    case 4:{
      returnValue = Monday;
      break;
    }
    case 5:{
      returnValue = Tuesday;
      break;
    }
    case 6:{
      returnValue = Wednesday;
      break;
    }
  }
}

unsigned long secondsInTheMonth(byte month, unsigned int year){
  unsigned long seconds;
  
  if(month == 2){
    //February in a leap year
    if(isLeapYear(year)){
      seconds = 29*SECONDS_IN_A_DAY;
    }
    //normal February
    else{
      seconds = 28*SECONDS_IN_A_DAY;
    }
  }
  else if(month == 1 || month == 3 || month == 5 ||month == 7 || month == 8 ||month == 10 || month ==12) {
    seconds = 31*SECONDS_IN_A_DAY;
  }
  else{
    seconds = 30*SECONDS_IN_A_DAY;
  }
  
  return seconds;
}

unsigned long secondsInTheYear(unsigned int year){
  if(isLeapYear(year)){
    return SECONDS_IN_A_LEAP_YEAR;
  }
  else{
    return SECONDS_IN_A_YEAR;
  }
}

boolean isLeapYear(unsigned int year){
  if( year % 4 == 0) {
    return true;
  }
  else {
    return false;
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(UDP &udp) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(TIME_SERVER, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


unsigned long ntpUnixTime(UDP &udp){
  sendNTPpacket(udp); // send an NTP packet to a time server

  // wait to see if a reply is available
  delay(100);
  if (udp.parsePacket()) {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    return secsSince1900 - 2208988800ul;
  }
  else {
    return 0;
  }
}

