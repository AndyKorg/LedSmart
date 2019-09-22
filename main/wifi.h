/*
 * wifi.h
 *
 *  Created on: 2 июн. 2019 г.
 *      Author: Administrator
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include <string.h>
#include <stddef.h>
#include "esp_wifi_types.h"

#define AP_SSID_WATERCOUNT		"smartLamp"
#define AP_PASS_WATERCOUNT		""			//Если пусто, то без пароля
#define OTA_CHECK_PERIOD_MIN	(24*60)		//Period checked update application, minute

extern wifi_sta_config_t wifi_sta_param;

void wifi_init_param(void);					//Прочитать параметры wifi
void wifi_init(wifi_mode_t mode);
bool wifi_isOn();							//wifi включен
bool wifi_AP_isOn();						//AP включен, действительно тольео если wifi_isOn() != 0
bool wifi_ap_count_client();				//количество клиентов подключенных к ap


#endif /* MAIN_WIFI_H_ */
