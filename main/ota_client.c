/*
 * ota_client.c
 *
 *  Created on: 31 авг. 2019 г.
 *      Author: Administrator
 */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "/esp8266/esp8266_rtos_sdk/components/lwip/lwip/src/include/lwip/sockets.h"
#include "/esp8266/esp8266_rtos_sdk/components/lwip/lwip/src/include/lwip/errno.h"

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "ota_client.h"
#include "esp_md5.h"

#include "version.h"
#include "utils.h"

static const char *TAG = "OTA:";

#define DOWNLOAD_SERVER_PORT	"80"
#define EXT_FILE_NAME_VER		"txt"		//Расширение для файла описания прошивки
#define EXT_FILE_NAME_BIN		"bin"		//для самой прошивки

#define BUFFSIZE 1500
#define TEXT_BUFFSIZE 1024
#define RESPONSE_VALUE_LEN_MAX	128	//Максимальная длина размера значения параметра заголовка
#define MD5_LEN					16	//Длина md5 хэша в байтах
#define MD5_STR_LEN				((MD5_LEN*2)+1)	//Длина md5 хэша в виде строки,плюс терминал

#define	HTTP_END_HEADER			"\r\n\r\n"

/*send GET request to http server*/
const char *GET_FORMAT =
    "GET /%s HTTP/1.0\r\n"
    "Host: %s:%s\r\n"
    "User-Agent: esp-idf/1.0 esp8266"HTTP_END_HEADER;

static int socket_id = -1;

static char	*response_ota;
static char *ota_write_data;
ota_param_t ota_param;

static bool connect_to_http_server(void)
{
    ESP_LOGI(TAG, "Server IP: %s Server Port:%s", ota_param.server_ip, DOWNLOAD_SERVER_PORT);

    int  http_connect_flag = -1;
    struct sockaddr_in sock_info;

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        return false;
    }

    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(ota_param.server_ip);
    sock_info.sin_port = htons(atoi(DOWNLOAD_SERVER_PORT));

    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        close(socket_id);
        return false;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        return true;
    }
    return false;
}

/*
 * Получить значение парамтера заголовка http
 * param - имя парметра, без завершающего двоеточия
 * value - сюда положит значение если фукнция вернет ERR_OK
 */
static esp_err_t parse_header(char *response, char *param, char *value, uint8_t lenValue){

    uint16_t lenResp;

	char* strResp = calloc(RESPONSE_VALUE_LEN_MAX, sizeof(char));
    char* strParamFormat = calloc(strlen(param)+5, sizeof(char));
    char *retPos;
    esp_err_t ret = ESP_ERR_INVALID_SIZE;
    if (strResp && strParamFormat){
    	strParamFormat = strcat(strParamFormat, param);
    	strParamFormat = strcat(strParamFormat, ":%s");
		do{
			sscanf(response, strParamFormat, strResp);
			lenResp = strlen(strResp);
			if (lenResp){
				if (lenResp > lenValue){
					break;
				}
				memset(value, 0, lenValue);
				strcpy(value, strResp);
				ret = ESP_OK;
				break;
			}
			memset(strResp, 0, RESPONSE_VALUE_LEN_MAX);
			retPos = strstr(response, "\r\n");
			if (retPos){
				response = retPos;
				response += 2;
			}
			else {
				ESP_LOGE(TAG, "no content");
				break;
			}
		}while (strlen(response)>0);
    }
    free(strResp);
    free(strParamFormat);
	return ret;

}

/*
 * Запрашивает файл и возвращает первый кусок файла если он найден
 */
static esp_err_t getFile(char *filename, char *response, uint32_t *lenResponse){

    esp_err_t ret = ESP_ERR_NOT_FOUND;

	char *http_request = NULL;
    int get_len = asprintf(&http_request, GET_FORMAT, filename, ota_param.server_ip, DOWNLOAD_SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");

    }
    ESP_LOGI(TAG, "request = %s", http_request);
    int res = send(socket_id, http_request, get_len, 0);
    free(http_request);

    if (res >= 0) {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
        *lenResponse = recv(socket_id, response, TEXT_BUFFSIZE, 0);
        if ( *lenResponse > 0){
        	if (*response == 'H'){
        		uint32_t resp;
                sscanf(response, "HTTP/1.1 %d  OK", &resp);
                if (resp == 200){
                	ret = ESP_OK;
            	}
            }
        }
    }
    return ret;
}

static uint8_t getFileNameVer(char **fileName,  char* typeFile){
    return asprintf(fileName, "%s.%s", DOWNLOAD_FILENAME, typeFile);
}

