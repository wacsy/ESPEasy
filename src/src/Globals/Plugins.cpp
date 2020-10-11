#include "Plugins.h"

#include "../../ESPEasy_Log.h"
#include "../../_Plugin_Helper.h"

#include "../DataStructs/ESPEasy_EventStruct.h"
#include "../DataStructs/ESPEasy_plugin_functions.h"
#include "../DataStructs/TimingStats.h"

#include "../Globals/Device.h"
#include "../Globals/ESPEasy_Scheduler.h"
#include "../Globals/ExtraTaskSettings.h"
#include "../Globals/GlobalMapPortStatus.h"
#include "../Globals/Settings.h"

#include "../Helpers/ESPEasyRTC.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Helpers/Hardware.h"
#include "../Helpers/Misc.h"
#include "../Helpers/PortStatus.h"

#define USERVAR_MAX_INDEX    (VARS_PER_TASK * TASKS_MAX)


deviceIndex_t  INVALID_DEVICE_INDEX  = PLUGIN_MAX;
taskIndex_t    INVALID_TASK_INDEX    = TASKS_MAX;
pluginID_t     INVALID_PLUGIN_ID     = 0;
userVarIndex_t INVALID_USERVAR_INDEX = USERVAR_MAX_INDEX;
taskVarIndex_t INVALID_TASKVAR_INDEX = VARS_PER_TASK;

std::map<pluginID_t, deviceIndex_t> Plugin_id_to_DeviceIndex;
std::vector<pluginID_t>    DeviceIndex_to_Plugin_id;
std::vector<deviceIndex_t> DeviceIndex_sorted;

float customFloatVar[CUSTOM_VARS_MAX];

float UserVar[VARS_PER_TASK * TASKS_MAX];

int deviceCount = -1;

boolean (*Plugin_ptr[PLUGIN_MAX])(byte,
                                  struct EventStruct *,
                                  String&);





bool validDeviceIndex(deviceIndex_t index) {
  if (index < PLUGIN_MAX) {
    const pluginID_t pluginID = DeviceIndex_to_Plugin_id[index];
    return pluginID != INVALID_PLUGIN_ID;
  }
  return false;
}

bool validTaskIndex(taskIndex_t index) {
  return index < TASKS_MAX;
}

bool validPluginID(pluginID_t pluginID) {
  return (pluginID != INVALID_PLUGIN_ID);
}

bool validPluginID_fullcheck(pluginID_t pluginID) {
  if (!validPluginID(pluginID)) {
    return false;
  }
  auto it = Plugin_id_to_DeviceIndex.find(pluginID);
  return (it != Plugin_id_to_DeviceIndex.end());
}

bool validUserVarIndex(userVarIndex_t index) {
  return index < USERVAR_MAX_INDEX;
}

bool validTaskVarIndex(taskVarIndex_t index) {
  return index < VARS_PER_TASK;
}

bool supportedPluginID(pluginID_t pluginID) {
  return validDeviceIndex(getDeviceIndex(pluginID));
}

deviceIndex_t getDeviceIndex_from_TaskIndex(taskIndex_t taskIndex) {
  if (validTaskIndex(taskIndex)) {
    return getDeviceIndex(Settings.TaskDeviceNumber[taskIndex]);
  }
  return INVALID_DEVICE_INDEX;
}

deviceIndex_t getDeviceIndex(pluginID_t pluginID)
{
  if (pluginID != INVALID_PLUGIN_ID) {
    auto it = Plugin_id_to_DeviceIndex.find(pluginID);

    if (it != Plugin_id_to_DeviceIndex.end())
    {
      if (!validDeviceIndex(it->second)) { return INVALID_DEVICE_INDEX; }

      if (Device[it->second].Number != pluginID) {
        // FIXME TD-er: Just a check for now, can be removed later when it does not occur.
        addLog(LOG_LEVEL_ERROR, F("getDeviceIndex error in Device Vector"));
      }
      return it->second;
    }
  }
  return INVALID_DEVICE_INDEX;
}

/********************************************************************************************\
   Find name of plugin given the plugin device index..
 \*********************************************************************************************/
