#include "victron.h"
#include "esphome/core/log.h"

namespace esphome {
namespace victron {

static const char *const TAG = "victron";

void VictronComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Victron:");
  LOG_SENSOR("  ", "Max Power Yesterday", max_power_yesterday_sensor_);
  LOG_SENSOR("  ", "Max Power Today", max_power_today_sensor_);
  LOG_SENSOR("  ", "Yield Total", yield_total_sensor_);
  LOG_SENSOR("  ", "Yield Yesterday", yield_yesterday_sensor_);
  LOG_SENSOR("  ", "Yield Today", yield_today_sensor_);
  LOG_SENSOR("  ", "Panel Voltage", panel_voltage_sensor_);
  LOG_SENSOR("  ", "Panel Power", panel_power_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Current", battery_current_sensor_);
  LOG_SENSOR("  ", "Load Current", load_current_sensor_);
  LOG_SENSOR("  ", "Day Number", day_number_sensor_);
  LOG_SENSOR("  ", "Charging Mode ID", charging_mode_id_sensor_);
  LOG_SENSOR("  ", "Error Code", error_code_sensor_);
  LOG_SENSOR("  ", "Tracking Mode ID", tracking_mode_id_sensor_);
  LOG_TEXT_SENSOR("  ", "Charging Mode", charging_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Error Text", error_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Tracking Mode", tracking_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Firmware Version", firmware_version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Device Type", device_type_text_sensor_);

  // Smart shunt specific sensors
  LOG_SENSOR("  ", "Instantaneous power", instantaneous_power_sensor_);
  LOG_SENSOR("  ", "Time to go", time_to_go_sensor_);
  LOG_SENSOR("  ", "Consumed amp hours", consumed_amp_hours_sensor_);
  LOG_SENSOR("  ", "State of Charge", state_of_charge_sensor_);
  LOG_TEXT_SENSOR("  ", "BMV alarm", bmv_alarm_sensor_);
  LOG_TEXT_SENSOR("  ", "BMV pid", bmv_sensor_);
  LOG_SENSOR("  ", "Minimum main (battery) voltage", min_battery_voltage_sensor_);
  LOG_SENSOR("  ", "Maximum main (battery) voltage", max_battery_voltage_sensor_);
  LOG_SENSOR("  ", "Amount of charged energy", amount_of_charged_sensor_);
  LOG_SENSOR("  ", "Number of seconds since last full charge", last_full_charge_sensor_);
  LOG_SENSOR("  ", "Depth of the deepest discharge", depth_deepest_dis_sensor_);
  LOG_SENSOR("  ", "Depth of the last discharge", depth_of_the_last_discharge_sensor_);
  LOG_SENSOR("  ", "Amount of discharged energy", amount_of_discharged_energy_sensor_)

  check_uart_settings(19200);
}

void VictronComponent::loop() {
  const uint32_t now = millis();
  if ((state_ > 0) && (now - last_transmission_ >= 200)) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGW(TAG, "Last transmission too long ago.");
    state_ = 0;
  }

  if (!available())
    return;