//Удаляю вручную т.к. esp_ota_begin падает на большом разделе
inline static esp_err_t clearPartition(const esp_partition_t *partition){

	uint32_t sectorCount = partition->size/SPI_FLASH_SEC_SIZE;
	uint32_t eraseCount;
	for(eraseCount = 0; eraseCount <sectorCount;eraseCount++){
		if (esp_partition_erase_range(partition, SPI_FLASH_SEC_SIZE * eraseCount, SPI_FLASH_SEC_SIZE) != ESP_OK){
			ESP_LOGE(TAG, "erase error adr = %x", SPI_FLASH_SEC_SIZE * eraseCount);
			return ESP_ERR_INVALID_STATE;
		}
		taskYIELD();//Обязательно надо дать другим поработать!
	}
	return ESP_OK;

}

static esp_err_t getUpdateFile(const esp_partition_t* partition, const uint8_t md5file[MD5_LEN]){
    esp_err_t ret = ESP_ERR_NOT_FOUND;

	if (!connect_to_http_server()) {
        ESP_LOGE(TAG, "Connect to http server failed!");
        close(socket_id);
        return ESP_ERR_NOT_FOUND;
    }

	response_ota = calloc(TEXT_BUFFSIZE+1, sizeof(char));
    ota_write_data = calloc(TEXT_BUFFSIZE+2, sizeof(char));
    char* paramValue = calloc(RESPONSE_VALUE_LEN_MAX, sizeof(char));
    char* fileName = NULL;

    uint32_t respLen = 0, fileLen = 0, readLen = 0;

    if (response_ota && paramValue && ota_write_data){
    	if (getFileNameVer(&fileName, EXT_FILE_NAME_BIN)){
			if (getFile(fileName, response_ota, &respLen) == ESP_OK){
				if (parse_header(response_ota, "Content-Length", paramValue, RESPONSE_VALUE_LEN_MAX) == ESP_OK){
					fileLen = atoi(paramValue);
					if (fileLen){
						ESP_LOGE(TAG, "file len = %d", fileLen);
						char* retPos = strstr(response_ota, HTTP_END_HEADER);
						if (retPos){
							retPos += 4;
							readLen = respLen - (retPos - response_ota);//Принято первой порцией
							esp_ota_handle_t update_handle = 0 ;
							ret = clearPartition(partition);
							if (ret == ESP_OK){
								ret = esp_ota_begin(partition, TEXT_BUFFSIZE/*OTA_SIZE_UNKNOWN*/, &update_handle);//Обяательно проверить md5 т.к. стирание большого раздела занимает ну очень много времени и task падает
								ret = ESP_OK;
								ESP_LOGE(TAG, "ota begin ret = %x handle = %d", ret, update_handle);
								if ((ret == ESP_OK) && update_handle) {
									memcpy(ota_write_data, retPos, readLen);
									ret = esp_ota_write(update_handle, (const void *)ota_write_data, readLen);
									ESP_LOGE(TAG, "ota write start ret = %x", ret);
									if (ret == ESP_OK){
										while (fileLen > readLen){
											memset(response_ota, 0, TEXT_BUFFSIZE+1);
											respLen = recv(socket_id, response_ota, TEXT_BUFFSIZE, 0);
											if (respLen == 0){
												ESP_LOGE(TAG, "connection error");
												break;
											}
											memcpy(ota_write_data, response_ota, respLen);
											ret = esp_ota_write(update_handle, (const void *)ota_write_data, respLen);
											ret = ESP_OK;
											if (ret != ESP_OK){
												ESP_LOGE(TAG, "ota write  ret = %x respLen = %d", ret, respLen);
												break;
											}
											readLen += respLen;
											taskYIELD();//Обязательно надо дать другим поработать!
										}
										ret = esp_ota_end(update_handle);
									}
								}
							}
						}
					}
				}
			}
		}
    }
    if (ret == ESP_OK){
		esp_md5_context_t md5;
		esp_md5_init(&md5);
	    size_t offset = partition->address;
	    respLen = (fileLen<TEXT_BUFFSIZE)?fileLen:TEXT_BUFFSIZE;
	    readLen = 0;
		ESP_LOGE(TAG, "md5 offset = %d resp = %d read = %d", offset, respLen, readLen);
	    do{
	    	ret = spi_flash_read(offset, response_ota, respLen);
		    if (ret != ESP_OK){
    			ESP_LOGE(TAG, "md5 read error = %x",ret);
		    	break;
		    }
		    esp_md5_update(&md5, (const uint8_t*) response_ota, respLen);
		    offset += respLen;
		    readLen += respLen;
		    respLen = ( (fileLen-readLen) <TEXT_BUFFSIZE)?(fileLen-readLen):TEXT_BUFFSIZE;
        	taskYIELD();//Обязательно надо дать другим поработать!
	    }while(readLen < fileLen);
	    if (ret == ESP_OK){
			uint8_t md5calc[16];
			esp_md5_final(&md5, md5calc);
			for (uint8_t i = 0;i<MD5_LEN; i++){
				if (md5calc[i] != md5file[i]){
					ESP_LOGE(TAG, "md5 error = %x %x", md5calc[i], md5file[i]);
					ret = ESP_ERR_INVALID_CRC;
					break;
				}
			}
    	}
    }
    free(response_ota);
    free(paramValue);
    free(ota_write_data);
    close(socket_id);
    return ret;
}

