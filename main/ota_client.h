/*
 * WARNING! Without checked correct new application!
 *
 */

#ifndef MAIN_OTA_CLIENT_H_
#define MAIN_OTA_CLIENT_H_

#include "/esp8266/esp8266_rtos_sdk/components/lwip/lwip/src/include/lwip/ip4_addr.h"

#define DOWNLOAD_FILENAME		"ledsmart"	//without extension

typedef struct {
    char server_ip[IP4ADDR_STRLEN_MAX+1];
} ota_param_t;

extern ota_param_t 	ota_param;

void ota_check(void);
void ota_init(void);

#endif /* MAIN_OTA_CLIENT_H_ */
