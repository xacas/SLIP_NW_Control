/*
 * Arduino_NW_Control Driver example.
 *
 * Based upon uIP SMTP example by Adam Dunkels <adam@sics.se>
 * SerialIP was ported to Arduino IDE by Adam Nielsen <malvineous@shikadi.net>
 * This code is based on Mr. @HppyCtrlEngnrng 's example  <https://qiita.com/HppyCtrlEngnrng>
 * This code is written by Masahiro Kusaka <masahiro906@gmail.com>
 */

#include <SerialIP.h>
#include <TimerOne.h>

// PIN Setting
#define INPUT_PIN 3
#define OUTPUT_PIN_1 6
#define OUTPUT_PIN_2 5

// This is the Control server to connect to.  This default assumes an Control server
// on your PC.  Note this is a parameter list, so use commas not dots!
#define CONTROL_SERVER 192,168,5,1

// IP and subnet that the Arduino will be using.  Commas again, not dots.
#define MY_IP 192,168,5,2
#define MY_SUBNET 255,255,255,0

// Quantize Gain
#define Q_GAIN 48.0

// If you're using a real Internet Control server, the Arduino will need to route
// traffic via your PC, so set the PC's IP address here.  Note that this IP is
// for the PC end of the SLIP connection (not any other IP your PC might have.)
// Your PC will have to be configured to share its Internet connection over the
// SLIP interface for this to work.
//#define GATEWAY_IP 192,168,5,1

//Enable Input Disturbance(e.g. Pole Place example)
//#define INPUT_DIST

//Enable Fast Mode(e.g. Dynamic Quantizer example)
//#define FAST_MODE

//Enable Logarithmic DeQuantizer
//#define LOG_DEQ

#ifdef LOG_DEQ
float Log_Dequantizer[16]={-8.0,-4.0,-2.0,-1.0,-0.5,-0.25,-0.125,-0.0,
                            0.0,0.125,0.25,0.5,1.0,2.0,4.0,8.0};
#endif

// Because some of the weird uIP declarations confuse the Arduino IDE, we
// need to manually declare a couple of functions.
void uip_callback(uip_tcp_appstate_t *s);

typedef struct {
  char input_buffer;
  char output_buffer[2];
} connection_data;

uip_ipaddr_t control_server;
struct uip_conn *conn;
#define SET_IP(var, ip)  uip_ipaddr(var, ip)

// State Variable
float V1, Vo;
// Input
float Vi;

// Convert ADC output to -2.5~2.5V
float convDac(float y){
  return (y - 511) / 1023.0 * 5.0;
}

// Convert Voltage to PWM duty ratio
int convPwm(float u){
  int ret = 255/5*u + 128;

  if (ret > 255)
    ret = 255;
  else if (ret < 0)
    ret = 0;

  return ret;
}

void setup() {

#ifndef FAST_MODE
  Serial.begin(115200);
#else
  Serial.begin(1000000);
#endif
  SerialIP.use_device(Serial);
  SerialIP.set_uip_callback(uip_callback);
  SerialIP.begin({MY_IP}, {MY_SUBNET});
  SET_IP(control_server, CONTROL_SERVER);
#ifdef GATEWAY_IP
  SerialIP.set_gateway({GATEWAY_IP});
#endif
#ifndef FAST_MODE
  Timer1.initialize(100000);
#else
  Timer1.initialize(50000);
#endif
  Timer1.attachInterrupt(open_port);

  Timer1.start();
}

char quantizer(float sig){
    return char(sig*Q_GAIN);
  }

float dequantizer(char sig){
    return float(sig/Q_GAIN);
  }

void open_port() {
  
  if(!uip_connected()){
    conn = uip_connect(&control_server, HTONS(8000));
    if (conn == NULL) {
      return;
    }
  }
  
}

void loop() {
  // Check the serial port and process any incoming data.
  SerialIP.tick();
}

void uip_callback(uip_tcp_appstate_t *s)
{
  if (uip_connected()) {
    connection_data *d = (connection_data *)malloc(sizeof(connection_data));
    s->user = d;
    V1 = convDac(analogRead(OUTPUT_PIN_1));
    Vo = convDac(analogRead(OUTPUT_PIN_2));
    d->output_buffer[0]=quantizer(Vo);
    d->output_buffer[1]=quantizer(V1);
    PSOCK_INIT(&s->p, (uint8_t *)(&d->input_buffer),sizeof(&d->input_buffer));
  }

    // Call/resume our protosocket handler.
  handle_connection(s,(connection_data *)s->user);
  // If the connection has been closed, release the data we allocated earlier.
  if (uip_closed()) {
    if (s->user) free(s->user);
    s->user = NULL;
  }
}

int handle_connection(uip_tcp_appstate_t *s,connection_data *d)
{

  PSOCK_BEGIN(&s->p);
  // Send plant data over the connection.
  PSOCK_SEND(&s->p,d->output_buffer,sizeof(d->output_buffer));

  // Read control input into the input buffer we set in PSOCK_INIT.  Data
  // is read until the input buffer gets filled up.
  PSOCK_READBUF(&s->p);
#ifndef LOG_DEQ
  Vi=dequantizer(d->input_buffer);
#else
  Vi=Log_Dequantizer[d->input_buffer]/2.8;
#endif
#ifndef INPUT_DIST  
  analogWrite(INPUT_PIN, convPwm(Vi));
#else
  analogWrite(INPUT_PIN, convPwm(Vi+random(-20,21)/10.0));
#endif
  // Disconnect.
  PSOCK_CLOSE(&s->p);

  // All protosockets must end with this macro.  It closes the switch().
  PSOCK_END(&s->p);
}
