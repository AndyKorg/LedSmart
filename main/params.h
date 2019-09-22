/*
 * ќбработка всех известных html-переменных в виде списка или по имени
 *  онвертирование в строки и обратно.
 */

#ifndef MAIN_PARAMS_H_
#define MAIN_PARAMS_H_

#include <stddef.h>
#include "esp_err.h"
#include "version.h"

//define method save
enum paramType_t{
  PARAM_TYPE_NONE = -1,	//no save to nvs
  PARAM_TYPE_WIFI = 0,
  PARAM_TYPE_CAEN = 1,
  PARAM_TYPE_OTA = 2
};

//params possible
enum param_Num_t{
  PARAM_SSID_NAME_NUM = 0,
  PARAM_PASWRD_NUM,
  PARAM_MQTT_HOST_NUM,
  PARAM_MQTT_PORT_NUM,
  PARAM_MQTT_USER_NUM,
  PARAM_MQTT_PASS_NUM,
  PARAM_MQTT_CLIENT_ID_NUM,
  PARAM_MQTT_MODEL_NAME_NUM,
  PARAM_APP_VER,							//¬ерси€ приложени€ в виде строки
  PARAM_OTA_SERVER_IP						//IP http сервера с обновлением прошивки, порт всегда 80
};

#define	PARAM_CHANAL_CAYEN			0				//Ќомер канала обслуживающий переменную, определ€етс€ облаком cayenn
#define PARAM_NAME_SENSOR			"Door"			//»м€ сенсора дл€ облака
#define PARAM_CHANAL_LED_STATE		1				// анал статуса светодиода
#define PARAM_NAME_LED_STATE		"led"
#define PARAM_CHANAL_LED_UPDATE		3				//управлени€
#define PARAM_NAME_LED_UPDATE		"LedUpdate"
#define PARAM_NAME_VERSION			"version"		//¬ерси€ приложени€

typedef struct{
  uint8_t pos;
  enum paramType_t paramType;
} seechRecord_t;

/*
 * —тартует выборку известных имен переменных
 * ¬озвращает указатель ESP_OK если список не пустой
 * ¬ передаваемый параметр заносит текущую позицию в списке переменных
 */
esp_err_t getFirstVarName(seechRecord_t* sr);
/*
 * Ѕерет следующий элемент списка переменных
 * ¬озвращает указатель на им€ переменной или null если достигнут конец списка.
 * ”величивает парметр позиции на одну позицию
 */
char* getNextVarName(seechRecord_t* sr);

/*
 * «аписывает значение переменой указанную в позиции sr списка переменных
 * Ќе доделано
 */
esp_err_t saveValue(seechRecord_t sr, char* value);

/*
 * добавл€ет в строку toStr значение переменной valName если найдет.
 * увеличивает lenVal на количество добавленых символов
 * возвращает указатель на конец добавленой строки
 */
char* putsValue(char* toStr, char* varName, size_t *lenVal);

#endif /* MAIN_PARAMS_H_ */
