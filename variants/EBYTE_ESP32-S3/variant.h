// Supporting information: https://github.com/S5NC/EBYTE_ESP32-S3/

// Originally developed for E22-900M30S with ESP32-S3-WROOM-1-N4
// NOTE: Uses ESP32-S3-WROOM-1-N4.json in boards folder (via platformio.ini board field), assumes 4 MB (quad SPI) flash, no PSRAM

// FIXME: implement SX12 module type autodetection and have setup for each case (add E32 support)
// E32 has same pinout except having extra pins. I assume that the GND on it is connected internally to other GNDs so it is not a
// problem to NC the extra GND pins.

// For each EBYTE module pin in this section, provide the pin number of the ESP32-S3 you connected it to
// The ESP32-S3 is great because YOU CAN USE PRACTICALLY ANY PINS for the connections, but avoid some pins (such as on the WROOM
// modules the following): strapping pins (except 0 as a user button input as it already has a pulldown resistor in typical
// application schematic) (0, 3, 45, 46), USB-reserved (19, 20), and pins which aren't present on the WROOM-2 module for
// compatiblity as it uses octal SPI, or are likely connected internally in either WROOM version (26-37), and avoid pins whose
// voltages are set by the SPI voltage (47, 48), and pins that don't exist (22-25) You can ALSO set the SPI pins (SX126X_CS,
// SX126X_SCK, SX126X_MISO, SX126X_MOSI) to any pin with the ESP32-S3 due to \ GPIO Matrix / IO MUX / RTC IO MUX \, and also the
// serial pins, but this isn't recommended for Serial0 as the WROOM modules have a 499 Ohm resistor on U0TXD (to reduce harmonics
// but also acting as a sort of protection)

// We have many free pins on the ESP32-S3-WROOM-X-Y module, perhaps it is best to use one of its pins to control TXEN, and use
// DIO2 as an extra interrupt, but right now Meshtastic does not benefit from having another interrupt pin available.

// Adding two 0-ohm links on your PCB design so that you can choose between the two modes for controlling the E22's TXEN would
// enable future software to make the most of an extra available interrupt pin

// Possible improvement: can add extremely low resistance MOSFET to physically toggle power to E22 module when in full sleep (not
// waiting for interrupt)?

// PA stands for Power Amplifier, used when transmitting to increase output power
// LNA stands for Low Noise Amplifier, used when \ listening for / receiving \ data to increase sensitivity

//////////////////////////////////////////////////////////////////////////////////
//                                                                              //
//   Have custom connections or functionality? Configure them in this section   //
//                                                                              //
//////////////////////////////////////////////////////////////////////////////////

#define SX126X_CS 14    // EBYTE module's NSS pin
#define SX126X_SCK 21   // EBYTE module's SCK pin
#define SX126X_MOSI 38  // EBYTE module's MOSI pin
#define SX126X_MISO 39  // EBYTE module's MISO pin
#define SX126X_RESET 40 // EBYTE module's NRST pin
#define SX126X_BUSY 41  // EBYTE module's BUSY pin
#define SX126X_DIO1 42  // EBYTE module's DIO1 pin
// We don't define a pin for SX126X_DIO2 as Meshtastic doesn't use it as an interrupt output, so it is never connected to an MCU
// pin! Also E22 module datasheets say not to connect it to an MCU pin.
// We don't define a pin for SX126X_DIO3 as Meshtastic doesn't use it as an interrupt output, so it is never connected to an MCU
// pin! Also E22 module datasheets say to use it as the TCXO's reference voltage.
// E32 module (which uses SX1276) may not have ability to set TCXO voltage using a DIO pin.

// Choose ONLY ONE option from below, comment in/out the '/*'s and '*/'s
// SX126X_TXEN is the E22's TXEN pin, SX126X_RXEN is the E22's RXEN pin

// Option 1: E22's TXEN pin connected to E22's DIO2 pin, E22's RXEN pin connected to NEGATED output of E22's DIO2 pin (more
// expensive option hardware-wise, is the 'most proper' way, removes need for routing one/two traces from MCU to RF switching
// pins)
/*
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_RXEN RADIOLIB_NC
*/

// Option 2: E22's TXEN pin connected to E22's DIO2 pin, E22's RXEN pin connected to MCU pin (cheaper option hardware-wise,
// removes need for routing another trace from MCU to an RF switching pin)
// /*
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_RXEN 10
// */

