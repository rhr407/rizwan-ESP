#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "mongoose.h"

#include <stdio.h>
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


#include <stdlib.h>
#include "freertos/queue.h"


#define MG_LISTEN_ADDR "9998"

/*=== User can change these values according to their DevKits*/  //Rizwan

#define WIFI_SSID "xx"
#define WIFI_PASS "xx"


#define LED_GPIO                2
#define BUTTON_GPIO             0
/*======================================================================*/



#define GPIO_OUTPUT_PIN_SEL     (1ULL<<LED_GPIO)
#define GPIO_INPUT_PIN_SEL      (1ULL<<BUTTON_GPIO)
#define ESP_INTR_FLAG_DEFAULT   0


static void update_status_on_web(struct mg_connection *nc, const struct mg_str msg);