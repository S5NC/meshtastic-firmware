/**
 * @file ExternalNotificationModule.cpp
 * @brief Implementation of the ExternalNotificationModule class.
 *
 * This file contains the implementation of the ExternalNotificationModule class, which is responsible for handling external
 * notifications such as vibration, buzzer, and LED lights. The class provides methods to turn on and off the external
 * notification outputs and to play ringtones using PWM buzzer. It also includes default configurations and a runOnce() method to
 * handle the module's behavior.
 *
 * Documentation:
 * https://meshtastic.org/docs/configuration/module/external-notification
 *
 * @author Jm Casler & Meshtastic Team
 * @date [Insert Date]
 */
#include "ExternalNotificationModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "buzz/buzz.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/rtttl.pb.h"
#include <Arduino.h>

#include "main.h"

#ifdef HAS_NCP5623
#include <graphics/RAKled.h>

uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t colorState = 1;
uint8_t brightnessIndex = 0;
uint8_t brightnessValues[] = {0, 10, 20, 30, 50, 90, 160, 170}; // blue gets multiplied by 1.5
bool ascending = true;
#endif

enum Peripheral {
    SIGNAL,
    BUZZER,
    VIBRA
};

#ifndef PIN_BUZZER
#define PIN_BUZZER 0 // FIXME: default to -1 not 0?
#endif

/*
    Documentation:
        https://meshtastic.org/docs/configuration/module/external-notification
*/

// Default configurations
#ifdef EXT_NOTIFY_OUT
#define EXT_NOTIFICATION_MODULE_OUTPUT EXT_NOTIFY_OUT
#else
#define EXT_NOTIFICATION_MODULE_OUTPUT 0 // FIXME: use -1 for unset pin (like RadioLib)
#endif
#define EXT_NOTIFICATION_MODULE_OUTPUT_MS 1000

#define ASCII_BELL 0x07

meshtastic_RTTTLConfig rtttlConfig;

ExternalNotificationModule *externalNotificationModule;

bool peripheralState[3] = {}; // the state of the notification signal pin, buzzer, and vibramotor
uint32_t peripheralStateLastChanged[3] = {}; // when the state of the notification signal pin, buzzer, and vibramotor were last changed

static const char *rtttlConfigFile = "/prefs/ringtone.proto";

int32_t ExternalNotificationModule::runOnce()
{
    if (!moduleConfig.external_notification.enabled) {
        return INT32_MAX; // we don't need this thread here...
    } else {
        // let the song finish if we reach timeout by only stopping external notifications if RTTTL has also stopped playing
        if ((millis() > nagCycleCutoff) && !rtttl::isPlaying()) {
            nagCycleCutoff = UINT32_MAX;
            LOG_INFO("Turning off external notification: ");
            for (Peripheral peripheral : {SIGNAL, VIBRA}) { // The buzzer has already stopped so we don't need to turn it off again
                setPeripheralOff(peripheral);
                LOG_INFO("%d ", peripheral);
            }
            LOG_INFO("\n");
            isNagging = false;
            return INT32_MAX; // save cycles till we're needed again
        }

        // If the output is turned on, turn it back off after the given period of time.
        if (isNagging) {
            // Invert the state of every external peripheral
            for (Peripheral peripheral : {SIGNAL, BUZZER, VIBRA}) {
                if (millis() > peripheralStateLastChanged[peripheral] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms : EXT_NOTIFICATION_MODULE_OUTPUT_MS)) {
                    getPeripheralState(peripheral) ? setPeripheralOff(peripheral) : setPeripheralOn(peripheral);
                }
            }

#ifdef HAS_NCP5623
            if (rgb_found.type == ScanI2C::NCP5623) {
                red = (colorState & 4) ? brightnessValues[brightnessIndex] : 0;          // Red enabled on colorState = 4,5,6,7
                green = (colorState & 2) ? brightnessValues[brightnessIndex] : 0;        // Green enabled on colorState = 2,3,6,7
                blue = (colorState & 1) ? (brightnessValues[brightnessIndex] * 1.5) : 0; // Blue enabled on colorState = 1,3,5,7
                rgb.setColor(red, green, blue);

                if (ascending) { // fade in
                    brightnessIndex++;
                    if (brightnessIndex == (sizeof(brightnessValues) - 1)) {
                        ascending = false;
                    }
                } else {
                    brightnessIndex--; // fade out
                }
                if (brightnessIndex == 0) {
                    ascending = true;
                    colorState++; // next color
                    if (colorState > 7) {
                        colorState = 1;
                    }
                }
            }
#endif

#ifdef T_WATCH_S3
            drv.go();
#endif
        }

        if (moduleConfig.external_notification.use_pwm) {
            // Let the PWM buzzer play on if it's set to be playing
            if (rtttl::isPlaying()) {
                rtttl::play();
            }
            // If we aren't playing and are still within the nag window, play the ringtone again
            else if (isNagging && (millis() < nagCycleCutoff)) {
                rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
            }
        }

        return 25; // FIXME: communicate with rtttl to find which pause length is best to maintain optimal ringtone playback, and find the minimum with another value (like perhaps 25)
    }
}