// Option 3: E22's TXEN pin connected to MCU pin, E22's RXEN pin connected to MCU pin (cheaper option hardware-wise, allows for
// ramping up PA before transmission (add feature yourself in RadioLib) if PA takes a while to stabilise)
// Don't define DIO2_AS_RF_SWITCH because we only use DIO2 or an MCU pin mutually exclusively to connect to E22's TXEN to prevent
// a short if they are both connected at the same time (suboptimal PCB design) and there's a slight non-neglibible delay and/or
// voltage difference between DIO2 and TXEN
/*
#define SX126X_TXEN 9
#define SX126X_RXEN 10
*/

// (NOT RECOMMENDED, if need to ramp up PA before transmission, better to use option 3)
// Option 4: E22's TXEN pin connected to MCU pin, E22's RXEN pin connected to NEGATED output of E22's DIO2 pin (more expensive
// option hardware-wise, allows for ramping up PA before transmission (add feature yourself in RadioLib) if PA takes a while to
// stabilise, removes need for routing another trace from MCU to an RF switching pin, however may mean if in RadioLib you don't
// tell DIO2 to go high to indicate transmission (so the negated output goes to RXEN to turn the LNA off) then you may end up
// enabling E22's TXEN and RXEN pins at the same time whilst you ramp up the PA which is not ideal, changing DIO2's switching
// advance in RadioLib may not even be possible, may be baked into the SX126x)
/*
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_TXEN 9
#define SX126X_RXEN RADIOLIB_NC
*/

// Status
#define LED_PIN 1
// External notification
// FIXME: Check if EXT_NOTIFY_OUT actualy has any effect and removes the need for setting the external notication pin in the
// app/preferences
#define EXT_NOTIFY_OUT 2 // The GPIO pin that acts as the external notification output (here we connect an LED to it)
// Buzzer
#define PIN_BUZZER 11
// Buttons
#define BUTTON_PIN 0 // Use the BOOT button as the user button
// I2C
#define I2C_SCL 18
#define I2C_SDA 8
// UART
#define UART_TX 43
#define UART_RX 44

// Power
// Outputting 22dBm from SX1262 results in ~30dBm E22-900M30S output (module only uses last stage of the YP2233W PA)
// Respect local regulations! If your E22-900M30S outputs the advertised 30 dBm and you use a 6 dBi antenna, you are at the
// equivalent of 36 EIRP (Effective Isotropic Radiated Power), which in this case is the limit for non-HAM users in the US (4W
// EIRP, at SPECIFIC frequencies).
// In the EU (and UK), as of now, you are allowed 27 dBm ERP which is 29.15 EIRP.
// https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32022D0180
// https://www.legislation.gov.uk/uksi/1999/930/schedule/6/made
// To respect the 29.15 dBm EIRP (at SPECIFIC frequencies, others are lower) EU limit with a 2.5 dBi gain antenna, consulting
// https://github.com/S5NC/EBYTE_ESP32-S3/blob/main/power%20testing.txt, assuming 0.1 dBm insertion loss, output 20 dBm from the
// E22-900M30S's SX1262. It is worth noting that if you are in this situation and don't have a HAM license, you may be better off
// with a lower gain antenna, and output the difference as a higher total power input into the antenna, as your EIRP would be the
// same, but you would get a wider angle of coverage. Also take insertion loss and possibly VSWR into account
// (https://www.everythingrf.com/tech-resources/vswr). Please check regulations yourself and check airtime, usage (for example
// whether you are airborne), frequency, and power laws.
#define SX126X_MAX_POWER 22 // SX126xInterface.cpp defaults to 22 if not defined, but here we define it for good practice

// Display
// FIXME: change behavior in src to default to not having screen if is undefined
// FIXME: remove 0/1 option for HAS_SCREEN in src, change to being defined or not
// FIXME: check if it actually causes a crash when not specifiying that a display isn't present
#define HAS_SCREEN 0 // Assume no screen present by default to prevent crash...

// GPS
// FIXME: remove 0/1 option for HAS_GPS in src, change to being defined or not
// #define HAS_GPS 0 // Don't need to set this to 0 to prevent a crash as it doesn't crash if GPS not found, will probe by default
// #define PIN_GPS_EN 15
// #define GPS_TX_PIN 16
// #define GPS_RX_PIN 17

/////////////////////////////////////////////////////////////////////////////////
//                                                                             //
//   You should have no need to modify the code below, nor in pins_arduino.h   //
//                                                                             //
/////////////////////////////////////////////////////////////////////////////////

