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
            //mqtt directly control https://www.letscontrolit.com/wiki/index.php/ESPEasy_Command_Reference
            cmd = event->String2;
            parseCommandString(&TempEvent, cmd);
            TempEvent.Source = EventValueSource::Enum::VALUE_SOURCE_MQTT;
            validTopic = true;
          } else if (lastPartTopic == F("control")) {
            // iot manager control
            // format as Topic: cqw/ESPED/stns1b/control payload: 1 or 0 or text
            // topic information {prefix}/{sysname}/{widget last topic}/control
            // {widget last topic} should config same as device name
            // need convert device name to gpio === To Do ===
            const String infoTopic = event->String1.substring(0, lastindex);
            const int infoIndex = infoTopic.lastIndexOf('/');
            const String wigtopic = infoTopic.substring(infoIndex + 1);
            bool cmd_pulse = false;
            cmd = F("gpio,");

            if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {

              String log = F("MQTT wigtopic : ");
              log += wigtopic;

              addLog(LOG_LEVEL_DEBUG, log);
              log = F("info : ");
              log += infoTopic;

              addLog(LOG_LEVEL_DEBUG, log);
              log = F("index : ");
              log += String(infoIndex);

              addLog(LOG_LEVEL_DEBUG, log);
            }
            // === todo wigtopic name convert to gpio (from file?)
            {
              if (wigtopic == F("StudyDeskLight")) {
                  cmd += F("14,");
              } else if (wigtopic == F("StudyExt")) {
                  cmd += F("12,");
              } else if (wigtopic == F("StudyTV")) {
                  cmd += F("16,");
              } else if (wigtopic == F("StudyNAS")) {
                  cmd += F("5,");
              } else if (wigtopic == F("NasSwitch")) {
                  //Pulse,14,1,500
                  cmd = F("Pulse,3,1,300");
                  cmd_pulse = true;
              } else if (wigtopic == F("LivingEnt")) {
                  cmd += F("4,");
              }
            }
            // add control value (e.g. 1 or 0) to end cmd
            if (!(cmd_pulse)){
              cmd += event->String2;
            }
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
                iot_topic += F("%sysname%");
                iot_topic += "/config";
                parseSystemVariables(iot_topic, false);
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
                  // update current gpio status
                  for (taskIndex_t task = 0; task < TASKS_MAX; task++)
                  {
                    if (Settings.TaskDeviceEnabled[task])
                    {
                      LoadTaskSettings(task);
                      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(task);
                      byte valueCount = getValueCountFromSensorType(Device[DeviceIndex].VType);

                      String deviceName = ExtraTaskSettings.TaskDeviceName;
                      
                      for (byte x = 0; x < valueCount; x++) {
                        String value = formatUserVarNoCheck(task, x);
                        String iot_update_topic = iot_topic;
                        iot_update_topic.replace(F("config"), deviceName);
                        if (valueCount == 1) {
                          iot_update_topic += "/{{takvalue}}";
                        } else {
                          iot_update_topic += "A{{takvalue}}/status";
                        }
                        iot_update_topic.replace(F("{{takvalue}}"), ExtraTaskSettings.TaskDeviceValueNames[x]);

                        String iot_update_payload = "{\"status\":";
                        iot_update_payload += value;
                        iot_update_payload += "}";
                        MQTTpublish(event->ControllerIndex, iot_update_topic.c_str(), iot_update_payload.c_str(), mqtt_retainFlag);
                      }
                    }
                  }
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

            if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
              String log = F("cmd Rec : ");
              log += cmd;
              addLog(LOG_LEVEL_DEBUG, log);

              log = F("Ma: ");
              log += command;
              addLog(LOG_LEVEL_DEBUG, log);
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
            // report status to IoT manager once a status updated for a task (device)
            String iot_update_topic = "cqw/";
            iot_update_topic += tmppubname;

            if (valueCount != 1) {
              const int lastindex = iot_update_topic.lastIndexOf('/');
              iot_update_topic[lastindex] = 'A';
              iot_update_topic += "/status";
            }
            iot_update_topic.replace(F("{{takvalue}}"), ExtraTaskSettings.TaskDeviceValueNames[x]);

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
