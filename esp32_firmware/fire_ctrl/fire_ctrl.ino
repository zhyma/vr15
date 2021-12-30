//This example code is in the Public Domain (or CC0 licensed, at your option.)
//By Evandro Copercini - 2018
//
//This example creates a bridge between Serial and Classical Bluetooth (SPP)
//and also demonstrate that SerialBT have the same functionalities of a normal Serial

#include "BluetoothSerial.h"

#include <Wire.h>
#include <VL6180X.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
VL6180X sensor;

// output
// bolt catch
const int bcPin = 2;
const int macPin = 13;
// input
const int firePin = 15;
const int selPin = 12;
const int magPin = 14;
const int boltPin = 27;

int cnt = 0;
uint8_t prev_states[] = {0xFA, 0x00, 0x00, 0x00,0xAF};
uint8_t com_buffer[]  = {0xFA, 0x00, 0x00, 0x00,0xAF};

const int buffer_len = 7;
const int low = 0x15;
//const int high = 0x44;
uint16_t sensor_buffer[buffer_len] = {0};
int buffer_ptr = 0;

bool read_io(int pin){
  int val = digitalRead(pin);
  if (val == HIGH)
    return true;
  else
    return false;
}

int switches_update(){
  bool mag_in = read_io(magPin);
  bool fire = read_io(firePin);
  bool burst = read_io(selPin);
  bool bolt_open = read_io(boltPin);
  uint8_t states = 0;
  if (bolt_open)
    states |= 1 << 3;
  if (mag_in)
    states |= 1 << 2;
  if (fire)
    states |= 1 << 1;
  if (burst)
    states |= 1 << 0;

  com_buffer[3] = states;
}

int state_update(){
  // update switches' states and sensor's reading
  switches_update();

  sensor_buffer[buffer_ptr] = sensor.readRangeSingleMillimeters();
  buffer_ptr = (buffer_ptr+1)%buffer_len;
  uint16_t sum = 0;
  uint16_t s_min = 0xff;
  uint16_t s_max = 0;
  for (int i=0; i < buffer_len; i++)
  {
    sum += sensor_buffer[i];
    if (sensor_buffer[i] < s_min)
      s_min = sensor_buffer[i];
    if (sensor_buffer[i] > s_max)
      s_max = sensor_buffer[i];
  }
  uint16_t dist = (sum - s_min - s_max)/(buffer_len-2);
  if (dist < low)
    dist = low;
//  else if (dist > high)
//    dist = high;
//  uint16_t dist = sensor.readRangeSingleMillimeters();
  com_buffer[1] = dist >> 8;
  com_buffer[2] = dist & 0xff;
  
  int flag = 0;
  for (int i=0; i < 5; i++){
    if (prev_states[i]!=com_buffer[i])
      flag += 1;
  }
  return flag;
}

int prev_update(){
  // update last states
  for (int i=0; i < 5; i++)
    prev_states[i] = com_buffer[i];
}

void setup() {
  // set output pins
  pinMode(bcPin, OUTPUT);
  digitalWrite(bcPin, LOW);
  pinMode(macPin, OUTPUT);
  digitalWrite(macPin, LOW);

  // set input pins
  pinMode(firePin, INPUT);
  pinMode(selPin, INPUT);
  pinMode(magPin, INPUT);
  pinMode(boltPin, INPUT);
  
  Serial.begin(115200);
  SerialBT.begin("VR15"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");

  Wire.begin();
  
  sensor.init();
  sensor.configureDefault();
  sensor.setTimeout(50);

  for(int i=0; i < buffer_len; i++)
  {
    sensor_buffer[i] = sensor.readRangeSingleMillimeters();
    delay(50);
  }
}

void loop() {
  if (SerialBT.available())
  {
    // receive a "fire" command from the Unity
    uint8_t buff = SerialBT.read();
//    Serial.write(buff);
    int fire = int((buff >> 4) & 0x0F);
    int lock = int(buff & 0x0F);
    int fire_timer = 150*fire/15;
    int lock_timer = 150*lock/15;
    digitalWrite(macPin, HIGH);
    if (lock > 0)
    {
      digitalWrite(bcPin, HIGH);
    }
    delay(fire_timer);
    digitalWrite(macPin, LOW);
    if (lock > 0)
    {
      delay(lock_timer);
    }
    digitalWrite(bcPin, LOW);
    switches_update();
    // Reply "done" and switches states
    com_buffer[3] |= 1<<4;
    SerialBT.write(com_buffer, 5);
  }
  else
  {
    int state_flag = state_update();

    if (state_flag != 0)
    {
      // There is a state update needs to be reported. Send one command.
      SerialBT.write(com_buffer, 5);
      prev_update();
    }
    else if (cnt >= 1000)
    {
      // No command has been sent for a while. Send one.
      cnt = 0;
      
      // no command from the upstream, read sensors' data and send them back.
      // write charging handle position
      SerialBT.write(com_buffer, 5);
    }
    else
    {
      cnt++;
      delay(1);
    }
  }
}
