#include "src/Helpers/_CPlugin_Helper.h"
#ifdef USES_C005

#include "src/Commands/InternalCommands.h"
#include "src/Globals/EventQueue.h"
#include "src/Globals/ExtraTaskSettings.h"
#include "src/Helpers/PeriodicalActions.h"
#include "src/Helpers/StringParser.h"
#include "_Plugin_Helper.h"

// #######################################################################################################
// ################### Controller Plugin 005: Home Assistant (openHAB) MQTT ##############################
// #######################################################################################################

#define CPLUGIN_005
#define CPLUGIN_ID_005         5
#define CPLUGIN_NAME_005       "openHAB MQTT"

String CPlugin_005_pubname;
bool CPlugin_005_mqtt_retainFlag = false;

bool CPlugin_005(CPlugin::Function function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPlugin::Function::CPLUGIN_PROTOCOL_ADD:
    {
      Protocol[++protocolCount].Number     = CPLUGIN_ID_005;
      Protocol[protocolCount].usesMQTT     = true;
      Protocol[protocolCount].usesTemplate = true;
      Protocol[protocolCount].usesAccount  = true;
      Protocol[protocolCount].usesPassword = true;
      Protocol[protocolCount].usesExtCreds = true;
      Protocol[protocolCount].defaultPort  = 1883;
      Protocol[protocolCount].usesID       = false;
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

          // Topic  : event->String1
          // Message: event->String2
          String cmd;
          bool validTopic = false;
          const int lastindex = event->String1.lastIndexOf('/');
          const String lastPartTopic = event->String1.substring(lastindex + 1);

          if (lastPartTopic == F("cmd")) {//mqtt directly control https://www.letscontrolit.com/wiki/index.php/ESPEasy_Command_Reference

            // Example:
            // topic: ESP_Easy/Bathroom_pir_env/cmd
            // data: gpio,14,0
            // Full command:  gpio,14,0
            cmd = event->String2;
            //SP_C005a: string= ;cmd=gpio,12,0 ;taskIndex=12 ;string1=ESPT12/cmd ;string2=gpio,12,0
            validTopic = true;

          } else if (lastPartTopic == F("control")) {// iot manager control
            // format as Topic: cqw/ESPED/stns1b/control payload: 1 or 0 or text
            // topic information {prefix}/{sysname}/{widget last topic}/control
            // {widget last topic} should config same as device name
            // convert device name to gpio number based on Settings.TaskDevicePin1[taskIndex]
            const String infoTopic = event->String1.substring(0, lastindex);
            const int infoIndex = infoTopic.lastIndexOf('/');
            const String devTopic = event->String1.substring(0, infoIndex);
            const int devIndex = devTopic.lastIndexOf('/');
            const String devName = devTopic.substring(devIndex + 1);
            String log = F("");

            String tdevName = F("%sysname%");
            parseSystemVariables(tdevName, false);

            if (devName == tdevName) { // message is sent for this node host
              // take the task device name from MQTT topic
              const String wigtopic = infoTopic.substring(infoIndex + 1);

              // try to load gpio topic config from task device name
              bool cmd_pulse = false;

              // === [wigtopic] name convert to gpio
              bool find_task = false;
              taskIndex_t task = 0;
              while ((!(find_task)) && (task < TASKS_MAX))  // finde mached task with device name
              {
                // go through all the tasks (items in the devices page) 
                if (Settings.TaskDeviceEnabled[task])
                {
                  LoadTaskSettings(task);
                  if (wigtopic == ExtraTaskSettings.TaskDeviceName) {
                    find_task = true;
                    log = F("found match task: ");
                    log += wigtopic;
                    log += F(" ind: ");
                    log += task;
                  }
                }
                task++;
              }
              
              if(find_task) {
                task--;
                log = F("cmd forming from MQTT-json: ");
                if (wigtopic.indexOf(F("Btn")) > -1) {
                  cmd_pulse = true;
                  cmd = F("Pulse,");
                } else {
                  cmd = F("GPIO,");
                }
                cmd += Settings.TaskDevicePin1[task];
                cmd += F(",");
              } else {
                addLog(LOG_LEVEL_DEBUG, F("no taskDeviceName find with giving topic"));
                break;
              }
              // add control value (e.g. 1 or 0) to cmd
              // String2 is the message from MQTT
              if ((event->String2 == F("1")) ||  (event->String2 == F("0"))) {
                int cmdSta = event->String2.toInt();
                //check if logic inversed
                if (Settings.TaskDevicePin1Inversed[task]) {
                  cmdSta = !cmdSta;
                }
                cmd += String(cmdSta);
                
              } else {// unkown payload cmd  
                String log = F("get unkonkw payload: ");
                log += event->String2;
                addLog(LOG_LEVEL_DEBUG, log);
                break;
              }
              if (cmd_pulse){
                cmd += F(",500");
              }
              // parseCommandString(&TempEvent, cmd);
              // TempEvent.Source = EventValueSource::Enum::VALUE_SOURCE_MQTT;
              validTopic = true;
              
            } else {// the message is not for this device
              break;
            }

          } else if (lastPartTopic == F("dev")) {
            String log = F("in topic dev: ");

            addLog(LOG_LEVEL_DEBUG, log);
            break;
          } else {
            // Example:
            // topic: ESP_Easy/Bathroom_pir_env/GPIO/14 
            // data: 0 or 1
            // Full command:  gpio,14,0

            if (lastindex > 0) {
              // Topic has at least one separator
              if (isFloat(event->String2) && isInt(lastPartTopic)) {
                int prevLastindex = event->String1.lastIndexOf('/', lastindex - 1);
                cmd = event->String1.substring(prevLastindex + 1, lastindex);
                cmd += ',';
                cmd += lastPartTopic;
                cmd += ',';
                cmd += event->String2;
                validTopic = true;
              }
            } else { // no sub topic, should be a broadcast message
              
              cmd = event->String2;
              if (cmd == F("HELLO")) { // hello message from IoTmanager
                // addLog(LOG_LEVEL_DEBUG, F("get HELLO broadcast message"));
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
                  // update current gpio status
                  StaticJsonDocument<384> tk_stat;
                  
                  for (taskIndex_t task = 0; task < TASKS_MAX; task++)
                  {
                    // go through all the tasks (items in the devices page) 
                    // and save stats to tk_stat[deviceName], where deviceName is same as iotconfig topic
                    if (Settings.TaskDeviceEnabled[task])
                    {
                      // only check when such task enabled (a setting in the device item)
                      LoadTaskSettings(task);
                      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(task);
                      byte valueCount = getValueCountFromSensorType(Device[DeviceIndex].VType);

                      String deviceName = ExtraTaskSettings.TaskDeviceName;
                      for (byte x = 0; x < valueCount; x++) {

                        String value = formatUserVarNoCheck(task, x);

                        if (valueCount == 1) {
                          tk_stat[deviceName] = value;
                        } else {
                          tk_stat[deviceName + F("A") + ExtraTaskSettings.TaskDeviceValueNames[x]] = value;
                        }
                      }
                    }
                  }

                  char buffer[384];

                  while (f.available()) {
                    int l = f.readBytesUntil('\n', buffer, sizeof(buffer));
                    
                    buffer[l] = 0;
                    StaticJsonDocument<384> iot_item;
                    // json read
                    deserializeJson(iot_item, buffer);
                    const String topic_devic = iot_item["topic"];
                    // 1 find which task based on topic
                    const int devIndex = topic_devic.lastIndexOf('/');
                    const String devName = topic_devic.substring(devIndex + 1);

                    // 2 find the status of such task
                    // no status information if it is a btn (maybe others as well)
                    if ((tk_stat[devName]) && (iot_item["widget"] != F("btn"))) {
                      iot_item["status"] = tk_stat[devName];
                    }
                    // 3 add status to string for iot payload
                    iot_payload = F("");
                    serializeJson(iot_item, iot_payload);
                    
                    MQTTpublish(event->ControllerIndex, iot_topic.c_str(), iot_payload.c_str(), mqtt_retainFlag);
                  }
                  f.close();
                } else {
                  // MQTTpublish(event->ControllerIndex, iot_topic.c_str(), iot_payload.c_str(), mqtt_retainFlag);
                  addLog(LOG_LEVEL_DEBUG, F("no iotconfig.txt file read"));
                  break;
                }
              } else { // unknow broadcast message
                String log = F("unknow broadcast message: ");
                log += cmd;
                addLog(LOG_LEVEL_DEBUG, log);
                break;
              }
            }
          }
          if (validTopic) {
            // in case of event, store to buffer and return...
            String command = parseString(cmd, 1);
            if (command == F("event") || command == F("asyncevent")) {

              if (Settings.UseRules) {
                eventQueue.add(parseStringToEnd(cmd, 2));
              }
            } else {
              ExecuteCommand(event->TaskIndex, EventValueSource::Enum::VALUE_SOURCE_MQTT, cmd.c_str(), true, true, true);


            }
          }
        }
        break;
      }
      


    case CPlugin::Function::CPLUGIN_PROTOCOL_SEND:
    {
        String pubname = CPlugin_005_pubname;
        bool mqtt_retainFlag = CPlugin_005_mqtt_retainFlag;

        LoadTaskSettings(event->TaskIndex);
        parseControllerVariables(pubname, event, false);

        byte valueCount = getValueCountForTask(event->TaskIndex);
        for (byte x = 0; x < valueCount; x++)
        {
          //MFD: skip publishing for values with empty labels (removes unnecessary publishing of unwanted values)
          if (ExtraTaskSettings.TaskDeviceValueNames[x][0]==0)
             continue; //we skip values with empty labels

          String tmppubname = pubname;
          tmppubname.replace(F("%valname%"), ExtraTaskSettings.TaskDeviceValueNames[x]);
          String value;
          // Small optimization so we don't try to copy potentially large strings
          bool m1, m2, m3;
          if (event->sensorType == Sensor_VType::SENSOR_TYPE_STRING) {
            m1 = MQTTpublish(event->ControllerIndex, tmppubname.c_str(), event->String2.c_str(), mqtt_retainFlag);
            value = event->String2.substring(0, 20); // For the log
          } else {
            value = formatUserVarNoCheck(event, x);
            // send the mqtt message to update openHAB
            m2 = MQTTpublish(event->ControllerIndex, tmppubname.c_str(), value.c_str(), mqtt_retainFlag);

            // report status to IoT manager once a status updated for a task (device)
            String strDevicName = ExtraTaskSettings.TaskDeviceName;
            if (strDevicName.indexOf(F("Btn")) == -1) {

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
          }

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

#endif // ifdef USES_C005
