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

#include "rtos/Thread.h"
#include "HostToController.h"
#include "ControllerToHost.h"
#include "util/CordioHCIHook.h"
#include "util/HostSerial.h"
#include "mbed_power_mgmt.h"

// static initialization as these objects are large.
HostToController host_to_controller(util::get_host_serial());
ControllerToHost controller_to_host(util::get_host_serial());

int main() {
    ble::CordioHCIHook::get_driver().initialize();
    host_to_controller.start();
    controller_to_host.start();

    while (true) {
        sleep();
    }

    return 0;
}


