
/*
 * Teensy 3.5 crude logger
 yoinked from hytech racing "telemetry control unit" repo
 removed all features except can logging to SD
 */
#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <TimeLib.h>
#include <Metro.h>
#include <FlexCAN_T4.h>
/*
 * CAN Variables
 */
#define CAN_CHANNEL CAN1 //CAN1 is what is connected on the LORA logger
FlexCAN_T4<CAN_CHANNEL, RX_SIZE_256, TX_SIZE_16> CAN;
static CAN_message_t msg_rx;
static CAN_message_t msg_tx;
// static CAN_message_t xb_msg;
File logger;
/*
 * Variables to help with time calculation
 */
uint64_t global_ms_offset = 0;
uint64_t last_sec_epoch;
Metro timer_debug_RTC = Metro(1000,1);
Metro timer_flush = Metro(5,1);
void digitalClockDisplay();
void printDigits(int digits);
void parse_can_message();
void write_to_SD(CAN_message_t *msg);
time_t getTeensy3Time();
void sd_date_time(uint16_t* date, uint16_t* time);
uint8_t blank[]={1,2,0,0,0,0,0,0};
void setup() {
    msg_tx.id=0xC9;
    memcpy(msg_tx.buf,blank,sizeof(msg_tx.buf));
    delay(5000); // Prevents suprious text files when turning the car on and off rapidly
    Serial.print("Firmware Version: ");
    Serial.println(AUTO_VERSION);   // Use the preprocessor directive
    pinMode(LED_BUILTIN,OUTPUT);
    /* Set up Serial, CAN */
    //Serial.begin(115200);

    /* Set up real-time clock */
    //Teensy3Clock.set(1665542615); // set time (epoch) at powerup  (COMMENT OUT THIS LINE AND PUSH ONCE RTC HAS BEEN SET!!!!)
    setSyncProvider(getTeensy3Time); // registers Teensy RTC as system time
    if (timeStatus() != timeSet) {
        Serial.println("RTC not set up - uncomment the Teensy3Clock.set() function call to set the time");
    } else {
        Serial.println("System time set to RTC");
    }
    last_sec_epoch = Teensy3Clock.get();
    
    //FLEXCAN0_MCR &= 0xFFFDFFFF; // Enables CAN message self-reception
    CAN.begin();
    CAN.setBaudRate(500000);
    /* Set up SD card */
    Serial.println("Initializing SD card...");
    SdFile::dateTimeCallback(sd_date_time); // Set date/time callback function
    if (!SD.begin(BUILTIN_SDCARD)) { // Begin Arduino SD API (Teensy 3.5)
        Serial.println("SD card failed or not present");
    }
    digitalClockDisplay();
    delay(1000);
    digitalClockDisplay();
    char filename[] = "data0000.CSV";
    for (uint8_t i = 0; i < 10000; i++) {
        filename[4] = i / 1000     + '0';
        filename[5] = i / 100 % 10 + '0';
        filename[6] = i / 10  % 10 + '0';
        filename[7] = i       % 10 + '0';
        if (!SD.exists(filename)) {
            logger = SD.open(filename, (uint8_t) O_WRITE | (uint8_t) O_CREAT); // Open file for writing
            break;
        }
        if (i == 9999) { // If all possible filenames are in use, print error
            Serial.println("All possible SD card log filenames are in use - please clean up the SD card");
        }
    }
    
    if (logger) {
        Serial.print("Successfully opened SD file: ");
        Serial.println(filename);
    } else {
        Serial.println("Failed to open SD file");
    }
    
    logger.println("time,msg.id,msg.len,data"); // Print CSV heading to the logfile
    logger.flush();
}
void loop() {
  digitalWrite(LED_BUILTIN,LOW);
    /* Process and log incoming CAN messages */
    parse_can_message();
    /* Flush data to SD card occasionally */
    if (timer_flush.check()) {
        logger.flush(); // Flush data to disk (data is also flushed whenever the 512 Byte buffer fills up, but this call ensures we don't lose more than a second of data when the car turns off)
    }
    /* Print timestamp to serial occasionally */
    if (timer_debug_RTC.check()) {
        Serial.println(Teensy3Clock.get());
        //CAN.write(msg_tx);
    }
}
void parse_can_message() {
    while (CAN.read(msg_rx)) {
        
            write_to_SD(&msg_rx); // Write to SD card buffer (if the buffer fills up, triggering a flush to disk, this will take 8ms)
                
    }
}
void write_to_SD(CAN_message_t *msg) { // Note: This function does not flush data to disk! It will happen when the buffer fills or when the above flush timer fires
    // Calculate Time
    //This block is verified to loop through
    digitalWrite(LED_BUILTIN,HIGH);

    uint64_t sec_epoch = Teensy3Clock.get();
    if (sec_epoch != last_sec_epoch) {
        global_ms_offset = millis() % 1000;
        last_sec_epoch = sec_epoch;
    }
    uint64_t current_time = sec_epoch * 1000 + (millis() - global_ms_offset) % 1000;

    // Log to SD
    Serial.print(current_time);
    Serial.print(",");
    Serial.print(msg->id,HEX);
    Serial.println();
    logger.print(current_time);
    logger.print(",");
    logger.print(msg->id, HEX);
    logger.print(",");
    logger.print(msg->len);
    logger.print(",");
    for (int i = 0; i < msg->len; i++) {
        if (msg->buf[i] < 16) {
            logger.print("0");
        }
        logger.print(msg->buf[i], HEX);
    }
    logger.println();
}
time_t getTeensy3Time() {
    return Teensy3Clock.get();
}
void sd_date_time(uint16_t* date, uint16_t* time) {
    // return date using FAT_DATE macro to format fields
    *date = FAT_DATE(year(), month(), day());
    // return time using FAT_TIME macro to format fields
    *time = FAT_TIME(hour(), minute(), second());
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print("_");
  Serial.print(day());
  Serial.print("_");
  Serial.print(month());
  Serial.print("_");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print("-");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}