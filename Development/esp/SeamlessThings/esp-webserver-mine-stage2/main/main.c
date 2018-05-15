#include "freertos/FreeRTOS.h"
#include "freertos/task.h"      
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "spiffs_vfs.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include <sys/fcntl.h>
#include "string.h"
#include "mongoose.h"
#include "freertos/queue.h"
#include "http-firmware-upgrade.h"
#include "json.h"




#define GPIO_OUTPUT_PIN_SEL     (1ULL<<CONFIG_LED_GPIO)
#define GPIO_INPUT_PIN_SEL      (1ULL<<CONFIG_BUTTON_GPIO)


static const char *TAG = "esp-webserver:";

static struct mg_connection *nc_for_led = NULL;
void *p_for_led = NULL;

static int led_var = 1;
static xQueueHandle gpio_evt_queue = NULL;


static void update_status_on_web(struct mg_connection *nc, const struct mg_str msg) {
  char buf[500];
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.len, msg.p);


  if(msg.len == 9 ) buf[0] = '1';
  else if(msg.len == 10) buf[0] = '0';

  ESP_LOGI(TAG, "Sent Data %s\n" , buf);
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, buf, 1);
}

static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}

static void broadcast(struct mg_connection *nc, const struct mg_str msg) {
  struct mg_connection *c;
  char buf[500];
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  snprintf(buf, sizeof(buf), "%s %.*s", addr, (int) msg.len, msg.p);
 	ESP_LOGI(TAG, "Buf %s\n" , buf);
  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
  if (c == nc) continue; /* Don't send to the sender. */
     mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, strlen(buf));
  }
}
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

            gpio_set_level(CONFIG_LED_GPIO, led_var);
/*----------------------------------------------------------------------------*/
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
/*----------------------------------------------------------------------------*/            
            
        }
    }
}

static void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData)
{
	char buf[500];
    char addr[32];

	struct http_message *hm = (struct http_message *) evData;
    static struct mg_serve_http_opts s_opts;

	memset(&s_opts, 0, sizeof(s_opts));
	s_opts.document_root= "/spiffs/";
	s_opts.index_files = "index.html, LED_is_ON.jpg, LED_is_OFF.jpg";

    // p_for_led = evData;

	switch (ev) {

    case MG_EV_ACCEPT: 
    {
        broadcast(nc, mg_mk_str("++ joined"));
        nc_for_led = nc;
        break;
    }

	case MG_EV_HTTP_REQUEST: 
    {
    	mg_serve_http(nc, hm, s_opts);

    	break;
	}

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: 
    {
		if (is_websocket(nc)) 
        {   
            nc_for_led = nc;
			// ESP_LOGI(TAG, "After Serve %s\r\n", nc);
		}

		break;
    }

	case MG_EV_WEBSOCKET_FRAME: 
    {
        struct websocket_message *wm = (struct websocket_message *) evData;
        struct mg_str d = {(char *) wm->data, wm->size};


        mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), 
                            MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

        snprintf(buf, sizeof(buf), "%s %.*s\n", addr, (int) d.len, d.p);

        ESP_LOGI(TAG, "Received Data: %s\n" , d.p);

        for (int iii = 0; iii < 100; ++iii)
        {
            printf("%c", d.p[iii]);
        }

        printf("\n");
      

/*----------------------- Work in Progress -----------------------------------*/

        // const char json[] = "{\"a\" : true, \"b\" : [false, null, \"foo\"]}";
        // struct json_value_s* root = json_parse(json, strlen(json));
        // /* root->type is json_type_object */

        // struct json_object_s* obj = (struct json_object_s*)root->payload;
        // printf("size of object is: %d\n", object->length);
        // objNum = obj->start;

        // for (int i = 0; i < obj->length; ++i)
        // {
        //     obj_dig_down(obj, objNum);
        //     objNum = objNum->next;
        // }

        
        // free(root);


/*----------------------------------------------------------------------------*/


  
      if(d.p[0] == '1'){  

     /*---------------------------------------------------------------------*/
      This should come from websocket message from index.html form.  
      const char *url = "http://192.168.43.197:8000/esp-webserver.bin"; 

      ESP_LOGI(TAG, "Fetching FW image from %s", url);
      
      int fw_upgrade = http_firmware_upgrade(url, NULL);

      if (fw_upgrade == ESP_OK) {
          ESP_LOGI(TAG, "FW Upgrade Successful");
          // fw_upgrade_status = FW_UPG_STATUS_SUCCESS;
      } else {
          ESP_LOGE(TAG, "FW Upgrade Failed");
          // fw_upgrade_status = FW_UPG_STATUS_FAIL;
      }
      ESP_LOGI(TAG, "Prepare to restart system!");
      esp_restart();
/*---------------------------------------------------------------------*/


        gpio_set_level(CONFIG_LED_GPIO, 1);       
        d.p = "LED is ON";
        d.len = 9;
      }
      else
        if(d.p[0] == '0'){
        gpio_set_level(CONFIG_LED_GPIO, 0);  
        d.p = "LED is OFF";   
        d.len = 10;
		}

      update_status_on_web(nc, d);

      // broadcast(nc, d);
      break;
    }

	    case MG_EV_CLOSE: 
        {
	      /* Disconnect. Tell everybody. */
	      if (is_websocket(nc)) {
	        broadcast(nc, mg_mk_str("-- left"));
	      }
	      break;
	    }

	}
}

