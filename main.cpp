#include "DigitalIn.h"
#include "DigitalOut.h"
#include "InterfaceDigitalIn.h"
#include "Thread.h"
#include "Ticker.h"
#include "mbed.h"
#include "Adafruit_SSD1306.h"
#include "mbed_thread.h"
#include <cstdio>
#include <cstring>


DigitalIn button(PA_8);

I2C display_I2C(PB_9,PB_8);
Adafruit_SSD1306_I2c display(display_I2C,PA_1);


static BufferedSerial wifi_port(PB_6, PB_7);
#define MAXIMUM_BUFFER_SIZE             32
char buf[MAXIMUM_BUFFER_SIZE] = {0};

Ticker update_display_ticker;

Thread sensor_update_thread;
Thread read_UART_thread;
Thread check_poliv_signal_thread;
Thread check_button_signal_thread;

DigitalOut led(PC_13);
AnalogIn water_sensor(PA_0);
DigitalIn poliv_signal(PB_0);
DigitalOut relay(PB_12);

volatile int level;
volatile int delay_for_poliv;

int read_water_sensor(){
    float x = water_sensor.read();
    int y = x*100;
    return y;
}

void send_water_sensor_level(int level){
    char level_char[3];
    sprintf(level_char, "%d",level);
    wifi_port.write(level_char, sizeof(level_char));
    return;
}


void poliv(){
    wifi_port.write("poliv", sizeof("poliv"));
    relay=1;
    thread_sleep_for(delay_for_poliv);
    relay=0;
    return;
}

// Если пользователь нажмет на кнопку, то будет выполнен полив
void button_click(){
    while (true) {
        if (button != true) {
            poliv();
            thread_sleep_for(1000);
        }
    }
}

void check_poliv_signal(){
    while (true) {
        led = !poliv_signal;
        if (poliv_signal == false) relay = 0;
    }
}


void update_display_data(){
    level = read_water_sensor();
    display.clearDisplay();
    display.setTextCursor(0, 0);
    display.printf("Water sensor: %d\r\n",level);
    display.printf("Status %s\r\n", poliv_signal.read() ? "true" : "false");
    display.printf("Time for poliv: %d\r\n", delay_for_poliv/1000);
    display.display();
    display.display();
}

void check_for_poliv(){
    if (poliv_signal.read() && level > 32){
        for (int i=0; i<5; i++){
            poliv();
            thread_sleep_for(1000);
            int after_poliv_sensor_level = read_water_sensor();
            send_water_sensor_level(after_poliv_sensor_level);
            if (after_poliv_sensor_level>level) break;
            level = after_poliv_sensor_level;
            if (after_poliv_sensor_level<=28) break;
            if (poliv_signal.read()==0) break;
        }
    }
}

void transmit_sensor_data(){
    while(true){
        level = read_water_sensor();
        send_water_sensor_level(level);
        check_for_poliv();
        thread_sleep_for(120000);
    }
}

void read_UART(){
    while (true) {
        thread_sleep_for(1000);
        if (wifi_port.readable()){
            int length = wifi_port.read(buf, sizeof(buf));
            printf("length = %d, buf = %s\n\r", length, buf);
            if (strcmp(buf, "Do\n\r")==0) {
                if(poliv_signal.read()) {
                    poliv();
                    send_water_sensor_level(read_water_sensor());
                }
            } else {
                if (length>3) {
                    char subbuf[3];
                    memcpy(subbuf, &buf[0], 3);
                    if (strcmp(subbuf, "Set")==0){
                        memcpy(subbuf, &buf[4], 2);
                        subbuf[3]='\0';
                        if (subbuf[2]=='\n') subbuf[2]='\0';
                        delay_for_poliv = atoi(subbuf)*1000;
                    }
                }
                
            }
            memset(buf, 0, sizeof(buf));
        }
    }
}

int main()
{
    led = 1;
    delay_for_poliv = 5000;
    wifi_port.set_baud(115200);
    wifi_port.set_blocking(false);
    thread_sleep_for(5000);

    
    check_button_signal_thread.start(&button_click);
    sensor_update_thread.start(&transmit_sensor_data);

    //update_display_ticker.attach(&update_display_data,1s);
    read_UART_thread.start(read_UART);
    check_poliv_signal_thread.start(check_poliv_signal);
    //display.setTextColor(0x07E0);
    //display.printf("%ux%u OLED Display\r\n", display.width(), display.height());
    
    while (true) {
        update_display_data();
        thread_sleep_for(30000);  
    }
}

