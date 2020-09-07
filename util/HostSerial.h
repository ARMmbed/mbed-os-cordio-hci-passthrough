/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UTIL_HOSTSERIAL_H_
#define UTIL_HOSTSERIAL_H_

#include "drivers/UnbufferedSerial.h"

namespace util {

/**
 * Macros for setting console flow control.
 */
#define CONSOLE_FLOWCONTROL_RTS     1
#define CONSOLE_FLOWCONTROL_CTS     2
#define CONSOLE_FLOWCONTROL_RTSCTS  3
#define mbed_console_concat_(x) CONSOLE_FLOWCONTROL_##x
#define mbed_console_concat(x) mbed_console_concat_(x)
#define CONSOLE_FLOWCONTROL mbed_console_concat(MBED_CONF_TARGET_CONSOLE_UART_FLOW_CONTROL)

/**
 * Return the host serial instance
 * @return The serial instance connected to the host.
 */
mbed::UnbufferedSerial& get_host_serial() {
    static mbed::UnbufferedSerial serial(USBTX, USBRX);
    static bool initialized = false;

    if (!initialized) {
#if   CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTS
        serial.set_flow_control(SerialBase::RTS, STDIO_UART_RTS, NC);
#elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_CTS
        serial.set_flow_control(SerialBase::CTS, NC, STDIO_UART_CTS);
#elif CONSOLE_FLOWCONTROL == CONSOLE_FLOWCONTROL_RTSCTS
        serial.set_flow_control(SerialBase::RTSCTS, STDIO_UART_RTS, STDIO_UART_CTS);
#endif
        serial.baud(MBED_CONF_APP_PASSTHROUGH_BAUDRATE);
        initialized = true;
    }

    return serial;
}

}

#endif /* UTIL_HOSTSERIAL_H_ */
