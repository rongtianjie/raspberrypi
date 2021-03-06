#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include <bcm2835.h>

#include "get_weather.h"
#include "system_usage.h"
#include "dht22.h"
#include "12864_display.h"
#include "srv_func.h"

#define TRY_FUNC(x)                             \
    {                                           \
        if ((x) < 0) {                          \
            fprintf(stderr, "%s failed\n", #x); \
            return -1;                          \
        }                                       \
    }

struct tm  *tm;

static int gpio_init(void)
{
    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init() failed\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    char        print_buf[256] = { 0 };
    char       *weeks[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    time_t      timevalue;
    pthread_t   temp_hum_pt, system_usage_pt, weather_pt, srv_pt;
    pthread_attr_t attr;
    char        roll_weather[512];
    int         off = 0;
    char       *curr_dis = roll_weather + 6;
    char       *curr_wea = weather;

    if (argc >= 2 && !strcmp(argv[1], "-d")) {
        daemon(1, 0);
    }

    if (gpio_init() < 0) {
        return -1;
    }

    timevalue = time(NULL);
    tm = localtime(&timevalue);
    
    lcd_init();

    TRY_FUNC(pthread_attr_init(&attr));

    TRY_FUNC(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));

    TRY_FUNC(pthread_attr_setstacksize(&attr, 1024 * 1024)); 

    TRY_FUNC(pthread_create(&temp_hum_pt, &attr, temp_hum_func, NULL));

    TRY_FUNC(pthread_create(&system_usage_pt, &attr, system_usage_func, NULL));

    TRY_FUNC(pthread_create(&weather_pt, &attr, get_weather_func, NULL));

    TRY_FUNC(pthread_create(&srv_pt, &attr, srv_func, NULL));

    TRY_FUNC(pthread_attr_destroy(&attr));

    //strcpy(weather, "阴转小");    //测试用

    while (1) {
        timevalue = time(NULL);
        tm = localtime(&timevalue);

        if (tm->tm_hour >= 22 || tm->tm_hour < 7) {
            lcd_light_ctl(0);

        } else {
            lcd_light_ctl(1);
        }

        memset(roll_weather, 0x20, sizeof(roll_weather));
        if (strlen(weather) / 3 >= 4) {
            if (curr_dis > roll_weather) {              // 未显示完全, 前面的空格必须是偶数个
                strcpy(curr_dis, curr_wea);
                curr_dis -= 2;

            } else {                                    // 显示完全
                strcpy(roll_weather, curr_wea);
                off = strlen(roll_weather);
                if (off < 12) {
                    strcpy(curr_dis + off, "    ");
                }
                curr_wea += 3;
                if (*curr_wea == '\0') {
                    curr_dis = roll_weather + 6;
                    curr_wea = weather;
                }
            }

        } else {
            curr_dis = roll_weather + 6;
            curr_wea = weather;
            
            off = strlen(weather);
            
            strcpy(roll_weather + (4 - off / 3) * 2, weather);
        }

        sprintf(print_buf, 
                "[%04d/%02d/%02d %s]\n"
                "%02d:%02d:%02d%s\n"
                "T/H%5.1lfC/%4.1lf%%%c\n"
                "C/M%5.1lf%%/%4.1lf%%%c\n", 
                tm->tm_year + 1900, tm->tm_mon + 1, 
                tm->tm_mday, weeks[tm->tm_wday], 
                tm->tm_hour, tm->tm_min, tm->tm_sec, 
                roll_weather, temperature, humidity, 
                temp_hum_stat, cpu_usage, mem_usage, weather_stat);
        lcd_print(print_buf);

        // fprintf(stderr, "%s", print_buf);    //DEBUG INFO
        sleep(1);
    }

    bcm2835_close();
    return 0;
}

