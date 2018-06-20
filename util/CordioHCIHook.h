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

#ifndef UTIL_CORDIOHCIHOOK_H_
#define UTIL_CORDIOHCIHOOK_H_

#include "driver/CordioHCITransportDriver.h"
#include "driver/CordioHCIDriver.h"

extern ble::vendor::cordio::CordioHCIDriver& ble_cordio_get_hci_driver();

namespace ble {
namespace vendor {
namespace cordio {

/**
 * Access to the internal state of the Cordio driver.
 */
struct CordioHCIHook {
    /**
     * Return the instance to the Cordio HCI driver.
     */
    static CordioHCIDriver& get_driver() {
        return ble_cordio_get_hci_driver();
    }

    /**
     * Return the instance to the transport driver.
     */
    static CordioHCITransportDriver& get_transport_driver() {
        return get_driver()._transport_driver;
    }

    /**
     * replace the RX handler used by the transport handler.
     */
    static void set_data_received_handler(void (*handler)(uint8_t*, uint8_t)) {
        get_transport_driver().set_data_received_handler(handler);
    }
};

} // namespace cordio
} // namespace vendor
} // namespace ble


#endif /* UTIL_CORDIOHCIHOOK_H_ */