/*
 * Запрашивает у сервера обновления описание обновления прошивки,
 * возвращает версию и ожидаемую сумму md5
 */
static esp_err_t getVersionFile(char **version, uint8_t md5[MD5_LEN]){

	esp_err_t ret = ESP_ERR_NOT_FOUND;

	if (!connect_to_http_server()) {
        ESP_LOGE(TAG, "Connect to http server failed!");
        close(socket_id);
        return ret;
    }

	uint32_t respLen;
    char* response = calloc(TEXT_BUFFSIZE, sizeof(char));
    char* paramValue = calloc(RESPONSE_VALUE_LEN_MAX, sizeof(char));
	char *fileNameVer = NULL;

	if (getFileNameVer(&fileNameVer, EXT_FILE_NAME_VER) != 0){
		if (response && paramValue){
			if (getFile(fileNameVer, response, &respLen) == ESP_OK){
				if (parse_header(response, "Content-Type", paramValue, RESPONSE_VALUE_LEN_MAX) == ESP_OK){
					if (strcmp(paramValue, "text/plain") == 0){
						char* retPos = strstr(response, HTTP_END_HEADER);		//Header end
						if (retPos){
							memset(paramValue, 0, RESPONSE_VALUE_LEN_MAX);
							retPos += 4;//enter, enter
							sscanf(retPos, "%s", paramValue);					//read version
							if (paramValue){
								asprintf(version, "%s", paramValue);
								retPos += strlen(paramValue)+2;//2- enter
								memset(paramValue, 0, RESPONSE_VALUE_LEN_MAX);
								sscanf(retPos, "%s", paramValue); 				//read md5
								if (strlen(paramValue) == (MD5_STR_LEN-1)){
									ESP_LOGE(TAG, "Md5 = %s len md5 = %d", paramValue, strlen(paramValue));
									uint8_t i = 0;
									char tmp[3];
									tmp[2] = 0;
									for(;i<MD5_LEN; i++ ){ //md5 string to uint8 array
										tmp[0] = *paramValue++;
										tmp[1] = *paramValue++;
										md5[i] = strtol(tmp, NULL, 16);
									}
									ret = ESP_OK;
								}
							}
						}
					}
				}
			}
		}
    }
    free(response);
    free(paramValue);
    free(fileNameVer);
    close(socket_id);

    return ret;
}

void ota_check(void){

	const esp_partition_t *configured = esp_ota_get_boot_partition();
    if (!configured){
        ESP_LOGW(TAG, "Configured OTA boot partition is null");
        return;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running){
        ESP_LOGW(TAG, "Configured OTA running partition is null");
        return;
    }

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    if (!update_partition){
        ESP_LOGW(TAG, "Configured OTA update partition is null");
        return;
    }

    if (strlen(ota_param.server_ip) == 0){
        ESP_LOGW(TAG, "Config OTA is null");
        return;
    }

    uint8_t md5Need[MD5_LEN];
    char* version = NULL;

    if (getVersionFile(&version, md5Need) == ESP_OK){
		ESP_LOGI(TAG, "server version %s current version %d.%d.%d.%d", version, VERSION_APPLICATION.part[VERSION_MAJOR], VERSION_APPLICATION.part[VERSION_MINOR], VERSION_APPLICATION.part[VERSION_PATCH], VERSION_APPLICATION.part[VERSION_BUILD]);
    	if (needUpdate(version)){
        	if (getUpdateFile(update_partition, md5Need) == ESP_OK){
    			ESP_LOGI(TAG, "begin change partition");
        		if (esp_ota_set_boot_partition(update_partition) == ESP_OK){
    				ESP_LOGI(TAG, "RESTART");
        			esp_restart();
        		}
        	}
    	}
    }
    free(version);
}

void ota_init(void){
	uint8_t i = 0;
	for (; i<IP4ADDR_STRLEN_MAX+1; i++)
		ota_param.server_ip[i] = 0;
}
