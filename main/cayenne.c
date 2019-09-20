/*
 * cayenne.c
 *
 *  Created on: 10 мая 2019 г.
 *      Author: Administrator
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sdkconfig.h>
#include <esp_log.h>

#include "utils.h"
#include "nvs_params.h"
#include "cayenne.h"
#include "params.h"

static const char *TAG_MQTT = "MQTT-MY:";

cayenne_t cayenn_cfg;
esp_mqtt_client_handle_t mqtt_client;	//Клиент для каена

cayenne_cb_t reciveTopic;

char*
CayenneTopic (cayenne_t* cfg, const char* type, const char* channal) {
  char* msg = NULL;

  if ((cfg->user) && (cfg->client_id)){
    uint8_t lenTopic = strlen (MQTT_CAYENNE_VER) + strlen(cfg->user)
					+ strlen (MQTT_CAYENNE_DELEMITER) + strlen (cfg->client_id)
					+ strlen (type);
    if (channal)
      lenTopic += strlen ((const char*)channal);
    msg = (char*) calloc (lenTopic + 1, sizeof(char));
    if (msg) {
      strcpy (msg, MQTT_CAYENNE_VER);
      strcpy (msg + strlen (msg), cfg->user);
      strcpy (msg + strlen (msg), MQTT_CAYENNE_DELEMITER);
      strcpy (msg + strlen (msg), cfg->client_id);
      strcpy (msg + strlen (msg), type);
      if (channal)
    	  strcpy (msg + strlen (msg), (const char*) channal);
    }
  }
  return msg;
}

esp_err_t CayenneChangeInteger(cayenne_t* cfg, const uint8_t chanal, const char* nameSensor, const uint32_t value){

  esp_err_t ret = ESP_ERR_INVALID_STATE;
  if (mqtt_client){
    #define CHANAL_NUM_LEN_MAX 10
    char* chanalString = (char*) calloc(CHANAL_NUM_LEN_MAX, sizeof(char));
    sprintf(chanalString, "%d", chanal);

    char* topic = CayenneTopic(cfg, MQTT_CAYENNE_TYPE_DATA, chanalString);
    #define VALUE_LEN 20 //Типа 20 цифр данные
    char* result = calloc(strlen(nameSensor)+VALUE_LEN, sizeof(char));
    sprintf(result, CAYENNE_DIGITAL_SENSOR, value);//TODO: нужна динамическая строка форматирования
    ESP_LOGE(TAG_MQTT, "pub topic = %s result = %s", topic, result);
    if (esp_mqtt_client_publish(mqtt_client, topic, result, strlen(result), MQTT_QOS_TYPE_AT_MOST_ONCE, MQTT_RETAIN_OFF) >= 0)
      ret = ESP_OK;
    free(result);
    free(topic);
    free(chanalString);
  }
  return ret;
}

esp_err_t CayenneSubscribe(cayenne_t* cfg, const uint8_t chanal){
	  esp_err_t ret = ESP_ERR_INVALID_STATE;
	  if (mqtt_client){
	    #define CHANAL_NUM_LEN_MAX 10
	    char* chanalString = (char*) calloc(CHANAL_NUM_LEN_MAX, sizeof(char));
	    sprintf(chanalString, "%d", chanal);

	    char* topic = CayenneTopic(cfg, MQTT_CAYENNE_TYPE_CMD, chanalString);
	    ESP_LOGE(TAG_MQTT, "subs topic = %s", topic);
	    ret = esp_mqtt_client_subscribe(mqtt_client, topic, MQTT_RETAIN_OFF);
	    free(topic);
	    free(chanalString);
	  }
	  return ret;
}

esp_err_t CayenneResponse(cayenne_t* cfg, const char* nameResponse, esp_err_t response, const char* msg_error){
	  esp_err_t ret = ESP_ERR_INVALID_STATE;
	  if (mqtt_client){
	    char* topic = CayenneTopic(cfg, MQTT_CAYENNE_TYPE_RESPONSE, NULL);

	    int resultLen = strlen(MQTT_CAYENNE_RESPONSE_ERR) //max length string
	    		+strlen(nameResponse);
	    if ((response != ESP_OK) && (msg_error)){
	    	resultLen += strlen(msg_error);
	    }
	    char* result = calloc(resultLen+ (sizeof(char)*3), sizeof(char));//3 = end string + symbol "=" for error message + reserved
	    if (response != ESP_OK){
	    	strcpy (result, MQTT_CAYENNE_RESPONSE_ERR);
    		strcpy(result, nameResponse);
	    	if (msg_error){
	    		strcpy(result, "=");
	    		strcpy(result, msg_error);
	    	}
	    }
	    else{
	    	strcpy (result, MQTT_CAYENNE_RESPONSE_OK);
    		strcpy(result, nameResponse);
	    }
	    ESP_LOGE(TAG_MQTT, "resp topic = %s result = %s", topic, result);
	    if (esp_mqtt_client_publish(mqtt_client, topic, result, strlen(result), MQTT_QOS_TYPE_AT_MOST_ONCE, MQTT_RETAIN_OFF) >= 0)
	      ret = ESP_OK;
	    free(topic);
	    free(result);
	  }
	  return ret;
}

/*
 * Работает, но на стороне сервера обнвоялет состояение только после обновления страницы
 */