bool ExternalNotificationModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

/**
 * Sets the external notification on for the specified index.
 *
 * @param index The index of the external notification to turn on.
 */
// Turn the specified peripheral on if it is configured correctly. For SIGNAL, set it to its active state
void ExternalNotificationModule::setPeripheralOn(Peripheral peripheral)
{
    peripheralState[peripheral] = 1; // Record that it's on
    peripheralStateLastChanged[peripheral] = millis(); // Record when it got turned on

    switch (peripheral) {
    case BUZZER:
        if (moduleConfig.external_notification.output_buzzer) // FIXME: change unset value to -1
            digitalWrite(moduleConfig.external_notification.output_buzzer, true);
        break;
    case VIBRA:
        if (moduleConfig.external_notification.output_vibra) // FIXME: change unset value to -1
            digitalWrite(moduleConfig.external_notification.output_vibra, true);
        break;
    case SIGNAL:
        if (output > 0) // FIXME: change unset value to -1
            digitalWrite(output, (moduleConfig.external_notification.active ? true : false));
        break;
    default: // Not a known peripheral
        // FIXME: raise / output error
        break;
    }
#ifdef HAS_NCP5623
    if (rgb_found.type == ScanI2C::NCP5623) {
        rgb.setColor(red, green, blue);
    }
#endif
#ifdef T_WATCH_S3
    drv.go();
#endif
}

// Turn the specified peripheral off. For SIGNAL, set it to its inactive state
void ExternalNotificationModule::setPeripheralOff(Peripheral peripheral)
{
    peripheralState[peripheral] = 0; // Record that it's off
    peripheralStateLastChanged[peripheral] = millis(); // Record when it got turned off

    switch (peripheral) {
    case SIGNAL:
        if (output > 0) // FIXME: change unset value to -1
            digitalWrite(output, (moduleConfig.external_notification.active ? false : true));
        break;
    case BUZZER:
        if (moduleConfig.external_notification.output_buzzer) // FIXME: change unset value to -1
            digitalWrite(moduleConfig.external_notification.output_buzzer, false);
        break;
    case VIBRA:
        if (moduleConfig.external_notification.output_vibra) // FIXME: change unset value to -1
            digitalWrite(moduleConfig.external_notification.output_vibra, false);
        break;
    default: // Not a known peripheral
        // FIXME: raise / output error
        break;
    }

#ifdef HAS_NCP5623
    if (rgb_found.type == ScanI2C::NCP5623) {
        red = 0;
        green = 0;
        blue = 0;
        rgb.setColor(red, green, blue);
    }
#endif
#ifdef T_WATCH_S3
    drv.stop();
#endif
}

// Get the state of the provided peripheral
bool ExternalNotificationModule::getPeripheralState(Peripheral peripheral)
{
    return peripheralState[peripheral];
}

void ExternalNotificationModule::stopNow()
{
    rtttl::stop();
    isNagging = false;
    nagCycleCutoff = 1; // small value // FIXME: why not 0? Is it even needed to set this if isNagging is set to false and is always checked?
    setIntervalFromNow(0);
#ifdef T_WATCH_S3
    drv.stop();
#endif
}

