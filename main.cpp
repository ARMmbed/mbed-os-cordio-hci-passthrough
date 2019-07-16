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
#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_msg.h"
#include "wsf_os.h"
#include "wsf_buf.h"
#include "wsf_timer.h"
#include "hci_handler.h"
#include "mbed.h"

using ble::vendor::cordio::CordioHCIHook;

// static initialization as these objects are large.
HostToController host_to_controller(util::get_host_serial());
ControllerToHost controller_to_host(util::get_host_serial());

extern uint8_t *SystemHeapStart;
extern uint32_t SystemHeapSize;

void init_wsf(ble::vendor::cordio::buf_pool_desc_t& buf_pool_desc) {
    // use the buffer for the WSF heap
    SystemHeapStart = buf_pool_desc.buffer_memory;
    SystemHeapSize = buf_pool_desc.buffer_size;

    // Initialize buffers with the ones provided by the HCI driver
    uint16_t bytes_used = WsfBufInit(
        buf_pool_desc.pool_count,
        (wsfBufPoolDesc_t*)buf_pool_desc.pool_description
    );

    // Raise assert if not enough memory was allocated
    MBED_ASSERT(bytes_used != 0);

    SystemHeapStart += bytes_used;
    SystemHeapSize -= bytes_used;

    WsfTimerInit();
}

int main() {
    ble::vendor::cordio::CordioHCIDriver& hci_driver = CordioHCIHook::get_driver();

#if CORDIO_ZERO_COPY_HCI
    ble::vendor::cordio::buf_pool_desc_t buf_pool_desc = hci_driver.get_buffer_pool_description();
    init_wsf(buf_pool_desc);
#endif

    hci_driver.initialize();

    host_to_controller.start();
    controller_to_host.start();

#if CORDIO_ZERO_COPY_HCI
    uint64_t last_update_us;
    mbed::LowPowerTimer timer;

    while (true) {
        last_update_us += (uint64_t) timer.read_high_resolution_us();
        timer.reset();

        uint64_t last_update_ms = (last_update_us / 1000);
        wsfTimerTicks_t wsf_ticks = (last_update_ms / WSF_MS_PER_TICK);

        if (wsf_ticks > 0) {
            WsfTimerUpdate(wsf_ticks);
            last_update_us -= (last_update_ms * 1000);
        }

        wsfOsDispatcher();

        bool sleep = false;
        {
            /* call needs interrupts disabled */
            CriticalSectionLock critical_section;
            if (wsfOsReadyToSleep()) {
                sleep = true;
            }
        }

        uint64_t time_spent = (uint64_t) timer.read_high_resolution_us();

        /* don't bother sleeping if we're already past tick */
        if (sleep && (WSF_MS_PER_TICK * 1000 > time_spent)) {
            /* sleep to maintain constant tick rate */
            wait_us(WSF_MS_PER_TICK * 1000 - time_spent);
        }
    }
#else
    while(true) {
        rtos::Thread::wait(osWaitForever);
    }
#endif // CORDIO_ZERO_COPY_HCI

    return 0;
}


