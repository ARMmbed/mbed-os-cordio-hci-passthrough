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

#ifndef HOSTTOCONTROLLER_H_
#define HOSTTOCONTROLLER_H_

#include "UnidirectionalProxy.h"
#include "hci_defs.h"
#include "util/CordioHCIHook.h"
#include "drivers/RawSerial.h"

#if CORDIO_ZERO_COPY_HCI
#include "wsf_types.h"
#include "wsf_buf.h"
#include "wsf_msg.h"

/* avoid many small allocs (and WSF doesn't have smaller buffers) */
#define MIN_WSF_ALLOC (16)
#endif //CORDIO_ZERO_COPY_HCI

/**
 * Proxy implementation that transfer data from the host to the controller.
 */
class HostToController : public UnidirectionalProxy<HostToController> {
    friend UnidirectionalProxy<HostToController>;

public:
    HostToController(mbed::RawSerial& serial) :
       UnidirectionalProxy<HostToController>(),
       serial(serial),
       packet_state(WAITING_FOR_PACKET_TYPE),
       packet_type(0),
#if CORDIO_ZERO_COPY_HCI
       packet(NULL),
       packet_buffer_size(0),
#endif //CORDIO_ZERO_COPY_HCI
       packet_index(0),
       packet_length(0)
    { }

private:
    //
    // Implement interface expected by the UnidirectionalProxy:
    //   - register_listener
    //   - transfer
    //
    void register_listener() {
        serial.attach(
            mbed::callback(this, &HostToController::when_rx_interrupt),
            serial.RxIrq
        );
    }

    void transfer(uint8_t* buffer, uint32_t length) {
        // The HCI driver expect a full packet to be transfered therefore data
        // in input must be parsed and grouped in a packet.
        while (length) {
#if CORDIO_ZERO_COPY_HCI
            /* link layer expects a wsf message it will take ownership of
             * so we maintain a buffer that is a wsf msg and if we get
             * the packet in parts we keep growing the msg copying the old
             * into the new msg */

            uint8_t *old_msg = packet;

            if (!packet || (length + packet_index > packet_buffer_size)) {
                packet_buffer_size = packet_index + length;
                if (packet_buffer_size < MIN_WSF_ALLOC) {
                    packet_buffer_size = MIN_WSF_ALLOC;
                }
                packet = (uint8_t*)WsfMsgAlloc(packet_buffer_size);
            }

            if (old_msg && (old_msg != packet)) {
                memcpy(packet, old_msg, packet_index);
                WsfMsgFree(old_msg);
            }
#endif // CORDIO_ZERO_COPY_HCI

            switch (packet_state) {
                case WAITING_FOR_PACKET_TYPE:
                    handle_packet_type(buffer, length);
                    break;

                case WAITING_FOR_HEADER_COMPLETE: {
                    handle_header(buffer, length);
                }   break;

                case WAITING_FOR_DATA_COMPLETE:
                    copy_packet_data(buffer, length);
                    break;

                default:
                    break;
            }

            if (is_packet_complete()) {
                transfer_packet();
            }

        }
    }

    // handle state WAITING_FOR_PACKET_TYPE.
    void handle_packet_type(uint8_t*& buffer, uint32_t& length)  {
        packet_type = *buffer++;
        packet_index = 0;
        --length;

        packet_state = WAITING_FOR_HEADER_COMPLETE;
        switch (packet_type) {
            case HCI_CMD_TYPE:
                packet_length = HCI_CMD_HDR_LEN;
                break;
            case HCI_ACL_TYPE:
                packet_length = HCI_ACL_HDR_LEN;
                break;
            default:
                fatal_error();
                break;
        }
    }

    // handle state WAITING_FOR_HEADER_COMPLETE
    void handle_header(uint8_t*& buffer, uint32_t& length) {
        copy_packet_data(buffer, length);

        if (packet_index == packet_length) {
            packet_state = WAITING_FOR_DATA_COMPLETE;
            switch (packet_length) {
                case HCI_CMD_HDR_LEN:
                    packet_length = HCI_CMD_HDR_LEN + packet[HCI_CMD_HDR_LEN - 1];
                    break;

                case HCI_ACL_HDR_LEN:
                    // packet length of an ACL packet is 16 bit long; copy it
                    // into packet length then add the header length.
                    memcpy(&packet_length, &packet[HCI_ACL_HDR_LEN - 1], sizeof(packet_length));
                    packet_length += HCI_ACL_HDR_LEN;
                    break;

                default:
                    fatal_error();
                    break;
            }
        }
    }

    // Copy data of the packet and update the SM.
    void copy_packet_data(uint8_t*& buffer, uint32_t& length) {
        uint32_t step = std::min(length, (uint32_t) (packet_length) - packet_index);
        memcpy(packet + packet_index, buffer, step);
        packet_index += step;
        buffer += step;
        length -= step;
    }

    // return true if the packet is complete and false otherwise
    bool is_packet_complete() {
        return (packet_state == WAITING_FOR_DATA_COMPLETE) &&
            (packet_index == packet_length);
    }

    // transfer the packet and reset the state machine
    void transfer_packet() {
        ble::vendor::cordio::CordioHCIHook::get_transport_driver().write(
            packet_type,
            packet_length,
            packet
        );
        packet_state = WAITING_FOR_PACKET_TYPE;
        packet_index = 0;
        packet_length = 0;
#if CORDIO_ZERO_COPY_HCI
        /* link layer takes ownerhsiop of the WSF message */
        packet = NULL;
        packet_buffer_size = 0;
#endif // CORDIO_ZERO_COPY_HCI
    }

    //
    // Implement the Reader interface
    //   - ready()
    //   - read()
    bool ready() {
        return serial.readable();
    }

    uint8_t read() {
        return serial.getc();
    }

    // handle reception of data
    void when_rx_interrupt() {
        acquire_data(*this);
    }

    enum state_t {
        WAITING_FOR_PACKET_TYPE,
        WAITING_FOR_HEADER_COMPLETE,
        WAITING_FOR_DATA_COMPLETE
    };

    mbed::RawSerial& serial;
    state_t packet_state;
    uint8_t packet_type;
#if CORDIO_ZERO_COPY_HCI
    uint8_t *packet;
    uint16_t packet_buffer_size;
#else
    uint8_t packet[512];
#endif // CORDIO_ZERO_COPY_HCI
    uint16_t packet_index;
    uint16_t packet_length;
};

#endif /* HOSTTOCONTROLLER_H_ */