ExternalNotificationModule::ExternalNotificationModule()
    : SinglePortModule("ExternalNotificationModule", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("ExternalNotificationModule")
{
    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */

    // moduleConfig.external_notification.alert_message = true;
    // moduleConfig.external_notification.alert_message_buzzer = true;
    // moduleConfig.external_notification.alert_message_vibra = true;

    // moduleConfig.external_notification.active = true;
    // moduleConfig.external_notification.alert_bell = 1;
    // moduleConfig.external_notification.output_ms = 1000;
    // moduleConfig.external_notification.output = 4; // RAK4631 IO4
    // moduleConfig.external_notification.output_buzzer = 10; // RAK4631 IO6
    // moduleConfig.external_notification.output_vibra = 28; // RAK4631 IO7
    // moduleConfig.external_notification.nag_timeout = 300;

    if (moduleConfig.external_notification.enabled) {
        // Load the ringtone, if it fails to load, use the default one below
        if (!nodeDB.loadProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, sizeof(meshtastic_RTTTLConfig),
                              &meshtastic_RTTTLConfig_msg, &rtttlConfig)) {
            memset(rtttlConfig.ringtone, 0, sizeof(rtttlConfig.ringtone));
            strncpy(rtttlConfig.ringtone,
                    "a:d=8,o=5,b=125:4d#6,a#,2d#6,16p,g#,4a#,4d#.,p,16g,16a#,d#6,a#,f6,2d#6,16p,c#.6,16c6,16a#,g#.,2a#",
                    sizeof(rtttlConfig.ringtone));
        }

        LOG_INFO("Initializing External Notification Module\n");
        
        // FIXME: use -1 for moduleConfig.external_notification.output for unset pin
        // Decide which pin to use as the external notification signal
        output = moduleConfig.external_notification.output ? moduleConfig.external_notification.output
                                                           : EXT_NOTIFICATION_MODULE_OUTPUT;

        // If that pin is valid, configure it as an output
        if (output > 0) { // FIXME: change unset value to -1
            LOG_INFO("Using Pin %i in digital mode\n", output);
            pinMode(output, OUTPUT);
        }
        setPeripheralOff(SIGNAL);

        // If using a buzzer, configure it
        if (moduleConfig.external_notification.output_buzzer) {
            // FIXME: we should either change the `config.` value throughout, or not change it and change our own local copy like we do for `output`
            // FIXME: move the `config.device.buzzer_gpio = config.device.buzzer_gpio ? config.device.buzzer_gpio : PIN_BUZZER;` to here and do it for both active and PWM buzzers, but need to check if there's a reason the variant file value isn't checked for PWM.
            // if configured to treat the buzzer as an active buzzer
            if (!moduleConfig.external_notification.use_pwm) {
                LOG_INFO("Using Pin %i for buzzer\n", moduleConfig.external_notification.output_buzzer);
                pinMode(moduleConfig.external_notification.output_buzzer, OUTPUT);
                setPeripheralOff(BUZZER);
            }
            // otherwise we are configured to treat the buzzer as a PWM (passive) buzzer
            else {
                config.device.buzzer_gpio = config.device.buzzer_gpio ? config.device.buzzer_gpio : PIN_BUZZER;
                // in PWM Mode we force the buzzer pin if it is set
                LOG_INFO("Using Pin %i in PWM mode\n", config.device.buzzer_gpio);
            }
        }

        // If using a vibramotor, configure it
        if (moduleConfig.external_notification.output_vibra) {
            LOG_INFO("Using Pin %i for vibra motor\n", moduleConfig.external_notification.output_vibra);
            pinMode(moduleConfig.external_notification.output_vibra, OUTPUT);
            setPeripheralOff(VIBRA);
        }
#ifdef HAS_NCP5623
        if (rgb_found.type == ScanI2C::NCP5623) {
            rgb.begin();
            rgb.setCurrent(10);
        }
#endif
    } else {
        LOG_INFO("External Notification Module Disabled\n");
        disable();
    }
}