/*----------------------- Work in Progress -----------------------------------*/


// void obj_dig_down(struct json_object_s* object, 
//                   struct json_object_element_s* objNum)
// {
//     struct json_string_s* ele_name = objNum->name;
//     /* a_name->string is "a" */
//     /* a_name->string_size is strlen("a") */

//     struct json_value_s* ele1_value = a->value;
//     /* a_value->type is json_type_true */
//     /* a_value->payload is NULL */

//     struct json_object_element_s* b = a->next;
//     /* b->next is NULL */

//     struct json_string_s* b_name = b->name;
//     /* b_name->string is "b" */
//     /* b_name->string_size is strlen("b") */

//     struct json_value_s* b_value = b->value;
//     /* b_value->type is json_type_array */

//     struct json_array_s* array = (struct json_array_s*)b_value->payload;
//     /* array->length is 3 */

//     struct json_array_element_s* b_1st = array->start;

//     struct json_value_s* b_1st_value = b_1st->value;
//     /* b_1st_value->type is json_type_false */
//     /* b_1st_value->payload is NULL */

//     struct json_array_element_s* b_2nd = b_1st->next;

//     struct json_value_s* b_2nd_value = b_2nd->value;
//     /* b_2nd_value->type is json_type_null */
//     /* b_2nd_value->payload is NULL */

//     struct json_array_element_s* b_3rd = b_2nd->next;
//     /* b_3rd->next is NULL */

//     struct json_value_s* b_3rd_value = b_3rd->value;
//     /* b_3rd_value->type is json_type_string */

//     struct json_string_s* string = (struct json_string_s*)b_3rd_value->payload;
//     /* string->string is "foo" */
//     /* string->string_size is strlen("foo") */

//     /* Don't forget to free the one allocation! */
// }

void mongooseTask(void *data)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);
  
  struct mg_mgr mgr;
	mg_mgr_init(&mgr, NULL);

	struct mg_connection *nc = mg_bind(&mgr, CONFIG_MG_LISTEN_ADDR, mongoose_event_handler);
	if (nc == NULL) {
		printf( "No connection from the mg_bind()\n");
		vTaskDelete(NULL);
		return;
	}

	mg_set_protocol_http_websocket(nc);
	while (1) {
		mg_mgr_poll(&mgr, 200);
	}
	mg_mgr_free(&mgr);
}

esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id){
		case SYSTEM_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Wireless connected\n");
			break;

		case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Wireless disconnected\n");
		  break;

		case SYSTEM_EVENT_STA_START:
      ESP_LOGI(TAG, "Wireless started\n");
			break;

		case SYSTEM_EVENT_STA_STOP:
      ESP_LOGI(TAG, "Wireless stoped\n");
			break;

		case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "Wireless Device got IP\n");
			xTaskCreate(mongooseTask, "mongooseTask", 20000, NULL, 5, NULL);
      ESP_LOGI(TAG, "Mongoos Server Started\n");
			break;

		default:
			break;
	}
	return ESP_OK;
}



void init_gpio(void){

  gpio_pad_select_gpio(CONFIG_LED_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(CONFIG_LED_GPIO, GPIO_MODE_OUTPUT);

  gpio_set_level(CONFIG_LED_GPIO, 1);

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

  //interrupt of rising edge
  io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;

  //bit mask of the pins, use GPIO0 here
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;

  //set as input mode    
  io_conf.mode = GPIO_MODE_INPUT;

  //disable pull-up mode
  io_conf.pull_up_en = 0;

  gpio_config(&io_conf);

  //change gpio intrrupt type for one pin
  gpio_set_intr_type(CONFIG_BUTTON_GPIO, GPIO_INTR_POSEDGE);

  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  //start gpio task
  xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

  //install gpio isr service
  gpio_install_isr_service(CONFIG_ESP_INTR_FLAG_DEFAULT);

  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handler, (void*) CONFIG_BUTTON_GPIO);
}

static void init_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

    wifi_config_t sta_config = {
      .sta = {.ssid = CONFIG_WIFI_SSID , .password = CONFIG_WIFI_PASSWORD, .bssid_set = false}};

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );

    ESP_ERROR_CHECK( esp_wifi_start() );
	  ESP_ERROR_CHECK(esp_wifi_connect());

}






void app_main(void)
{
    nvs_flash_init();
    vfs_spiffs_register();
   	if (spiffs_is_mounted) { ESP_LOGI(TAG, "Spiffs /spiffs mounted\n");}

   	init_wifi();
 	init_gpio();


}

