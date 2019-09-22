/*
 * wifi.h
 *
 *  Created on: 2 ���. 2019 �.
 *      Author: Administrator
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include <string.h>
#include <stddef.h>
#include "esp_wifi_types.h"

#define AP_SSID_WATERCOUNT		"smartLamp"
#define AP_PASS_WATERCOUNT		""			//���� �����, �� ��� ������
#define OTA_CHECK_PERIOD_MIN	(24*60)		//Period checked update application, minute

extern wifi_sta_config_t wifi_sta_param;

void wifi_init_param(void);					//��������� ��������� wifi
void wifi_init(wifi_mode_t mode);
bool wifi_isOn();							//wifi �������
bool wifi_AP_isOn();						//AP �������, ������������� ������ ���� wifi_isOn() != 0
bool wifi_ap_count_client();				//���������� �������� ������������ � ap


#endif /* MAIN_WIFI_H_ */
