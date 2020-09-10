#include "_CPlugin_Helper.h"
#ifdef USES_C005
//#######################################################################################################
//################### Controller Plugin 005: Home Assistant (openHAB) MQTT ##############################
//#######################################################################################################

#define CPLUGIN_005
#define CPLUGIN_ID_005         5
#define CPLUGIN_NAME_005       "openHAB MQTT"

String CPlugin_005_pubname;
bool CPlugin_005_mqtt_retainFlag;


bool CPlugin_005(CPlugin::Function function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPlugin::Function::CPLUGIN_PROTOCOL_ADD:
      {
        Protocol[++protocolCount].Number = CPLUGIN_ID_005;
        Protocol[protocolCount].usesMQTT = true;
        Protocol[protocolCount].usesTemplate = true;
        Protocol[protocolCount].usesAccount = true;
        Protocol[protocolCount].usesPassword = true;
        Protocol[protocolCount].usesExtCreds = true;
        Protocol[protocolCount].defaultPort = 1883;
        Protocol[protocolCount].usesID = false;
        break;
      }

    case CPlugin::Function::CPLUGIN_GET_DEVICENAME:
      {
        string = F(CPLUGIN_NAME_005);
        break;
      }

    case CPlugin::Function::CPLUGIN_INIT:
      {
        success = init_mqtt_delay_queue(event->ControllerIndex, CPlugin_005_pubname, CPlugin_005_mqtt_retainFlag);
        break;
      }

    case CPlugin::Function::CPLUGIN_EXIT:
      {
        exit_mqtt_delay_queue();
        break;
      }

    case CPlugin::Function::CPLUGIN_PROTOCOL_TEMPLATE:
      {
        // topic sub
        event->String1 = F("%sysname%/#");
        // topic pub
        event->String2 = F("%sysname%/%tskname%/%valname%");
        break;
      }

    case CPlugin::Function::CPLUGIN_PROTOCOL_RECV:
      {
        controllerIndex_t ControllerID = findFirstEnabledControllerWithId(CPLUGIN_ID_005);
        bool mqtt_retainFlag = CPlugin_005_mqtt_retainFlag;
        if (!validControllerIndex(ControllerID)) {
          // Controller is not enabled.
          break;
        } else {
          // FIXME TD-er: Command is not parsed for template arguments.
          String cmd;
          struct EventStruct TempEvent;
          TempEvent.TaskIndex = event->TaskIndex;
          bool validTopic = false;
          const int lastindex = event->String1.lastIndexOf('/');
          const String lastPartTopic = event->String1.substring(lastindex + 1);
          if (lastPartTopic == F("cmd")) {
            cmd = event->String2;
            parseCommandString(&TempEvent, cmd);
            TempEvent.Source = EventValueSource::Enum::VALUE_SOURCE_MQTT;
            validTopic = true;
          } else {
            if (lastindex > 0) {
              // Topic has at least one separator
              if (isFloat(event->String2) && isInt(lastPartTopic)) {
                int prevLastindex = event->String1.lastIndexOf('/', lastindex - 1);
                cmd = event->String1.substring(prevLastindex + 1, lastindex);
                TempEvent.Par1 = lastPartTopic.toInt();
                TempEvent.Par2 = event->String2.toFloat();
                TempEvent.Par3 = 0;
                validTopic = true;
              }
            } else {
              // no sub topic, should be a broadcast message
              cmd = event->String2;
              if (cmd == F("HELLO")) {
                //got hello broadcast message from app, send widget config on this device
                String iot_topic = "cqw/";
                // give the system name to iot topic
                iot_topic += F("{{name}}");
                iot_topic += "/config";
                String iot_payload = "{\"page\": \"Home\",\"pageId\": 1,\"descr\": \"No config.txt in ESP\",\"widget\": \"anydata\",\"topic\": \"cqw/ESPED/heartbeat\",\"after\": \"L\",\"icon\": \"heart-circle-outline\",\"order\": 10}";
                String fileName;
                fileName = F("iotconfig.txt");
                fs::File f = tryOpenFile(fileName, "r");
                if (f) {
                  char buffer[256];
                  while (f.available()) {
                    int l = f.readBytesUntil('\n', buffer, sizeof(buffer));
                    buffer[l] = 0;
                    iot_payload = buffer;
                    MQTTpublish(event->ControllerIndex, iot_topic.c_str(), iot_payload.c_str(), mqtt_retainFlag);
                  }
                  f.close();
                } else {
                  MQTTpublish(event->ControllerIndex, iot_topic.c_str(), iot_payload.c_str(), mqtt_retainFlag);
                }
              }
            }
          }
          if (validTopic) {
            // in case of event, store to buffer and return...
            String command = parseString(cmd, 1);
            if (command == F("event") || command == F("asyncevent")) {
              eventQueue.add(parseStringToEnd(cmd, 2));
            } else if (!PluginCall(PLUGIN_WRITE, &TempEvent, cmd)) {
              remoteConfig(&TempEvent, cmd);
            }
          }
        }
        break;
      }

    case CPlugin::Function::CPLUGIN_PROTOCOL_SEND:
      {
        String pubname = CPlugin_005_pubname;
        bool mqtt_retainFlag = CPlugin_005_mqtt_retainFlag;

        if (ExtraTaskSettings.TaskIndex != event->TaskIndex) {
          String dummy;
          PluginCall(PLUGIN_GET_DEVICEVALUENAMES, event, dummy);
        }

        parseControllerVariables(pubname, event, false);

        byte valueCount = getValueCountFromSensorType(event->sensorType);
        for (byte x = 0; x < valueCount; x++)
        {
          //MFD: skip publishing for values with empty labels (removes unnecessary publishing of unwanted values)
          if (ExtraTaskSettings.TaskDeviceValueNames[x][0]==0)
             continue; //we skip values with empty labels

          String tmppubname = pubname;
          tmppubname.replace(F("%valname%"), ExtraTaskSettings.TaskDeviceValueNames[x]);
          String value = "";
          // Small optimization so we don't try to copy potentially large strings
          bool m1, m2, m3;
          m1 = 0;
          m2 = 0;
          m3 = 0;
          if (event->sensorType == SENSOR_TYPE_STRING) {
            m1 = MQTTpublish(event->ControllerIndex, tmppubname.c_str(), event->String2.c_str(), mqtt_retainFlag);
            value = event->String2.substring(0, 20); // For the log
          } else {
            value = formatUserVarNoCheck(event, x);
            m2 = MQTTpublish(event->ControllerIndex, tmppubname.c_str(), value.c_str(), mqtt_retainFlag);
            String iot_update_topic = "cqw/";
            iot_update_topic += tmppubname;
            String iot_update_payload = "{\"status\":";
            iot_update_payload += value;
            iot_update_payload += "}";
            m3 = MQTTpublish(event->ControllerIndex, iot_update_topic.c_str(), iot_update_payload.c_str(), mqtt_retainFlag);
          }

          // hard code to publish a message to IoTmanager
          // String iot_topic = "cqw/ESPED/heartbeat/status";
          // String iot_payload = "{\"status\": 16}";
          // m3 = MQTTpublish(event->ControllerIndex, iot_topic.c_str(), iot_payload.c_str(), mqtt_retainFlag);
// #ifndef BUILD_NO_DEBUG
          if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
            String log = F("MQTT : ");
            log += tmppubname;
            log += ' ';
            log += value;
            if (m1) {
              log += F(" sensor mqtt sent==");
            } else
            {
              log += F(" sensor mqtt fail==");
            }
            if (m2) {
              log += F(" other mqtt sent==");
            } else
            {
              log += F(" other mqtt fail==");
            }
            if (m3) {
              log += F(" iot status sent==");
            } else
            {
              log += F(" iot status fail==");
            }
            addLog(LOG_LEVEL_DEBUG, log);
          }
// #endif
        }
        break;
      }

    case CPlugin::Function::CPLUGIN_FLUSH:
      {
        processMQTTdelayQueue();
        delay(0);
        break;
      }

    default:
      break;

  }

  return success;
}
#endif