ProcessMessage ExternalNotificationModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (moduleConfig.external_notification.enabled) {
#if T_WATCH_S3
        drv.setWaveform(0, 75);
        drv.setWaveform(1, 56);
        drv.setWaveform(2, 0);
        drv.go();
#endif
        // If the message is from someone else (not from ourselves)
        if (getFrom(&mp) != nodeDB.getNodeNum()) {

            // New message is received!

            if (moduleConfig.external_notification.alert_message) {
                LOG_INFO("externalNotificationModule - Notification Module\n");
                isNagging = true;
                setPeripheralOn(SIGNAL);
                // FIXME: completely separate signal pin duration, and buzzer/vibra nag duration.
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_buzzer) {
                LOG_INFO("externalNotificationModule - Notification Module (Buzzer)\n");
                isNagging = true;
                if (!moduleConfig.external_notification.use_pwm) {
                    setPeripheralOn(BUZZER);
                } else {
                    rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
                }
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_vibra) {
                LOG_INFO("externalNotificationModule - Notification Module (Vibra)\n");
                isNagging = true;
                setPeripheralOn(VIBRA);
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            // Check if the message contains a bell character
            auto &p = mp.decoded;
            bool containsBell = false;
            for (int i = 0; i < p.payload.size; i++) {
                if (p.payload.bytes[i] == ASCII_BELL) {
                    containsBell = true;
                }
            }

            if (containsBell) {

                // if set to trigger the external notification signal pin when there's a bell, if there was a bell, trigger it
                if (moduleConfig.external_notification.alert_bell) {
                    LOG_INFO("externalNotificationModule - Notification Bell\n");
                    isNagging = true;
                    setPeripheralOn(SIGNAL);
                    // FIXME: completely separate signal pin duration, and buzzer/vibra nag duration.
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }

                if (moduleConfig.external_notification.alert_bell_buzzer) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Buzzer)\n");
                    isNagging = true;
                    if (!moduleConfig.external_notification.use_pwm) {
                        setPeripheralOn(BUZZER);
                    } else {
                        rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
                    }
                    // FIXME: completely separate signal pin duration, and buzzer/vibra nag duration.
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }

                if (moduleConfig.external_notification.alert_bell_vibra) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Vibra)\n");
                    isNagging = true;
                    setPeripheralOn(VIBRA);
                    // FIXME: completely separate signal pin duration, and buzzer/vibra nag duration.
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }

            }

            setIntervalFromNow(0); // run once so we know if we should do something
        }
    } else {
        LOG_INFO("External Notification Module Disabled\n");
    }

    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

/**
 * @brief An admin message arrived to AdminModule. We are asked whether we want to handle that.
 *
 * @param mp The mesh packet arrived.
 * @param request The AdminMessage request extracted from the packet.
 * @param response The prepared response
 * @return AdminMessageHandleResult HANDLED if message was handled
 *   HANDLED_WITH_RESULT if a result is also prepared.
 */
AdminMessageHandleResult ExternalNotificationModule::handleAdminMessageForModule(const meshtastic_MeshPacket &mp,
                                                                                 meshtastic_AdminMessage *request,
                                                                                 meshtastic_AdminMessage *response)
{
    AdminMessageHandleResult result;

    switch (request->which_payload_variant) {
    case meshtastic_AdminMessage_get_ringtone_request_tag:
        LOG_INFO("Client is getting ringtone\n");
        this->handleGetRingtone(mp, response);
        result = AdminMessageHandleResult::HANDLED_WITH_RESPONSE;
        break;

    case meshtastic_AdminMessage_set_ringtone_message_tag:
        LOG_INFO("Client is setting ringtone\n");
        this->handleSetRingtone(request->set_canned_message_module_messages);
        result = AdminMessageHandleResult::HANDLED;
        break;

    default:
        result = AdminMessageHandleResult::NOT_HANDLED;
    }

    return result;
}

void ExternalNotificationModule::handleGetRingtone(const meshtastic_MeshPacket &req, meshtastic_AdminMessage *response)
{
    LOG_INFO("*** handleGetRingtone\n");
    if (req.decoded.want_response) {
        response->which_payload_variant = meshtastic_AdminMessage_get_ringtone_response_tag;
        strncpy(response->get_ringtone_response, rtttlConfig.ringtone, sizeof(response->get_ringtone_response));
    } // Don't send anything if not instructed to. Better than asserting.
}

void ExternalNotificationModule::handleSetRingtone(const char *from_msg)
{
    int changed = 0;

    if (*from_msg) {
        changed |= strcmp(rtttlConfig.ringtone, from_msg);
        strncpy(rtttlConfig.ringtone, from_msg, sizeof(rtttlConfig.ringtone));
        LOG_INFO("*** from_msg.text:%s\n", from_msg);
    }

    if (changed) {
        nodeDB.saveProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, &meshtastic_RTTTLConfig_msg, &rtttlConfig);
    }
}