String getPluginNameFromDeviceIndex(deviceIndex_t deviceIndex) {
  String deviceName = "";

  if (validDeviceIndex(deviceIndex)) {
    Plugin_ptr[deviceIndex](PLUGIN_GET_DEVICENAME, nullptr, deviceName);
  }
  return deviceName;
}

String getPluginNameFromPluginID(pluginID_t pluginID) {
  deviceIndex_t deviceIndex = getDeviceIndex(pluginID);

  if (!validDeviceIndex(deviceIndex)) {
    String name = F("Plugin ");
    name += String(static_cast<int>(pluginID));
    name += F(" not included in build");
    return name;
  }
  return getPluginNameFromDeviceIndex(deviceIndex);
}

// ********************************************************************************
// Device Sort routine, compare two array entries
// ********************************************************************************
bool arrayLessThan(const String& ptr_1, const String& ptr_2)
{
  unsigned int i = 0;

  while (i < ptr_1.length()) // For each character in string 1, starting with the first:
  {
    if (ptr_2.length() < i)  // If string 2 is shorter, then switch them
    {
      return true;
    }
    const char check1 = static_cast<char>(ptr_1[i]); // get the same char from string 1 and string 2
    const char check2 = static_cast<char>(ptr_2[i]);

    if (check1 == check2) {
      // they're equal so far; check the next char !!
      i++;
    } else {
      return check2 > check1;
    }
  }
  return false;
}

// ********************************************************************************
// Device Sort routine, actual sorting alfabetically by plugin name.
// Sorting does happen case sensitive.
// ********************************************************************************
void sortDeviceIndexArray() {
  // First fill the existing number of the DeviceIndex.
  DeviceIndex_sorted.resize(deviceCount + 1);

  for (deviceIndex_t x = 0; x <= deviceCount; x++) {
    if (validPluginID(DeviceIndex_to_Plugin_id[x])) {
      DeviceIndex_sorted[x] = x;
    } else {
      DeviceIndex_sorted[x] = INVALID_DEVICE_INDEX;
    }
  }

  // Do the sorting.
  int innerLoop;
  int mainLoop;

  for (mainLoop = 1; mainLoop <= deviceCount; mainLoop++)
  {
    innerLoop = mainLoop;

    while (innerLoop  >= 1)
    {
      if (arrayLessThan(
            getPluginNameFromDeviceIndex(DeviceIndex_sorted[innerLoop]),
            getPluginNameFromDeviceIndex(DeviceIndex_sorted[innerLoop - 1])))
      {
        deviceIndex_t temp = DeviceIndex_sorted[innerLoop - 1];
        DeviceIndex_sorted[innerLoop - 1] = DeviceIndex_sorted[innerLoop];
        DeviceIndex_sorted[innerLoop]     = temp;
      }
      innerLoop--;
    }
  }
}

// ********************************************************************************
// Functions to assist changing I2C multiplexer port or clock speed 
// when addressing a task
// ********************************************************************************

void prepare_I2C_by_taskIndex(taskIndex_t taskIndex, deviceIndex_t DeviceIndex) {
  if (!validTaskIndex(taskIndex) || !validDeviceIndex(DeviceIndex)) {
    return;
  }
  if (Device[DeviceIndex].Type != DEVICE_TYPE_I2C) {
    return;
  }
#ifdef FEATURE_I2CMULTIPLEXER
  I2CMultiplexerSelectByTaskIndex(taskIndex);
  // Output is selected after this write, so now we must make sure the
  // frequency is set before anything else is sent.
#endif

  if (bitRead(Settings.I2C_Flags[taskIndex], I2C_FLAGS_SLOW_SPEED)) {
    I2CSelectClockSpeed(true); // Set to slow
  }
}


void post_I2C_by_taskIndex(taskIndex_t taskIndex, deviceIndex_t DeviceIndex) {
  if (!validTaskIndex(taskIndex) || !validDeviceIndex(DeviceIndex)) {
    return;
  }
  if (Device[DeviceIndex].Type != DEVICE_TYPE_I2C) {
    return;
  }
#ifdef FEATURE_I2CMULTIPLEXER
  I2CMultiplexerOff();
#endif

  if (bitRead(Settings.I2C_Flags[taskIndex], I2C_FLAGS_SLOW_SPEED)) {
    I2CSelectClockSpeed(false);  // Reset
  }
}