  last_transmission_ = now;
  while (available()) {
    uint8_t c;
    read_byte(&c);
    if (state_ == 0) {
      if ((c == '\r') || (c == '\n'))
        continue;
      label_.clear();
      value_.clear();
      state_ = 1;
    }
    if (state_ == 1) {
      if (c == '\t')
        state_ = 2;
      else
        label_.push_back(c);
      continue;
    }
    if (state_ == 2) {
      if (label_ == "Checksum") {
        state_ = 0;
        continue;
      }
      if ((c == '\r') || (c == '\n')) {
        handle_value_();
        state_ = 0;
      } else {
        value_.push_back(c);
      }
    }
  }
}

static const __FlashStringHelper *charging_mode_text(int value) {
  switch (value) {
    case 0:
      return F("Off");
    case 2:
      return F("Fault");
    case 3:
      return F("Bulk");
    case 4:
      return F("Absorption");
    case 5:
      return F("Float");
    case 7:
      return F("Equalize (manual)");
    case 245:
      return F("Starting-up");
    case 247:
      return F("Auto equalize / Recondition");
    case 252:
      return F("External control");
    default:
      return F("Unknown");
  }
}

static const __FlashStringHelper *error_code_text(int value) {
  switch (value) {
    case 0:
      return F("No error");
    case 2:
      return F("Battery voltage too high");
    case 17:
      return F("Charger temperature too high");
    case 18:
      return F("Charger over current");
    case 19:
      return F("Charger current reversed");
    case 20:
      return F("Bulk time limit exceeded");
    case 21:
      return F("Current sensor issue");
    case 26:
      return F("Terminals overheated");
    case 28:
      return F("Converter issue");
    case 33:
      return F("Input voltage too high (solar panel)");
    case 34:
      return F("Input current too high (solar panel)");
    case 38:
      return F("Input shutdown (excessive battery voltage)");
    case 39:
      return F("Input shutdown (due to current flow during off mode)");
    case 65:
      return F("Lost communication with one of devices");
    case 66:
      return F("Synchronised charging device configuration issue");
    case 67:
      return F("BMS connection lost");
    case 68:
      return F("Network misconfigured");
    case 116:
      return F("Factory calibration data lost");
    case 117:
      return F("Invalid/incompatible firmware");
    case 119:
      return F("User settings invalid");
    default:
      return F("Unknown");
  }
}

static const std::string tracking_mode_text(int value) {
  switch (value) {
    case 0:
      return "Off";
    case 1:
      return "Limited";
    case 2:
      return "Active";
    default:
      return "Unknown";
  }
}

static const __FlashStringHelper *device_type_text(int value) {
  switch (value) {
    case 0x203:
      return F("BMV-700");
    case 0x204:
      return F("BMV-702");
    case 0x205:
      return F("BMV-700H");
    case 0xA389:
      return F("SmartShunt");
    case 0xA381:
      return F("BMV-712 Smart");
    case 0xA04C:
      return F("BlueSolar MPPT 75/10");
    case 0x300:
      return F("BlueSolar MPPT 70/15");
    case 0xA042:
      return F("BlueSolar MPPT 75/15");
    case 0xA043:
      return F("BlueSolar MPPT 100/15");
    case 0xA044:
      return F("BlueSolar MPPT 100/30 rev1");
    case 0xA04A:
      return F("BlueSolar MPPT 100/30 rev2");
    case 0xA041:
      return F("BlueSolar MPPT 150/35 rev1");
    case 0xA04B:
      return F("BlueSolar MPPT 150/35 rev2");
    case 0xA04D:
      return F("BlueSolar MPPT 150/45");
    case 0xA040:
      return F("BlueSolar MPPT 75/50");
    case 0xA045:
      return F("BlueSolar MPPT 100/50 rev1");
    case 0xA049:
      return F("BlueSolar MPPT 100/50 rev2");
    case 0xA04E:
      return F("BlueSolar MPPT 150/60");
    case 0xA046:
      return F("BlueSolar MPPT 150/70");
    case 0xA04F:
      return F("BlueSolar MPPT 150/85");
    case 0xA047:
      return F("BlueSolar MPPT 150/100");
    case 0xA050:
      return F("SmartSolar MPPT 250/100");
    case 0xA051:
      return F("SmartSolar MPPT 150/100");
    case 0xA052:
      return F("SmartSolar MPPT 150/85");
    case 0xA053:
      return F("SmartSolar MPPT 75/15");
    case 0xA054:
      return F("SmartSolar MPPT 75/10");
    case 0xA055:
      return F("SmartSolar MPPT 100/15");
    case 0xA056:
      return F("SmartSolar MPPT 100/30");
    case 0xA057:
      return F("SmartSolar MPPT 100/50");
    case 0xA058:
      return F("SmartSolar MPPT 150/35");
    case 0xA059:
      return F("SmartSolar MPPT 150/100 rev2");
    case 0xA05A:
      return F("SmartSolar MPPT 150/85 rev2");
    case 0xA05B:
      return F("SmartSolar MPPT 250/70");
    case 0xA05C:
      return F("SmartSolar MPPT 250/85");
    case 0xA05D:
      return F("SmartSolar MPPT 250/60");
    case 0xA05E:
      return F("SmartSolar MPPT 250/45");
    case 0xA05F:
      return F("SmartSolar MPPT 100/20");
    case 0xA060:
      return F("SmartSolar MPPT 100/20 48V");
    case 0xA061:
      return F("SmartSolar MPPT 150/45");
    case 0xA062:
      return F("SmartSolar MPPT 150/60");
    case 0xA063:
      return F("SmartSolar MPPT 150/70");
    case 0xA064:
      return F("SmartSolar MPPT 250/85 rev2");
    case 0xA065:
      return F("SmartSolar MPPT 250/100 rev2");
    case 0xA201:
      return F("Phoenix Inverter 12V 250VA 230V");
    case 0xA202:
      return F("Phoenix Inverter 24V 250VA 230V");
    case 0xA204:
      return F("Phoenix Inverter 48V 250VA 230V");
    case 0xA211:
      return F("Phoenix Inverter 12V 375VA 230V");
    case 0xA212:
      return F("Phoenix Inverter 24V 375VA 230V");
    case 0xA214:
      return F("Phoenix Inverter 48V 375VA 230V");
    case 0xA221:
      return F("Phoenix Inverter 12V 500VA 230V");
    case 0xA222:
      return F("Phoenix Inverter 24V 500VA 230V");
    case 0xA224:
      return F("Phoenix Inverter 48V 500VA 230V");
    case 0xA231:
      return F("Phoenix Inverter 12V 250VA 230V");
    case 0xA232:
      return F("Phoenix Inverter 24V 250VA 230V");
    case 0xA234:
      return F("Phoenix Inverter 48V 250VA 230V");
    case 0xA239:
      return F("Phoenix Inverter 12V 250VA 120V");
    case 0xA23A:
      return F("Phoenix Inverter 24V 250VA 120V");
    case 0xA23C:
      return F("Phoenix Inverter 48V 250VA 120V");
    case 0xA241:
      return F("Phoenix Inverter 12V 375VA 230V");
    case 0xA242:
      return F("Phoenix Inverter 24V 375VA 230V");
    case 0xA244:
      return F("Phoenix Inverter 48V 375VA 230V");
    case 0xA249:
      return F("Phoenix Inverter 12V 375VA 120V");
    case 0xA24A:
      return F("Phoenix Inverter 24V 375VA 120V");
    case 0xA24C:
      return F("Phoenix Inverter 48V 375VA 120V");
    case 0xA251:
      return F("Phoenix Inverter 12V 500VA 230V");
    case 0xA252:
      return F("Phoenix Inverter 24V 500VA 230V");
    case 0xA254:
      return F("Phoenix Inverter 48V 500VA 230V");
    case 0xA259:
      return F("Phoenix Inverter 12V 500VA 120V");
    case 0xA25A:
      return F("Phoenix Inverter 24V 500VA 120V");
    case 0xA25C:
      return F("Phoenix Inverter 48V 500VA 120V");
    case 0xA261:
      return F("Phoenix Inverter 12V 800VA 230V");
    case 0xA262:
      return F("Phoenix Inverter 24V 800VA 230V");
    case 0xA264:
      return F("Phoenix Inverter 48V 800VA 230V");
    case 0xA269:
      return F("Phoenix Inverter 12V 800VA 120V");
    case 0xA26A:
      return F("Phoenix Inverter 24V 800VA 120V");
    case 0xA26C:
      return F("Phoenix Inverter 48V 800VA 120V");
    case 0xA271:
      return F("Phoenix Inverter 12V 1200VA 230V");
    case 0xA272:
      return F("Phoenix Inverter 24V 1200VA 230V");
    case 0xA274:
      return F("Phoenix Inverter 48V 1200VA 230V");
    case 0xA279:
      return F("Phoenix Inverter 12V 1200VA 120V");
    case 0xA27A:
      return F("Phoenix Inverter 24V 1200VA 120V");
    case 0xA27C:
      return F("Phoenix Inverter 48V 1200VA 120V");
    default:
      return nullptr;
  }
}

static std::string flash_to_string(const __FlashStringHelper *flash) {
  std::string result;
  const char *fptr = (PGM_P) flash;
  result.reserve(strlen_P(fptr));
  char c;
  while ((c = pgm_read_byte(fptr++)) != 0)
    result.push_back(c);
  return result;
}

void VictronComponent::handle_value_() {
  int value;

  // See page 5-7 of VE.Direct-Protocol-3.32.pdf
  //
  // Label      Units   Description                            BMV60x   BMV70x   BMV71x    MPPT    Phoenix Inverter    Phoenix Charger
  //
  // V             mV   Main or channel 1 (battery) voltage       x        x        x        x            x                   x
  // V2            mV   Channel 2 (battery) voltage                                                                           x
  // V3            mV   Channel 3 (battery) voltage                                                                           x
  // VS            mV   Auxiliary (starter) voltage               x        x        x
  // VM            mV   Mid-point voltage of the battery bank              x        x
  // DM            %o   Mid-point deviation of the battery bank            x        x
  // VPV           mV   Panel voltage                                                        x
  // PPV            W   Panel power                                                          x
  // I             mA   Main or channel 1 battery current         x        x        x        x                                x
  // I2            mA   Channel 2 battery current                                                                             x
  // I3            mA   Channel 3 battery current                                                                             x
  // IL            mA   Load current                                                         x
  // LOAD               Load output state (ON/OFF)                                           x
  // T             °C   Battery temperature                                x        x
  // P              W   Instantaneous power                                x        x
  // CE           mAh   Consumed Amp Hours                        x        x        x
  //
  // SOC           ‰o   State-of-charge                           x        x        x
  // TTG          min   Time-to-go                                x        x        x
  // Alarm              Alarm condition active                    x        x        x
  // Relay              Relay state                               x        x        x        x            x                   x
  // AR                 Alarm reason                              x        x        x                     x
  // OR                 Off reason                                                           x            x
  // H1           mAh   Depth of the deepest discharge            x        x        x
  // H2           mAh   Depth of the last discharge               x        x        x
  // H3           mAh   Depth of the average discharge            x        x        x
  // H4                 Number of charge cycles                   x        x        x
  // H5                 Number of full discharges                 x        x        x
  // H6           mAh   Cumulative Amp Hours drawn                x        x        x
  // H7            mV   Minimum main (battery) voltage            x        x        x
  // H8            mV   Maximum main (battery) voltage            x        x        x
  // H9           sec   Number of seconds since last full charge  x        x        x
  // H10                Number of automatic synchronizations      x        x        x
  // H11                Number of low main voltage alarms         x        x        x
  // H12                Number of high main voltage alarms        x        x        x
  // H13                Number of low auxiliary voltage alarms    x
  // H14                Number of high auxiliary voltage alarms   x
  // H15           mV   Minimum auxiliary (battery) voltage       x        x        x
  // H16           mV   Maximum auxiliary (battery) voltage       x        x        x
  // H1    7 0.01 kWh   Amount of discharged energy (BMV) /                x        x
  //                    Amount of produced energy (DC monitor)

  // H18     0.01 kWh   Amount of charged energy (BMV) /                   x        x
  //                    Amount of consumed energy (DC monitor)
  // H19     0.01 kWh   Yield total (user resettable counter)                                x
  // H20     0.01 kWh   Yield today                                                          x
  // H21            W   Maximum power today                                                  x
  // H22     0.01 kWh   Yield yesterday                                                      x
  // H23            W   Maximum power yesterday                                              x
  // ERR                Error code                                                           x                                x
  // CS                 State of operation                                                   x            x                   x
  // BMV                Model description (deprecated)             x       x        x
  // FW                 Firmware version (16 bit)                  x       x        x        x            x
  // FWE                Firmware version (24 bit)                                            x
  // PID                Product ID                                         x        x        x            x                   x
  // SER#               Serial number                                                        x            x                   x
  // HSDS               Day sequence number (0..364)                                         x
  // MODE               Device mode                                                                       x                   x
  // AC_OUT_V  0.01 V   AC output voltage                                                                 x
  // AC_OUT_I   0.1 A   AC output current                                                                 x
  // AC_OUT_S      VA   AC output apparent power                                                          x
  // WARN               Warning reason                                                                    x
  // MPPT               Tracker operation mode                                               x
  // MON                DC monitor mode                                             x

  if (label_ == "H9") {
    if (last_full_charge_sensor_ != nullptr)
      last_full_charge_sensor_->publish_state(atoi(value_.c_str()) / 60);  // sec -> min, NOLINT(cert-err34-c)
  } else if (label_ == "H1") {
    if (depth_deepest_dis_sensor_ != nullptr)
      depth_deepest_dis_sensor_->publish_state(atoi(value_.c_str()) / 1000.0);  // mAh -> Ah, NOLINT(cert-err34-c)
  } else if (label_ == "H2") {
    if (depth_of_the_last_discharge_sensor_ != nullptr)
      depth_of_the_last_discharge_sensor_->publish_state(atoi(value_.c_str()) /
                                                         1000.0);  // mAh -> Ah, NOLINT(cert-err34-c)
  } else if (label_ == "H17") {
    if (amount_of_discharged_energy_sensor_ != nullptr)
      amount_of_discharged_energy_sensor_->publish_state(atoi(value_.c_str()) * 10.00);  // Wh, NOLINT(cert-err34-c)
  } else if (label_ == "H18") {
    if (amount_of_charged_sensor_ != nullptr)
      amount_of_charged_sensor_->publish_state(atoi(value_.c_str()) * 10.00);  // Wh, NOLINT(cert-err34-c)
  } else if (label_ == "BMV") {
    if (bmv_sensor_ != nullptr)
      bmv_sensor_->publish_state(value_);
  } else if (label_ == "Alarm") {
    if (bmv_alarm_sensor_ != nullptr)
      bmv_alarm_sensor_->publish_state(value_);
  } else if (label_ == "H7") {
    if (min_battery_voltage_sensor_ != nullptr)
      min_battery_voltage_sensor_->publish_state(atoi(value_.c_str()) / 1000.00);  // mV to V, NOLINT(cert-err34-c)
  } else if (label_ == "H8") {
    if (max_battery_voltage_sensor_ != nullptr)
      max_battery_voltage_sensor_->publish_state(atoi(value_.c_str()) / 1000.00);  // mV to V, NOLINT(cert-err34-c)
  } else if (label_ == "H18") {
    if (amount_of_charged_sensor_ != nullptr)
      amount_of_charged_sensor_->publish_state(atoi(value_.c_str()));  // Wh, NOLINT(cert-err34-c)
  } else if (label_ == "TTG") {
    if (time_to_go_sensor_ != nullptr)
      time_to_go_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "SOC") {
    if (state_of_charge_sensor_ != nullptr)
      state_of_charge_sensor_->publish_state(atoi(value_.c_str()) * 0.10);  // promiles to %, NOLINT(cert-err34-c)
  } else if (label_ == "CE") {
    if (consumed_amp_hours_sensor_ != nullptr)
      consumed_amp_hours_sensor_->publish_state(atoi(value_.c_str()) / 1000.00);  // NOLINT(cert-err34-c)
  } else if (label_ == "P") {
    if (instantaneous_power_sensor_ != nullptr)
      instantaneous_power_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "H23") {
    if (max_power_yesterday_sensor_ != nullptr)
      max_power_yesterday_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "H21") {
    if (max_power_today_sensor_ != nullptr)
      max_power_today_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "H19") {
    if (yield_total_sensor_ != nullptr)
      yield_total_sensor_->publish_state(atoi(value_.c_str()) * 10);  // NOLINT(cert-err34-c)
  } else if (label_ == "H22") {
    if (yield_yesterday_sensor_ != nullptr)
      yield_yesterday_sensor_->publish_state(atoi(value_.c_str()) * 10);  // NOLINT(cert-err34-c)
  } else if (label_ == "H20") {
    if (yield_today_sensor_ != nullptr)
      yield_today_sensor_->publish_state(atoi(value_.c_str()) * 10);  // NOLINT(cert-err34-c)
  } else if (label_ == "VPV") {
    if (panel_voltage_sensor_ != nullptr)
      panel_voltage_sensor_->publish_state(atoi(value_.c_str()) / 1000.0);  // NOLINT(cert-err34-c)
  } else if (label_ == "PPV") {
    if (panel_power_sensor_ != nullptr)
      panel_power_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "V") {
    if (battery_voltage_sensor_ != nullptr)
      battery_voltage_sensor_->publish_state(atoi(value_.c_str()) / 1000.0);  // NOLINT(cert-err34-c)
  } else if (label_ == "I") {
    if (battery_current_sensor_ != nullptr)
      battery_current_sensor_->publish_state(atoi(value_.c_str()) / 1000.0);  // NOLINT(cert-err34-c)
  } else if (label_ == "IL") {
    if (load_current_sensor_ != nullptr)
      load_current_sensor_->publish_state(atoi(value_.c_str()) / 1000.0);  // NOLINT(cert-err34-c)
  } else if (label_ == "HSDS") {
    if (day_number_sensor_ != nullptr)
      day_number_sensor_->publish_state(atoi(value_.c_str()));  // NOLINT(cert-err34-c)
  } else if (label_ == "CS") {
    value = atoi(value_.c_str());  // NOLINT(cert-err34-c)
    if (charging_mode_id_sensor_ != nullptr)
      charging_mode_id_sensor_->publish_state((float) value);
    if (charging_mode_text_sensor_ != nullptr)
      charging_mode_text_sensor_->publish_state(flash_to_string(charging_mode_text(value)));
  } else if (label_ == "ERR") {
    value = atoi(value_.c_str());  // NOLINT(cert-err34-c)
    if (error_code_sensor_ != nullptr)
      error_code_sensor_->publish_state(value);
    if (error_text_sensor_ != nullptr)
      error_text_sensor_->publish_state(flash_to_string(error_code_text(value)));
  } else if (label_ == "MPPT") {
    value = atoi(value_.c_str());  // NOLINT(cert-err34-c)
    if (tracking_mode_id_sensor_ != nullptr)
      tracking_mode_id_sensor_->publish_state((float) value);
    if (tracking_mode_text_sensor_ != nullptr)
      tracking_mode_text_sensor_->publish_state(tracking_mode_text(value));
  } else if (label_ == "FW") {
    if ((firmware_version_text_sensor_ != nullptr) && !firmware_version_text_sensor_->has_state())
      firmware_version_text_sensor_->publish_state(value_.insert(value_.size() - 2, "."));
  } else if (label_ == "PID") {
    // value = atoi(value_.c_str());

    // ESP_LOGD(TAG, "received PID: '%s'", value_.c_str());
    value = strtol(value_.c_str(), nullptr, 0);
    // ESP_LOGD(TAG, "received PID: '%04x'", value);
    if ((device_type_text_sensor_ != nullptr) && !device_type_text_sensor_->has_state()) {
      const __FlashStringHelper *flash = device_type_text(value);
      if (flash != nullptr) {
        device_type_text_sensor_->publish_state(flash_to_string(flash));
      }  // else {
         // char s[30];
         // snprintf(s, 30, "Unknown device (%04x)", value);
         // device_type_text_sensor_->publish_state(s);
      //}
    }
  } else {
    ESP_LOGW(TAG, "Unsupported message received: Key '%s' with value '%s'", label_.c_str(), value_.c_str());
  }
}

}  // namespace victron
}  // namespace esphome
