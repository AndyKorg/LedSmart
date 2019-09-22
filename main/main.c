#include <string.h>
#include <esp_system.h>
#include <rom/ets_sys.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "sdkconfig.h"
#include "wifi.h"
#include "cayenne.h"
#include "ota_client.h"
#include "params.h"

#undef __ESP_FILE__
#define __ESP_FILE__	NULL

static const char *TAG = "LED";

#define REED_SWITCH		GPIO_Pin_4
#define REED_SWITCH_NUM	GPIO_NUM_4

#define DOOR_CLOSE		0
#define DOOR_OPEN		1
#define door_state()	(gpio_get_level(REED_SWITCH_NUM))
#define doorIsOpen()	(door_state() == DOOR_OPEN)
#define doorIsClose()	(door_state() == DOOR_CLOSE)

#define AP_SWITCH		GPIO_Pin_12	//Soft AP go on
#define AP_SWITCH_NUM	GPIO_NUM_12
#define AP_ON			0
#define AP_OFF			1
#define AP_state()		(gpio_get_level(AP_SWITCH_NUM))
#define AP_IsOn()		(AP_state() == AP_ON)
#define AP_IsOff()		(AP_state() == AP_OFF)

#define LED				GPIO_Pin_5
#define LED_NUM			GPIO_NUM_5
#define LED_ON			0
#define LED_OFF			1
#define LedSwitch(x, y, state)	do{ gpio_set_level(LED_NUM, x); y = state;} while (0)
#define LedOn(x)		LedSwitch(LED_ON, x, true)
#define LedOff(x)		LedSwitch(LED_OFF, x, false)

#define LED_PERIOD_S	60

xSemaphoreHandle ledState;
TaskHandle_t ledHandleTask;

void led_send_status(bool led_on){
	static bool prevLedState;
	if (led_on != prevLedState){
		CayenneChangeInteger(&cayenn_cfg, PARAM_CHANAL_LED_STATE, PARAM_NAME_LED_STATE, led_on);
		CayenneUpdateActuator(&cayenn_cfg, PARAM_CHANAL_LED_UPDATE, led_on);
		prevLedState = led_on;
	}
}

void led_control(void *pvParameters){

	int startDelay = 0;
	const TickType_t 	ledPeriodOn = (LED_PERIOD_S*1000)/portTICK_PERIOD_MS,
						ledPeiodAP = (250/portTICK_PERIOD_MS);
	int	signal_off = 0;
	bool ledIsOn = 0;

	while(1){
		if (wifi_AP_isOn()){
			LedOn(ledIsOn);
			vTaskDelay(ledPeiodAP);
			LedOff(ledIsOn);
			vTaskDelay(ledPeiodAP);
			ESP_LOGI(TAG, "AP led");
		}
		else{
			if (xSemaphoreTake(ledState, 0) == pdTRUE){//no white
				startDelay++;
			}
			if (startDelay){
				startDelay--;
				LedOn(ledIsOn);
				led_send_status(ledIsOn);
				signal_off = 0;
				ESP_LOGI(TAG, "On led door");
				vTaskDelay(ledPeriodOn);
			}
			else{
				if (signal_off == 0){
					signal_off = 1;
					ESP_LOGI(TAG, "Led door off");
				}
				LedOff(ledIsOn);
				led_send_status(ledIsOn);
			}
		}
	}
}

void door_sensor_control(void *pvParameters){

	int doorStatePrev = DOOR_CLOSE, tmp;

	const TickType_t debounce = 250/portTICK_PERIOD_MS;

	while(1){
		if (doorStatePrev != door_state()){
			tmp = door_state();
			vTaskDelay(debounce);
			if (tmp == door_state()){
				doorStatePrev = door_state();
				xSemaphoreGive(ledState);
				CayenneChangeInteger(&cayenn_cfg, PARAM_CHANAL_CAYEN, PARAM_NAME_SENSOR, doorStatePrev);
			    ESP_LOGI(TAG, "change door state");
			}
		}
	}
}

void wifi_mode_control(void *pvParameters){
	wifi_mode_t currentMode = WIFI_MODE_NULL;

	while(1){
		if (AP_IsOn()){
			if ( !((currentMode == WIFI_MODE_AP) || (currentMode == WIFI_MODE_APSTA)) ){
				currentMode = WIFI_MODE_AP;
				if (currentMode == WIFI_MODE_STA){
					currentMode = WIFI_MODE_APSTA;
				}
				wifi_init(currentMode);

				ESP_LOGI(TAG, "AP on");
			}
		}
		else{
			if (currentMode != WIFI_MODE_STA){
				wifi_init(WIFI_MODE_STA);
				currentMode = WIFI_MODE_STA;
			    ESP_LOGI(TAG, "STA on");
			}
		}
	}
}

esp_err_t recivLed(int *value){
	if (*value){
		xSemaphoreGive(ledState);
	}
	return ESP_OK;
}

void app_main()
{


    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //sensors configure
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (REED_SWITCH | AP_SWITCH);
    io_conf.mode =	GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    //led configure
    io_conf.pin_bit_mask = (LED);
    io_conf.mode =	GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    uint8_t tmp;
    LedOff(tmp);

    ota_init();
    wifi_init_param();
    Cayenne_Init();

    ledState = xSemaphoreCreateBinary();
	xTaskCreate(wifi_mode_control, "mode AP", 4096, ( void * ) 1, 0, NULL);
    if (ledState != NULL){
    	if (xTaskCreate(door_sensor_control, "door_control", 4096, ( void * ) 1, 0, NULL) == pdPASS){
    		xTaskCreate(led_control, "led_control", 4096, (void *) 1, 0, &ledHandleTask);
    		cayenne_reg(PARAM_CHANAL_LED_STATE, recivLed);
    	}
    }
}
