
// #include section
#include "_Plugin_Helper.h"
#ifdef USES_P253


#define PLUGIN_253
#define PLUGIN_ID_253     253           // plugin id
#define PLUGIN_NAME_253   "GPIO - Time Restricted Link Switch " // dislpayed in the selection list
#define PLUGIN_VALUENAME1_253 "status" // variable output of the plugin. The label is in quotation marks
#define PLUGIN_VALUENAME2_253 "restricted" // multiple outputs are supported
#define PLUGIN_253_DEBUG  true         // set to true for extra log info in the debug


/*
//   PIN/port configuration is stored in the following:
//   CONFIG_PIN1 - The first GPIO pin selected within the task
//   CONFIG_PIN2 - The second GPIO pin selected within the task
//   CONFIG_PIN3 - The third GPIO pin selected within the task
//   CONFIG_PORT - The port in case the device has multiple in/out pins
//
//   Custom configuration is stored in the following:
//   PCONFIG(x)
//   x can be between 1 - 8 and can store values between -32767 - 32768 (16 bit)
//
//   N.B. these are aliases for a longer less readable amount of code. See _Plugin_Helper.h
//   PCONFIG_LABEL(x) is a function to generate a unique label used as HTML id to be able to match 
//                    returned values when saving a configuration.

#define Pxxx_BAUDRATE           PCONFIG_LONG(0)
#define Pxxx_BAUDRATE_LABEL     PCONFIG_LABEL(0)
*/
#define P253_Time_Start_Hour           PCONFIG(2)
#define P253_Time_Start_Minute         PCONFIG(3)
#define P253_Time_End_Hour             PCONFIG(4)
#define P253_Time_End_Minute           PCONFIG(5)
#define P253_His_G1_Status             PCONFIG(6)
#define P253_Debounce_Timer            PCONFIG_LONG(0)
#define P253_Debounce_Conf             PCONFIG_FLOAT(0)

#define P253_Time_Start_Hour_LABEL           PCONFIG_LABEL(2)
#define P253_Time_Start_Minute_LABEL         PCONFIG_LABEL(3)
#define P253_Time_End_Hour_LABEL             PCONFIG_LABEL(4)
#define P253_Time_End_Minute_LABEL           PCONFIG_LABEL(5)
#define P253_Debounce_Conf_LABEL             PCONFIG_LABEL(6)
// #define Pxxx_OUTPUT_TYPE_INDEX  2