esp_err_t CayenneUpdateActuator(cayenne_t* cfg, const uint8_t chanal, const uint32_t value){

  esp_err_t ret = ESP_ERR_INVALID_STATE;
  if (mqtt_client){
    #define CHANAL_NUM_LEN_MAX 10
    char* chanalString = (char*) calloc(CHANAL_NUM_LEN_MAX, sizeof(char));
    sprintf(chanalString, "%d", chanal);
    char* topic = CayenneTopic(cfg, MQTT_CAYENNE_TYPE_DATA, chanalString);
    #define VALUE_LEN 20 //Типа 20 цифр данные
    char* result = calloc(strlen(CAYENNE_DIGITAL_SENSOR)+VALUE_LEN, sizeof(char));
    sprintf(result, "%d", value);//TODO: нужна динамическая строка форматирования
    ESP_LOGE(TAG_MQTT, "upd topic = %s result = %s", topic, result);
    if (esp_mqtt_client_publish(mqtt_client, topic, result, strlen(result), MQTT_QOS_TYPE_AT_MOST_ONCE, MQTT_RETAIN_OFF) >= 0)
      ret = ESP_OK;
    free(result);
    free(topic);
    free(chanalString);
  }
  return ret;
}

esp_err_t cayenne_reg(uint8_t chanal, cayenne_cb_t func){	//Регистрация реакции на событие в канале
	reciveTopic = func;
	return ESP_OK;
}

esp_err_t Cayenne_event_handler (esp_mqtt_event_handle_t event) {
  esp_mqtt_client_handle_t client = event->client;
  char* topic = NULL;
  char * topicCmd = NULL;
  // your_context_t *context = event->context;
  switch (event->event_id)
  {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGE(TAG_MQTT, "MQTT_EVENT_CONNECTED");
      topic = CayenneTopic (&cayenn_cfg, MQTT_CAYENNE_TYPE_SYS_MODEL, NULL);
      ESP_LOGE(TAG_MQTT, "topic = %s", topic);
      ESP_LOGE(TAG_MQTT, "dev = %s", cayenn_cfg.deviceName);
      int msg_id = esp_mqtt_client_publish (client, topic, cayenn_cfg.deviceName, strlen(cayenn_cfg.deviceName), MQTT_QOS_TYPE_AT_MOST_ONCE, MQTT_RETAIN_OFF);
      ESP_LOGE(TAG_MQTT, "connect sent publish successful, msg_id = %x", msg_id);
      CayenneSubscribe(&cayenn_cfg, PARAM_CHANAL_LED_UPDATE);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGE(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");
      ESP_LOGI(TAG_MQTT, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
      ESP_LOGI(TAG_MQTT, "DATA=%.*s\r\n", event->data_len, event->data);
      char* comma = strchr(event->data, ',');
      if (comma){
    	  *comma = '\0';
          ESP_LOGI(TAG_MQTT, "payload=%s", event->data);
    	  CayenneResponse(&cayenn_cfg, event->data, ESP_OK, NULL);
    	  if (reciveTopic){
    		  int reciv = atoi(++comma);
              ESP_LOGI(TAG_MQTT, "led topic=%d", reciv);
    		  reciveTopic(&reciv);
    	  }
      }
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
      break;
    default:
      ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
      break;
  }
  if (topic != NULL){
    free (topic);
  }
  if (topicCmd != NULL){
    free (topicCmd);
  }

  return ESP_OK;
}

void Cayenne_app_start (void) {
  if (read_cay_param(&cayenn_cfg) == ESP_OK){

    char* tmp = CayenneTopic (&cayenn_cfg, MQTT_CAYENNE_TYPE_SYS_MODEL, NULL);

    char* hostProtokol = calloc(CAYENN_MAX_LEN+strlen(MQTT_PROTOKOL)+1, sizeof(char));
    strcpy(hostProtokol, MQTT_PROTOKOL);
    strcat(hostProtokol, cayenn_cfg.host);
    ESP_LOGE(TAG_MQTT, "h %s", hostProtokol);

    esp_mqtt_client_config_t mqtt_cfg =
      { .uri = hostProtokol, .port = cayenn_cfg.port,
	  .username = cayenn_cfg.user, .password = cayenn_cfg.pass, .client_id = cayenn_cfg.client_id,
	  .lwt_qos = 0, //Подтверждение доставки
	  .lwt_topic = cayenn_cfg.deviceName,
	  .lwt_msg = tmp,
	  .lwt_msg_len = strlen ((const char*)tmp),
	  .event_handle = Cayenne_event_handler,
	  .transport = MQTT_TRANSPORT_OVER_TCP
      // .user_context = (void *)your_context
      };

    mqtt_client = esp_mqtt_client_init (&mqtt_cfg);
    esp_mqtt_client_start (mqtt_client);
    free (tmp);
    free (hostProtokol);
  }
}

esp_err_t Cayenne_app_stop (void){ //Оторвать все подключения, ESP_OK - закрытие соединения начато
	return esp_mqtt_client_stop(mqtt_client);//TODO: отписку сделать
}

esp_err_t Cayenne_Init (void) {
  return read_cay_param(&cayenn_cfg);
}
