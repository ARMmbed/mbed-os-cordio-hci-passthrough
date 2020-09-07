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

#ifndef CONTROLLERTOHOST_H_
#define CONTROLLERTOHOST_H_

#include "util/CordioHCIHook.h"
#include "drivers/UnbufferedSerial.h"
#include "UnidirectionalProxy.h"
#include "platform/Span.h"

/**
 * Proxy implementation that transfer data from the controller to the host.
 */
class ControllerToHost : public UnidirectionalProxy<ControllerToHost> {
    friend UnidirectionalProxy<ControllerToHost>;

public:
    /**
     * Construct a proxy that transfer data from the controller used by the
     * cordio stack to the host.
     *
     * @param serial The serial instance used by the host
     */
    ControllerToHost(mbed::UnbufferedSerial& serial) :
       UnidirectionalProxy<ControllerToHost>(),
       serial(serial)
    { }

private:
    //
    // Implementation of the reader class.
    //
    struct Reader {
        Reader(uint8_t* data, uint16_t len) :
            _data_view(data, len),
            _read(false)
        { }

        bool ready() {
            return !_read;
        }

        const  mbed::Span<const uint8_t>& read() {
            _read = true;
            return _data_view;
        }

    private:
        mbed::Span<const uint8_t> _data_view;
        bool _read;
    };

    //
    // Implement interface expected by the UnidirectionalProxy:
    //   - register_listener
    //   - transfer
    //
    void register_listener() {
        self = this;
        ble::CordioHCIHook::set_data_received_handler(
            when_controller_data_dispatch
        );
    }

    void transfer(uint8_t* buffer, uint32_t length) {
        serial.write(buffer, length);
    }

    // handle reception of data
    static void when_controller_data_dispatch(uint8_t* data, uint8_t len) {
        self->when_controller_data(data, len);
    }

    void when_controller_data(uint8_t* data, uint8_t len) {
        Reader reader(data, len);
        acquire_data(reader);
    }

    static ControllerToHost* self;
    mbed::UnbufferedSerial& serial;
};



#endif /* CONTROLLERTOHOST_H_ */
