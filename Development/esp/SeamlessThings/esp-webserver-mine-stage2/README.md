This is a Mongoose webserver pubsub example that can be compiled under Espressif IoT Development Framework for ESP32.

The page ESP32_WS.html demonstrates how Mongoose could be used to implement publish–subscribe pattern by controlling the onboard blue 
LED on GPIO2 of ESP32 on DOIT ESP32 V1 Kit. The blue LED switching is also being controlled by an onboard BOOT button which 
also updates the status of LED on this webpage. Please refer to the source/main.h file to configure WiFi credentials and values of GPIOs 
for LED and button for your own ESP32 kit.


For building the example, you need to have [Docker](https://www.docker.com/products/docker) and use our pre-built SDK container.
To build just run in the example directory
```
$ make
```

Once built, use make flash for flashing and displaying the output on terminal
```
$ make flash monitor
```
<!-- ---------------------------------------------------------- -->
Rizwan comments on cliq 

Please unzip this file inside esp-idf/examples. Before launching
'make' command please enter Wifi and GPIO credentials in main.h
file inside source folder within this project.

among the three html files 'ESP32_WS.html' must be used

<!-- ---------------------------------------------------------- -->

I added to get it to build in our idf with our components

EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/components/

## Changes 

You will need to ```make menuconfig``` and change the partition table to come from CSV file in the root of the proj. 

--- 
### 1. Use mkspiffs and spiffs_image components to create a spiffs image

Use the mkspiffs and spiffs_image components to create a spiffs image containing your index.html file. 


Modify the html to default connect to itself and remove the connect to other WS address concept. 

Your html file has been moved to esp-webserver/components/spiffs_image/image/index.html. Any other assets needed by your html will also need to be copied into esp-webserver/components/spiffs_image/image/.

To flash this file onto the device ```make flashfs``` this will create a spiffs image of your index.html and flash it to the partition specified. 
    
or just  ```make flash flashfs  ```


### 2. Update the Websocket code inside catch HTTP requests and then to serve the index.html from the spiffs filesystem. 

##### Example code to help you get started  


    #include "spiffs_vfs.h"

    // Extra options for mongoose
    static struct mg_serve_http_opts s_opts;
	memset(&s_opts, 0, sizeof(s_opts));
	s_opts.document_root= "/spiffs/";
	s_opts.index_files = "index.html";
   
    // To catch the HTTP_REQUEST
    case MG_EV_HTTP_REQUEST: {
        mg_serve_http(nc, hm, s_opts);
        break;
    }
    
    // where you init your project comes from spiffs component
    vfs_spiffs_register();	
   	if (spiffs_is_mounted) { ESP_LOGI(TAG, "==== spiffs mounted .... ====\r\n"); }

---- 
### 3. Use the fw_upgrade module to take websocket message containing the firmware URL and use the http_firmware_upgrade function 

Here you will need to modify your javascript to send and recieve websocket JSON messages. You will need to implement a JSON parser on the ESP to extract the URL. 


##### Example code to help get you started  

    #include "http-firmware-upgrade.h"

    // This should come from websocket message from Index.html form.  
    const char *url = "http://192.168.1.150:8080/websocket.bin"; 

    ESP_LOGI(TAG, "Fetching FW image from %s", url);
    
	int fw_upgrade = http_firmware_upgrade(url, NULL);

    // if (fw_upgrade == ESP_OK) {
    //     ESP_LOGI(TAG, "FW Upgrade Successful");
    //     // fw_upgrade_status = FW_UPG_STATUS_SUCCESS;
    // } else {
    //     ESP_LOGE(TAG, "FW Upgrade Failed");
        // fw_upgrade_status = FW_UPG_STATUS_FAIL;
    // }
    ESP_LOGI(TAG, "Prepare to restart system!");
    // esp_restart();

---

1. Status of Firmware Progress Message could be downloading/installing/done
2. The __comment tags in the messages are to be removed
3. firmware progress message should be sent 1 per second or less
4. probably should add a “reboot” flags to the update-firmware call and an implicit message/reposonse for report

##### Example JSON message structure 

<!-- To ESP	 -->
	{
		    "__comment": "Request Update Firmware"

		
		    "device": "garage",
		    "message": {
		      "type": "update-firmware",
		      "content": {
		            "firmware-url": "http://s3.aws.com/garage-1.2.4.img",
		            "reboot" : "False"
		        }
		     }
		}
		

		
		{
		    "__comment": "Reboot Request"
		    "device": "garage",
		    "message": {
		      "type": "reboot-request",
		      "content": {
		            "delay" : 5
		        }
		     }
		}
<!-- From ESP -->

		{
		    "__comment": "Firmware Progress"
		    "device": "garage",
		    "message": {
		      "type": "firmware-status",
		      "content": {
		            "status" : "downloading firmware",
		            "percentage" : 12
		        }
		     }
		}
		{
		    "__comment": "Reboot Response"
		    "device": "garage",
		    "message": {
		      "type": "reboot-response",
		      "content": {
		            "message" : "device is rebooting"
		        }
		     }
		}



