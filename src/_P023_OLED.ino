#include "_Plugin_Helper.h"

#ifdef USES_P023

// #######################################################################################################
// #################################### Plugin 023: OLED SSD1306 display #################################
// #######################################################################################################


#include "src/PluginStructs/P023_data_struct.h"

// Sample templates
//  Temp: [DHT11#Temperature]   Hum:[DHT11#humidity]
//  DS Temp:[Dallas1#Temperature#R]
//  Lux:[Lux#Lux#R]
//  Baro:[Baro#Pressure#R]

#define PLUGIN_023
#define PLUGIN_ID_023         23
#define PLUGIN_NAME_023       "Display - OLED SSD1306"
#define PLUGIN_VALUENAME1_023 "OLED"

boolean Plugin_023(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number           = PLUGIN_ID_023;
      Device[deviceCount].Type               = DEVICE_TYPE_I2C;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_NONE;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = false;
      Device[deviceCount].ValueCount         = 0;
      Device[deviceCount].SendDataOption     = false;
      Device[deviceCount].TimerOption        = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_023);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_023));
      break;
    }

    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    {
      byte choice = PCONFIG(0);

      /*String options[2] = { F("3C"), F("3D") };*/
      int optionValues[2] = { 0x3C, 0x3D };
      addFormSelectorI2C(F("p023_adr"), 2, optionValues, choice);
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {
      {
        byte choice2         = PCONFIG(1);
        String options2[2]   = { F("Normal"), F("Rotated") };
        int optionValues2[2] = { 1, 2 };
        addFormSelector(F("Rotation"), F("p023_rotate"), 2, options2, optionValues2, choice2);
      }
      {
        byte   choice3          = PCONFIG(3);
        String options3[3]      = { F("128x64"), F("128x32"), F("64x48") };
        int    optionValues3[3] = { 1, 3, 2 };
        addFormSelector(F("Display Size"), F("p023_size"), 3, options3, optionValues3, choice3);
      }
      {
        byte   choice4          = PCONFIG(4);
        String options4[2]      = { F("Normal"), F("Optimized") };
        int    optionValues4[2] = { 1, 2 };
        addFormSelector(F("Font Width"), F("p023_font_spacing"), 2, options4, optionValues4, choice4);
      }
      {
        String strings[P23_Nlines];
        LoadCustomTaskSettings(event->TaskIndex, strings, P23_Nlines, P23_Nchars);

        for (byte varNr = 0; varNr < 8; varNr++)
        {
          addFormTextBox(String(F("Line ")) + (varNr + 1), getPluginCustomArgName(varNr), strings[varNr], 64);
        }
      }

      // FIXME TD-er: Why is this using pin3 and not pin1? And why isn't this using the normal pin selection functions?
      addFormPinSelect(F("Display button"), F("taskdevicepin3"), CONFIG_PIN3);

      addFormNumericBox(F("Display Timeout"), F("plugin_23_timer"), PCONFIG(2));

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      PCONFIG(0) = getFormItemInt(F("p023_adr"));
      PCONFIG(1) = getFormItemInt(F("p023_rotate"));
      PCONFIG(2) = getFormItemInt(F("plugin_23_timer"));
      PCONFIG(3) = getFormItemInt(F("p023_size"));
      PCONFIG(4) = getFormItemInt(F("p023_font_spacing"));

      // FIXME TD-er: This is a huge stack allocated object.
      char   deviceTemplate[P23_Nlines][P23_Nchars];
      String error;

      for (byte varNr = 0; varNr < P23_Nlines; varNr++)
      {
        if (!safe_strncpy(deviceTemplate[varNr], web_server.arg(getPluginCustomArgName(varNr)), P23_Nchars)) {
          error += getCustomTaskSettingsError(varNr);
        }
      }

      if (error.length() > 0) {
        addHtmlError(error);
      }
      SaveCustomTaskSettings(event->TaskIndex, (byte *)&deviceTemplate, sizeof(deviceTemplate));
      success = true;
      break;
    }

    case PLUGIN_INIT:
    {
      byte address                           = PCONFIG(0);
      byte type                              = 0;
      P023_data_struct::Spacing font_spacing = P023_data_struct::Spacing::normal;
      byte displayTimer                      = PCONFIG(2);

      switch (PCONFIG(3)) {
        case 1:
          // 128x64
          type = 0;
          break;
        case 2:
          type = P023_data_struct::OLED_64x48;
          break;
        case 3:
          type = P023_data_struct::OLED_128x32;
          break;
      }

      if (PCONFIG(1) == 2)
      {
        type |= P023_data_struct::OLED_rotated;
      }

      switch (static_cast<P023_data_struct::Spacing>(PCONFIG(4))) {
        case P023_data_struct::Spacing::normal:
        case P023_data_struct::Spacing::optimized:
          font_spacing = static_cast<P023_data_struct::Spacing>(PCONFIG(4));
          break;
      }

      initPluginTaskData(event->TaskIndex, new (std::nothrow) P023_data_struct(address, type, font_spacing, displayTimer));
      P023_data_struct *P023_data =
        static_cast<P023_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P023_data) {
        P023_data->StartUp_OLED();
        P023_data->clearDisplay();

        if (PCONFIG(1) == 2)
        {
          P023_data->sendCommand(0xA0 | 0x1); // SEGREMAP   //Rotate screen 180 deg
          P023_data->sendCommand(0xC8);       // COMSCANDEC  Rotate screen 180 Deg
        }

        P023_data->sendStrXY("ESP Easy ", 0, 0);

        if (CONFIG_PIN3 != -1) {
          pinMode(CONFIG_PIN3, INPUT_PULLUP);
        }
        success = true;
      }
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      if (CONFIG_PIN3 != -1)
      {
        if (!digitalRead(CONFIG_PIN3))
        {
          P023_data_struct *P023_data =
            static_cast<P023_data_struct *>(getPluginTaskData(event->TaskIndex));

          if (nullptr != P023_data) {
            P023_data->setDisplayTimer(PCONFIG(2));
          }
        }
      }
      break;
    }

    case PLUGIN_ONCE_A_SECOND:
    {
      P023_data_struct *P023_data =
        static_cast<P023_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P023_data) {
        P023_data->checkDisplayTimer();
      }
      break;
    }

    case PLUGIN_READ:
    {
      P023_data_struct *P023_data =
        static_cast<P023_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P023_data) {
        String strings[P23_Nlines];
        LoadCustomTaskSettings(event->TaskIndex, strings, P23_Nlines, P23_Nchars);

        for (byte x = 0; x < 8; x++)
        {
          if (strings[x].length())
          {
            String newString = P023_data->parseTemplate(strings[x], 16);
            P023_data->sendStrXY(newString.c_str(), x, 0);
          }
        }
      }
      success = false;
      break;
    }

    case PLUGIN_WRITE:
    {
      P023_data_struct *P023_data =
        static_cast<P023_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr != P023_data) {
        String arguments = String(string);

        // Fixed bug #1864
        // this was to manage multiple instances of the plug-in.
        // You can also call it this way:
        // [TaskName].OLED, 1,1, Temp. is 19.9
        int dotPos = arguments.indexOf('.');

        if ((dotPos > -1) && arguments.substring(dotPos, dotPos + 4).equalsIgnoreCase(F("oled")))
        {
          LoadTaskSettings(event->TaskIndex);
          String name = arguments.substring(0, dotPos);
          name.replace("[", "");
          name.replace("]", "");

          if (name.equalsIgnoreCase(getTaskDeviceName(event->TaskIndex)) == true)
          {
            arguments = arguments.substring(dotPos + 1);
          }
          else
          {
            return false;
          }
        }

        // We now continue using 'arguments' and not 'string' as full command line.
        // If there was any prefix to address a specific task, it is now removed from 'arguments'
        String cmd = parseString(arguments, 1);

        if (cmd.equalsIgnoreCase(F("OLEDCMD")))
        {
          success = true;
          String param = parseString(arguments, 2);

          if (param.equalsIgnoreCase(F("Off"))) {
            P023_data->displayOff();
          }
          else if (param.equalsIgnoreCase(F("On"))) {
            P023_data->displayOn();
          }
          else if (param.equalsIgnoreCase(F("Clear"))) {
            P023_data->clearDisplay();
          }
        }
        else if (cmd.equalsIgnoreCase(F("OLED")))
        {
          success = true;
          String text = parseStringToEndKeepCase(arguments, 4);
          text = P023_data->parseTemplate(text, 16);
          P023_data->sendStrXY(text.c_str(), event->Par1 - 1, event->Par2 - 1);
        }
      }
      break;
    }
  }
  return success;
}

#endif // USES_P023
