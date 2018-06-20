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

#ifndef UNIDIRECTIONALPROXY_H_
#define UNIDIRECTIONALPROXY_H_

#include "rtos/Thread.h"
#include "rtos/EventFlags.h"
#include "util/CircularBuffer.h"
#include "platform/CriticalSectionLock.h"

/**
 * Base class for an unidirectional proxy. It acquires and buffers data from
 * one end point in interrupt context and in parallel transfer the data to the
 * destination.
 *
 * Implementation needs to implement the functions:
 *    * register_listener(): Set up the handler that will receive the data.
 *    * transfer(const uint8_t* data, uint16_t length): transfer the data.
 *
 * The data handler in the implementation must also call the function
 * acquire_data with a reader when data is ready to be read.
 *
 * @warning This class has been designed with performance in mind to avoid bytes
 * loss over a transport non error resilient. Data are acquired and buffered in
 * one context (preferably the interrupt context) while a real-time thread
 * dequeue the data and transfer them. If your transport is resilient to error
 * use another class that does everything in thread mode.
 *
 * @note CRTP is used instead of virtual dispatch to enhance performances.
 *
 * @warning Instances can be quite large; be careful of where they are
 * allocated.
 */
template<class Impl>
class UnidirectionalProxy : mbed::NonCopyable<UnidirectionalProxy<Impl> > {

public:
    /**
     * Start the proxy thread that will handle the transfer of the data.
     */
    void start() {
        impl()->register_listener();
        _worker_thread.start(mbed::callback(this, &UnidirectionalProxy::run));
    }

protected:
    UnidirectionalProxy() :
        _worker_thread(osPriorityRealtime, sizeof(_thread_stack), _thread_stack),
        _rx_buffer(),
        _signal_channel()
    { }

    /**
     * Acquire data from a reader.
     *
     * @param reader The instance that read the data. It must provide the following
     * concepts:
     *
     *   - ready(): return true if the reader is ready to be read and false
     *   if there is no data left to read.
     *   - read(): return a chunk of data read; the type returned must be
     *   acceptable by a CircularBuffer<uint8_t>. So it can be a byte or an
     *   array view to a chunk of data read.
     */
    template<typename Reader>
    void acquire_data(Reader& reader) {
        if (reader.ready() == false) {
            return;
        }

        while (reader.ready()) {
            if(_rx_buffer.push(reader.read()) == false) {
                signal_reception_error();
                return;
            }
        }

        signal_data_available();
    }

    /**
     * Called when a fatal error happened.
     */
    void fatal_error() {
        // in case of error block the thread in a while loop; it will help
        // users in debugging.
        while (true) { }
    }

private:
    /**
     * Loop that wait and transfer data received.
     */
    void run() {
        uint8_t buffer[CONSUMER_BUFFER_LENGTH];
        uint32_t length = 0;

        while (true) {
            {
                mbed::CriticalSectionLock lock;
                length = _rx_buffer.pop(buffer);
            }

            if (length == 0) {
                wait_for_data();
            } else {
                impl()->transfer(buffer, length);
            }
        }
    }

    /**
     * Wait for data availability.
     *
     * @note in case of error, the function block.
     */
    void wait_for_data() {
        // clear reception flags
        uint32_t flags = _signal_channel.get();
        _signal_channel.set(flags & ~data_available_flag);

        flags = _signal_channel.wait_any(
            waiting_flags,
            /* timeout */ osWaitForever,
            /* clear */ false
        );

        if (flags & reception_error_flag) {
            fatal_error();
        }
    }

    /**
     * CRTP Helper; return a pointer to the implementation instance.
     * @return
     */
    Impl* impl() {
        return static_cast<Impl*>(this);
    }

    /**
     * Signal to the consumer that data is available.
     */
    void signal_data_available() {
        if (!(_signal_channel.get() & data_available_flag)) {
            _signal_channel.set(data_available_flag);
        }
    }

    /**
     * Signal to the consumer that an error happened during the reception of
     * the data.
     */
    void signal_reception_error() {
        if (!(_signal_channel.get() & reception_error_flag)) {
            _signal_channel.set(reception_error_flag);
        }
    }

    static const size_t CIRCULAR_BUFFER_LENGTH = 8192;
    static const size_t CONSUMER_BUFFER_LENGTH = 32;

    static const uint32_t data_available_flag = 1 << 0;
    static const uint32_t reception_error_flag = 1 << 1;
    static const uint32_t waiting_flags =
        data_available_flag | reception_error_flag;

    rtos::Thread _worker_thread;
    uint8_t _thread_stack[512];
    ::util::CircularBuffer<uint8_t, CIRCULAR_BUFFER_LENGTH> _rx_buffer;
    rtos::EventFlags _signal_channel;
};

#endif /* UNIDIRECTIONALPROXY_H_ */