/*********************************************************************************************\
* Function call to all or specific plugins
\*********************************************************************************************/
byte PluginCall(byte Function, struct EventStruct *event, String& str)
{
  struct EventStruct TempEvent;

  if (event == nullptr) {
    event = &TempEvent;
  }
  else {
    TempEvent = (*event);
  }


  checkRAM(F("PluginCall"), Function);

  switch (Function)
  {
    // Unconditional calls to all plugins
    case PLUGIN_DEVICE_ADD:
    case PLUGIN_UNCONDITIONAL_POLL:    // FIXME TD-er: PLUGIN_UNCONDITIONAL_POLL is not being used at the moment

      for (byte x = 0; x < PLUGIN_MAX; x++) {
        if (validPluginID(DeviceIndex_to_Plugin_id[x])) {
          if (Function == PLUGIN_DEVICE_ADD) {
            if ((deviceCount + 2) > static_cast<int>(Device.size())) {
              // Increase with 16 to get some compromise between number of resizes and wasted space
              unsigned int newSize = Device.size();
              newSize = newSize + 16 - (newSize % 16);
              Device.resize(newSize);

              // FIXME TD-er: Also resize DeviceIndex_to_Plugin_id ?
            }
          }
          // FIXME TD-er: This is not correct as we don't have a taskIndex here when addressing a plugin
          /*
          taskIndex_t  taskIndex = INVALID_TASK_INDEX;
          if (Function != PLUGIN_DEVICE_ADD && Device[x].Type == DEVICE_TYPE_I2C) {
            unsigned int varNr;
            validTaskVars(event, taskIndex, varNr);
            prepare_I2C_by_taskIndex(taskIndex, x);
          }
          */
          START_TIMER;
          Plugin_ptr[x](Function, event, str);
          STOP_TIMER_TASK(x, Function);
          /*
          // FIXME TD-er: This is not correct as we don't have a taskIndex here when addressing a plugin
          if (Function != PLUGIN_DEVICE_ADD) {
            post_I2C_by_taskIndex(taskIndex, x);
          }
          */
          delay(0); // SMY: call delay(0) unconditionally
        }
      }
      return true;

    case PLUGIN_MONITOR:

      for (auto it = globalMapPortStatus.begin(); it != globalMapPortStatus.end(); ++it) {
        // only call monitor function if there the need to
        if (it->second.monitor || it->second.command || it->second.init) {
          TempEvent.Par1 = getPortFromKey(it->first);

          // initialize the "x" variable to synch with the pluginNumber if second.x == -1
          if (!validDeviceIndex(it->second.x)) { it->second.x = getDeviceIndex(getPluginFromKey(it->first)); }

          const deviceIndex_t DeviceIndex = it->second.x;
          if (validDeviceIndex(DeviceIndex))  {
            // FIXME TD-er: This is not correct, as the event is NULL for calls to PLUGIN_MONITOR
            /*
            taskIndex_t  taskIndex = INVALID_TASK_INDEX;
            if (Device[DeviceIndex].Type == DEVICE_TYPE_I2C) {
              unsigned int varNr;  
              validTaskVars(event, taskIndex, varNr);
              prepare_I2C_by_taskIndex(taskIndex, DeviceIndex);
            }
            */
            START_TIMER;
            Plugin_ptr[DeviceIndex](Function, &TempEvent, str);
            STOP_TIMER_TASK(DeviceIndex, Function);
            // post_I2C_by_taskIndex(taskIndex, DeviceIndex);
          }
        }
      }
      return true;


    // Call to all plugins. Return at first match
    case PLUGIN_WRITE:
    case PLUGIN_REQUEST:
    {
      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++)
      {
        if (Settings.TaskDeviceEnabled[taskIndex] && validPluginID_fullcheck(Settings.TaskDeviceNumber[taskIndex]))
        {
          if (Settings.TaskDeviceDataFeed[taskIndex] == 0) // these calls only to tasks with local feed
          {
            const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);

            if (validDeviceIndex(DeviceIndex)) {
              TempEvent.setTaskIndex(taskIndex);
              //checkDeviceVTypeForTask(&TempEvent);
              prepare_I2C_by_taskIndex(taskIndex, DeviceIndex);
              checkRAM(F("PluginCall_s"), taskIndex);
              START_TIMER;
              bool retval = (Plugin_ptr[DeviceIndex](Function, &TempEvent, str));
              STOP_TIMER_TASK(DeviceIndex, Function);
              post_I2C_by_taskIndex(taskIndex, DeviceIndex);
              delay(0); // SMY: call delay(0) unconditionally

              if (retval) {
                CPluginCall(CPlugin::Function::CPLUGIN_ACKNOWLEDGE, &TempEvent, str);
                return true;
              }
            }
          }
        }
      }

      // @FIXME TD-er: work-around as long as gpio command is still performed in P001_switch.
      for (deviceIndex_t deviceIndex = 0; deviceIndex < PLUGIN_MAX; deviceIndex++) {
        if (validPluginID(DeviceIndex_to_Plugin_id[deviceIndex])) {
          if (Plugin_ptr[deviceIndex](Function, event, str)) {
            delay(0); // SMY: call delay(0) unconditionally
            CPluginCall(CPlugin::Function::CPLUGIN_ACKNOWLEDGE, event, str);
            return true;
          }
        }
      }
      break;
    }

    // Call to all plugins used in a task. Return at first match
    case PLUGIN_SERIAL_IN:
    case PLUGIN_UDP_IN:
    {
      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++)
      {
        if (Settings.TaskDeviceEnabled[taskIndex] && validPluginID_fullcheck(Settings.TaskDeviceNumber[taskIndex]))
        {
          const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);

          if (validDeviceIndex(DeviceIndex)) {
            TempEvent.setTaskIndex(taskIndex);
            //checkDeviceVTypeForTask(&TempEvent);

            // TempEvent.idx = Settings.TaskDeviceID[taskIndex]; todo check
            prepare_I2C_by_taskIndex(taskIndex, DeviceIndex);
            START_TIMER;
            bool retval =  (Plugin_ptr[DeviceIndex](Function, &TempEvent, str));
            STOP_TIMER_TASK(DeviceIndex, Function);
            post_I2C_by_taskIndex(taskIndex, DeviceIndex);
            delay(0); // SMY: call delay(0) unconditionally

            if (retval) {
              checkRAM(F("PluginCallUDP"), taskIndex);
              return true;
            }
          }
        }
      }
      return false;
    }

    // Call to all plugins that are used in a task
    case PLUGIN_ONCE_A_SECOND:
    case PLUGIN_TEN_PER_SECOND:
    case PLUGIN_FIFTY_PER_SECOND:
    case PLUGIN_INIT_ALL:
    case PLUGIN_CLOCK_IN:
    case PLUGIN_EVENT_OUT:
    case PLUGIN_TIME_CHANGE:
    {
      if (Function == PLUGIN_INIT_ALL) {
        Function = PLUGIN_INIT;
      }

      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++)
      {
        if (Settings.TaskDeviceEnabled[taskIndex] && validPluginID_fullcheck(Settings.TaskDeviceNumber[taskIndex]))
        {
          if (Settings.TaskDeviceDataFeed[taskIndex] == 0) // these calls only to tasks with local feed
          {
            const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);

            if (validDeviceIndex(DeviceIndex)) {
              TempEvent.setTaskIndex(taskIndex);
              //checkDeviceVTypeForTask(&TempEvent);

              // TempEvent.idx = Settings.TaskDeviceID[taskIndex]; todo check
              TempEvent.OriginTaskIndex = event->TaskIndex;
              checkRAM(F("PluginCall_s"), taskIndex);

              if (Function == PLUGIN_INIT) {
                // Schedule the plugin to be read.
                Scheduler.schedule_task_device_timer_at_init(TempEvent.TaskIndex);
              }
              prepare_I2C_by_taskIndex(taskIndex, DeviceIndex);
              START_TIMER;
              Plugin_ptr[DeviceIndex](Function, &TempEvent, str);
              STOP_TIMER_TASK(DeviceIndex, Function);
              post_I2C_by_taskIndex(taskIndex, DeviceIndex);
              delay(0); // SMY: call delay(0) unconditionally
            }
          }
        }
      }
      return true;
    }

    // Call to specific task which may interact with the hardware
    case PLUGIN_INIT:
    case PLUGIN_EXIT:
    case PLUGIN_WEBFORM_LOAD:
    case PLUGIN_READ:
    case PLUGIN_GET_PACKED_RAW_DATA:
    {
      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(event->TaskIndex);

      if (validDeviceIndex(DeviceIndex)) {
        if (Function == PLUGIN_INIT) {
          // Schedule the plugin to be read.
          Scheduler.schedule_task_device_timer_at_init(event->TaskIndex);
        }

        if (ExtraTaskSettings.TaskIndex != event->TaskIndex) {
          // LoadTaskSettings may call PLUGIN_GET_DEVICEVALUENAMES.
          LoadTaskSettings(event->TaskIndex);
        }
        event->BaseVarIndex = event->TaskIndex * VARS_PER_TASK;
        {
          String descr;
          descr.reserve(20);
          descr  = String(F("PluginCall_task_"));
          descr += event->TaskIndex;
          checkRAM(descr, String(Function));
        }
        prepare_I2C_by_taskIndex(event->TaskIndex, DeviceIndex);
        START_TIMER;
        bool retval =  Plugin_ptr[DeviceIndex](Function, event, str);

        if (retval && (Function == PLUGIN_READ)) {
          saveUserVarToRTC();
        }

        if (Function == PLUGIN_EXIT) {
          clearPluginTaskData(event->TaskIndex);
        }
        STOP_TIMER_TASK(DeviceIndex, Function);
        post_I2C_by_taskIndex(event->TaskIndex, DeviceIndex);
        delay(0); // SMY: call delay(0) unconditionally
        return retval;
      }
      return false;
    }

    // Call to specific task not interacting with hardware
    case PLUGIN_GET_CONFIG:
    case PLUGIN_GET_DEVICEVALUENAMES:
    case PLUGIN_GET_DEVICEVALUECOUNT:
    case PLUGIN_GET_DEVICEVTYPE:
    case PLUGIN_GET_DEVICEGPIONAMES:
    case PLUGIN_WEBFORM_SAVE:
    case PLUGIN_WEBFORM_SHOW_VALUES:
    case PLUGIN_WEBFORM_SHOW_CONFIG:
    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    case PLUGIN_WEBFORM_SHOW_SERIAL_PARAMS:
    case PLUGIN_SET_CONFIG:
    case PLUGIN_SET_DEFAULTS:
    {
      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(event->TaskIndex);

      if (validDeviceIndex(DeviceIndex)) {
        // LoadTaskSettings may call PLUGIN_GET_DEVICEVALUENAMES.
        LoadTaskSettings(event->TaskIndex);
        event->BaseVarIndex = event->TaskIndex * VARS_PER_TASK;
        {
          String descr;
          descr.reserve(20);
          descr  = String(F("PluginCall_task_"));
          descr += event->TaskIndex;
          checkRAM(descr, String(Function));
        }
        if (Function == PLUGIN_SET_DEFAULTS) {
          for (int i = 0; i < VARS_PER_TASK; ++i) {
            UserVar[event->BaseVarIndex + i] = 0.0f;
          }
        }
        if (Function == PLUGIN_GET_DEVICEVALUECOUNT) {
          event->Par1 = Device[DeviceIndex].ValueCount;
        }
        if (Function == PLUGIN_GET_DEVICEVTYPE) {
          event->sensorType = Device[DeviceIndex].VType;
        }

        START_TIMER;
        bool retval =  Plugin_ptr[DeviceIndex](Function, event, str);
        if (Function == PLUGIN_SET_DEFAULTS) {
          saveUserVarToRTC();
        }
        STOP_TIMER_TASK(DeviceIndex, Function);
        delay(0); // SMY: call delay(0) unconditionally
        return retval;
      }
      return false;
    }

  } // case
  return false;
}
