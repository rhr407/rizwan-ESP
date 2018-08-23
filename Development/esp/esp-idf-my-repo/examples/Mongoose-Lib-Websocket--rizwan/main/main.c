#include "main.h"


static struct mg_connection *nc_for_led = NULL;
void *p_for_led = NULL;

static int led_var = 1;
static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {

            led_var = !led_var;

            gpio_set_level(LED_GPIO, led_var);
/*=======================================================================*/
            if(p_for_led !=NULL){
              struct websocket_message *wm = (struct websocket_message *) p_for_led;
              /* New websocket message. Tell everybody. */
              struct mg_str d = {(char *) wm->data, wm->size};

              if(led_var){
                d.p = "LED is ON";
                d.len = 9;
              }
              else
                if(!led_var){
                  d.p = "LED is OFF";   
                  d.len = 10;
              }
              if(nc_for_led != NULL) update_status_on_web(nc_for_led, d);
            }
/*=======================================================================*/            
            
        }
    }
}




static esp_err_t event_handler(void *ctx, system_event_t *event) {
  (void) ctx;
  (void) event;
  return ESP_OK;
}


static void broadcast(struct mg_connection *nc, const struct mg_str msg) {
  struct mg_connection *c;
  char buf[500];
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.len, msg.p);
  printf("%s\n", buf); /* Local echo. */
  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (c == nc) continue; /* Don't send to the sender. */
    mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, strlen(buf));
  }
}

static void update_status_on_web(struct mg_connection *nc, const struct mg_str msg) {
  char buf[500];
  char addr[32];

  

  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.len, msg.p);


  if(msg.len == 9 ) buf[0] = '1';
  else if(msg.len == 10) buf[0] = '0';

  printf("%s\n", buf); /* Local echo. */
    mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, 1);
}

static void mg_ev_handler(struct mg_connection *nc, int ev, void *p) {
  static const char *reply_fmt =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "Hello %s\n";

  char buf[500];
  char addr[32];

  p_for_led = p;

  switch (ev) {
    case MG_EV_ACCEPT: {

      broadcast(nc, mg_mk_str("++ joined"));
      nc_for_led = nc;

      break;
    }

    case MG_EV_WEBSOCKET_FRAME: {
      struct websocket_message *wm = (struct websocket_message *) p;
      /* New websocket message. Tell everybody. */
      struct mg_str d = {(char *) wm->data, wm->size};

      gpio_set_level(LED_GPIO, 1); 

      snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) d.len, d.p);

      

      if(d.p[0] == '1'){  
        gpio_set_level(LED_GPIO, 1);       
        d.p = "LED is ON";
        d.len = 9;
      }
      else
        if(d.p[0] == '0'){
        gpio_set_level(LED_GPIO, 0);  
        d.p = "LED is OFF";   
        d.len = 10;
      }

      update_status_on_web(nc, d);

      // broadcast(nc, d);
      break;
    }

    case MG_EV_HTTP_REQUEST: {
      char addr[32];
      struct http_message *hm = (struct http_message *) p;
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
             hm->method.p, (int) hm->uri.len, hm->uri.p);
      mg_printf(nc, reply_fmt, addr);
      nc->flags |= MG_F_SEND_AND_CLOSE;
      break;
    }
    case MG_EV_CLOSE: {
      printf("Connection %p closed\n", nc);
      break;
    }
  }
}

void app_main(void) {
  nvs_flash_init();
  tcpip_adapter_init();
/*============================================================*/
  gpio_pad_select_gpio(LED_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  gpio_set_level(LED_GPIO, 1);
/*============================================================*/
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  /* Initializing WiFi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config = {
      .sta = {.ssid = WIFI_SSID, .password = WIFI_PASS, .bssid_set = false}};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());

  /* Starting Mongoose */
  struct mg_mgr mgr;
  struct mg_connection *nc;

  printf("Starting web-server on port %s\n", MG_LISTEN_ADDR);

  mg_mgr_init(&mgr, NULL);

  nc = mg_bind(&mgr, MG_LISTEN_ADDR, mg_ev_handler);
  if (nc == NULL) {
    printf("Error setting up listener!\n");
    return;
  }
  mg_set_protocol_http_websocket(nc);
/*===========================================================*/

    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;

    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;

    //bit mask of the pins that you want to set,e.g.GPIO2 for LED
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;

    //disable pull-down mode
    io_conf.pull_down_en = 0;

    //disable pull-up mode
    io_conf.pull_up_en = 0;

    //configure GPIO with the given settings
    gpio_config(&io_conf);
/*===========================================================*/
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;

    //bit mask of the pins, use GPIO0 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;

    //disable pull-up mode
    io_conf.pull_up_en = 0;

    gpio_config(&io_conf);
/*===========================================================*/
    //change gpio intrrupt type for one pin
    gpio_set_intr_type(BUTTON_GPIO, GPIO_INTR_POSEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);

  /* Processing events */
  while (1) {
    mg_mgr_poll(&mgr, 1000);
  }
}