#define USE_SX1262 // E22-900M30S, E22-900M22S, and E22-900MM22S (not E220!) use SX1262

// The below isn't needed as we directly define SX126X_TXEN and SX126X_RXEN instead of using proxies E22_TXEN and E22_RXEN
/*
// FALLBACK: If somehow E22_TXEN isn't defined or clearly isn't a valid pin number, set it to RADIOLIB_NC to avoid SX126X_TXEN
being defined but having no value #if (!defined(E22_TXEN) || !(0 <= E22_TXEN && E22_TXEN <= 48)) #define E22_TXEN RADIOLIB_NC
#endif
// FALLBACK: If somehow E22_RXEN isn't defined or clearly isn't a valid pin number, set it to RADIOLIB_NC to avoid SX126X_RXEN
being defined but having no value #if (!defined(E22_RXEN) || !(0 <= E22_RXEN && E22_RXEN <= 48)) #define E22_RXEN RADIOLIB_NC
#endif
#define SX126X_TXEN E22_TXEN
#define SX126X_RXEN E22_RXEN
*/

// E22 series TCXO voltage is 1.8V per https://www.ebyte.com/en/pdf-down.aspx?id=781 (source
// https://github.com/jgromes/RadioLib/issues/12#issuecomment-520695575), so set it as such
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

// Even if the module is not RF95 some pins used in src are still named as such due to relics of the past, see
// https://github.com/meshtastic/firmware/blob/8b82ae6fe3f36fbadc0dee87a82fc7e5c520a6f3/src/main.cpp#L533-L554
// Some boards (for example station-g1) don't define these pins, but they are still needed, instead the default ones from
// https://github.com/meshtastic/firmware/blob/61f6fb22c5de371783a86a5a7db7ea446e6202e7/src/main.cpp#L544-L549
// FIXME: rename the constants in the files above (and all variant files which don't use the RF95) to remove ambiguity and use the
// respective definitions in each src file
#define RF95_NSS                                                                                                                 \
    SX126X_CS // The names SX126X_CS and RF95_SCK are both used in src, we define SX126X_SCK in the configuration to stop users
              // wondering why an RF95 is defined when the module isn't an RF95
#define RF95_SCK                                                                                                                 \
    SX126X_SCK // The name SX126X_SCK is never used in src, RF95_SCK is the name used in src, but we define SX126X_SCK in the
               // configuration to stop users wondering why an RF95 is defined when the module isn't an RF95
#define RF95_MOSI                                                                                                                \
    SX126X_MOSI // The name SX126X_MOSI is never used in src, RF95_MOSI is the name used in src, but we define SX126X_MOSI in the
                // configuration to stop users wondering why an RF95 is defined when the module isn't an RF95
#define RF95_MISO                                                                                                                \
    SX126X_MISO // The name SX126X_MISO is never used in src, RF95_MISO is the name used in src, but we define SX126X_MISO in the
                // configuration to stop users wondering why an RF95 is defined when the module isn't an RF95

// Many of the below values would only be used if USE_RF95 was defined, but it's not as we aren't actually using an RF95, just
// that the 4 pins above are named like it If they aren't used they don't need to be defined and doing so cause confusion to those
// adapting this file LORA_RESET value is never used in src (as we are not using RF95), so no need to define LORA_DIO0 is not used
// in src (as we are not using RF95) as SX1262 does not have it per SX1262 datasheet, so no need to define
// FIXME: confirm that the linked lines below are actually only called when using the SX126x or SX128x and no other modules
// then use SX126X_DIO1 and SX128X_DIO1 respectively for that purpose, removing the need for RF95-style LORA_* definitions when
// the RF95 isn't used
#define LORA_DIO1                                                                                                                \
    SX126X_DIO1 // The old name is used in
                // https://github.com/meshtastic/firmware/blob/7eff5e7bcb2084499b723c5e3846c15ee089e36d/src/sleep.cpp#L298, so
                // must also define the old name
// LORA_DIO2 value is never used in src (as we are not using RF95), so no need to define, and if DIO2_AS_RF_SWITCH is set then it
// cannot serve any extra function even if requested to LORA_DIO3 value is never used in src (as we are not using RF95), so no
// need to define, and DIO3_AS_TCXO_AT_1V8 is set so it cannot serve any extra function even if requested to (from 13.3.2.1
// DioxMask in SX1262 datasheet: Note that if DIO2 or DIO3 are used to control the RF Switch or the TCXO, the IRQ will not be
// generated even if it is mapped to the pins.)