// main plugin function:
boolean Plugin_253(byte function, struct EventStruct *event, String& string)
{
  // function: output on pin 2 linked to pin1 with defined time restrict
  // event: when pin1 status changed -> chenck time -> toggle pin2

  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      // This case defines the device characteristics, edit appropriately

      Device[++deviceCount].Number           = PLUGIN_ID_253;
      Device[deviceCount].Type               = DEVICE_TYPE_DUAL;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_SWITCH;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].ValueCount         = 2;
      Device[deviceCount].OutputDataType     = Output_Data_type_t::Default;
      Device[deviceCount].PullUpOption       = true;
      Device[deviceCount].InverseLogicOption = true;
      Device[deviceCount].FormulaOption      = false;
      Device[deviceCount].Custom             = false;
      Device[deviceCount].SendDataOption     = true;
      Device[deviceCount].GlobalSyncOption   = true;
      Device[deviceCount].TimerOption        = false;
      Device[deviceCount].TimerOptional      = false;
      Device[deviceCount].DecimalsOnly       = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      // return the device name
      string = F(PLUGIN_NAME_253);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_253));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_253));
      break;
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
      // Called to show non default pin assignment or addresses like for plugins using serial or 1-Wire
      string += serialHelper_getSerialTypeLabel(event);
      success = true;
      break;
    }

    case PLUGIN_SET_DEFAULTS:  
    // To Do what settings we need here? 
    // GPIO pin1 pin2, time set 23:30 - 7:00
    {
      // Set a default config here, which will be called when a plugin is assigned to a task.
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      // The user's selection will be stored in
      // PCONFIG(x) (custom configuration)

      // Make sure not to append data to the string variable in this PLUGIN_WEBFORM_LOAD call.
      // This has changed, so now use the appropriate functions to write directly to the Streaming
      // web_server. This takes much less memory and is faster.
      // There will be an error in the web interface if something is added to the "string" variable.

      // Use any of the following (defined at Markup_forms.):
      // addFormNote(F("not editable text added here"));
      addFormSeparator(2);

      // addFormNote(F("here added form note"));

      //form to select pin1 - input signal switch pin select
      // addFormPinSelect(F("Input Switch Pin"),F("P253_InP"),CONFIG_PIN1);

      //form to select pin2 - output pin
      // addFormPinSelect(F("Output Pin (Lights)"),F("P253_OtP"),CONFIG_PIN2);

      //do we still need inverse logic option if such set as true?
      // addFormCheckBox(F("inverse output pin"), F("P253_opI"), PCONFIG(1));
      addFormNumericBox(F("Input De-bounce (ms)"), P253_Debounce_Conf_LABEL, round(P253_Debounce_Conf), 0, 250);

      //form to select time start
      addFormNote(F("Do not light up time start: hours:minutes"));

      //hours
      addFormNumericBox(F("Hour"), P253_Time_Start_Hour_LABEL, P253_Time_Start_Hour, 0, 23); //23:30
      //minutes
      addFormNumericBox(F("Minute"), P253_Time_Start_Minute_LABEL, P253_Time_Start_Minute, 0, 59);
      //form to select time end
      addFormNote(F("Do not light up time end: hours:minutes"));

      //hours
      addFormNumericBox(F("Hour"), P253_Time_End_Hour_LABEL, P253_Time_End_Hour, 0, 23);
      //minutes
      addFormNumericBox(F("Minute"), P253_Time_End_Minute_LABEL, P253_Time_End_Minute, 0, 59);

      // To add some html, which cannot be done in the existing functions, add it in the following way:
      // addHtml(F("<TR><TD>Analog Pin:<TD>"));



      // For strings, always use the F() macro, which stores the string in flash, not in memory.

      // String dropdown[5] = { F("option1"), F("option2"), F("option3"), F("option4")};
      // addFormSelector(string, F("drop-down menu"), F("plugin_xxx_displtype"), 4, dropdown, NULL, PCONFIG(0));

      // number selection (min-value - max-value)
      // addFormNumericBox(string, F("description"), F("plugin_xxx_description"), PCONFIG(1), min - value, max - value);

      // after the form has been loaded, set success and break
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      /*
      if isFormItemChecked(F("P253_opI"))
      {
        PCONFIG(1)=1;
      } else {
        PCONFIG(1)=0;
      }
      PCONFIG(0)      = isFormItemChecked({label}) ? 0 : 1;
      */

      P253_Time_Start_Hour = getFormItemInt(P253_Time_Start_Hour_LABEL);
      P253_Time_Start_Minute = getFormItemInt(P253_Time_Start_Minute_LABEL);
      P253_Time_End_Hour = getFormItemInt(P253_Time_End_Hour_LABEL);
      P253_Time_End_Minute = getFormItemInt(P253_Time_End_Minute_LABEL);
      P253_Debounce_Conf = getFormItemInt(P253_Debounce_Conf_LABEL);
      success = true;
      break;
    }
    case PLUGIN_INIT:
    {
      if (
          (CONFIG_PIN1 >= 0) && (CONFIG_PIN1 <= PIN_D_MAX) &&
          (CONFIG_PIN2 >= 0) && (CONFIG_PIN2 <= PIN_D_MAX)
         ) {
        portStatusStruct newStatus;
        const uint32_t   key = createKey(PLUGIN_ID_253, CONFIG_PIN1);

        // Read current status or create empty if it does not exist
        newStatus = globalMapPortStatus[key];

        // read and store current state to prevent switching at boot time
        // report it to host when do the _TEN_PER_SECOND
        // TODO do we want to set a config for the "send on boot"?
        newStatus.state = !(GPIO_Read_Switch_State(event));
        newStatus.output = newStatus.state;
        (newStatus.task < 3) ? newStatus.task++ : newStatus.task = 3; // add this GPIO/port as a task

        if (Settings.TaskDevicePin1PullUp[event->TaskIndex]) {
          setInternalGPIOPullupMode(CONFIG_PIN1);
          newStatus.mode = PIN_MODE_INPUT_PULLUP;
        } else {
          pinMode(CONFIG_PIN1, INPUT);
          newStatus.mode = PIN_MODE_INPUT;
        }

        savePortStatus(key, newStatus);

        pinMode(CONFIG_PIN2, OUTPUT);

        addLog(LOG_LEVEL_DEBUG, F("plugin init p253, config pin checked"));

        // set initial UserVar of the switch
        if (Settings.TaskDevicePin1Inversed[event->TaskIndex]) {
          UserVar[event->BaseVarIndex] = !newStatus.state;
        } else {
          UserVar[event->BaseVarIndex] = newStatus.state;
        }

        P253_Debounce_Timer = millis(); // debounce timer

      } else {
        addLog(LOG_LEVEL_DEBUG, F("plugin init p253, config pin incorrect"));
      }
      success = true;
      break;
    }

    case PLUGIN_READ:
    {
      // code to be executed to read data
      // It is executed according to the delay configured on the device configuration page, only once
      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        String log = F("P253 read : State ");
        log += UserVar[event->BaseVarIndex];
        addLog(LOG_LEVEL_INFO, log);
      }
      // after the plugin has read data successfuly, set success and break
      success = true;
      break;
    }

    case PLUGIN_WRITE:
    {
      String log;
      String command = parseString(string, 1);

      // WARNING: don't read "globalMapPortStatus[key]" here, as it will create a new entry if key does not exist
      log = String(F("P253 write, command: ")) + string;
      addLog(LOG_LEVEL_DEBUG, log);
      if (command == F("inputswitchstate")) {
        // do something
        success = true; // set to true only if plugin has executed a command successfully
      }

      break;
    }

    case PLUGIN_EXIT:
    {
      // perform cleanup tasks here. For example, free memory
      removeTaskFromPort(createKey(PLUGIN_ID_253, CONFIG_PIN1));
      removeTaskFromPort(createKey(PLUGIN_ID_253, CONFIG_PIN2));
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      if ((CONFIG_PIN1 >= 0) && (CONFIG_PIN1 <= PIN_D_MAX))
      {
        const uint32_t   key = createKey(PLUGIN_ID_001, CONFIG_PIN1);
        // WARNING operator [],creates an entry in map if key doesn't exist:
        portStatusStruct currentStatus = globalMapPortStatus[key];

        const int8_t state = GPIO_Read_Switch_State(CONFIG_PIN1,currentStatus.mode);

        if ((state != currentStatus.state) || currentStatus.forceEvent) {
          P253_His_G1_Status = state;
          addLog(LOG_LEVEL_DEBUG, F("plugin init p253, new switch state"));
          const unsigned long debounceTime = timePassedSince(P253_Debounce_Timer);
          if (debounceTime >= (unsigned long)lround(P253_Debounce_Conf)) {
            currentStatus.state = state;
            const boolean currentOutputState = currentStatus.output;
            boolean new_outputState          = currentOutputState;
            new_outputState = state;

              if (currentOutputState != new_outputState || currentStatus.forceEvent)
              {            
                byte output_value;
                currentStatus.output = new_outputState;
                boolean sendState = new_outputState;


                if (Settings.TaskDevicePin1Inversed[event->TaskIndex]) {
                  sendState = !sendState;
                }

                output_value = sendState ? 1 : 0;
                event->sensorType = Sensor_VType::SENSOR_TYPE_SWITCH;
                UserVar[event->BaseVarIndex] = output_value;
                if (loglevelActiveFor(LOG_LEVEL_INFO)) {
                  String log = F("SW  : GPIO=");
                  log += CONFIG_PIN1;
                  log += F(" State=");
                  log += state ? '1' : '0';
                  log += output_value == 3 ? F(" Doubleclick=") : F(" Output value=");
                  log += output_value;
                  addLog(LOG_LEVEL_INFO, log);
                }
                // send task event
                sendData(event);

                // check currect time vs time limit when set high
                // if() {
                //    if time is correct?
                //    GPIO_Internal_Write  
                //} else { // switch off
                //    GPIO_Internal_Write
                //}

                // send monitor event
                if (currentStatus.monitor) sendMonitorEvent(monitorEventString.c_str(), CONFIG_PIN1, output_value);
                // reset Userdata so it displays the correct state value in the web page
                UserVar[event->BaseVarIndex] = sendState ? 1 : 0;
              }
              PCONFIG_LONG(0) = millis();
          }
          // Reset forceEvent
          currentStatus.forceEvent = 0;
          savePortStatus(key, currentStatus);

        }

      }
      success = true;
    }
  } // switch
  return success;
}   // function

// implement plugin specific procedures and functions here
// void p253_do_sth_useful()
// {
//   // code
// }

#endif // USES_P253