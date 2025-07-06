# TCP/IP Protocol Stack Implementation
_Reference: https://cs144.stanford.edu_

This repository implements a complete TCP/IP protocol stack in C++, providing a layered network architecture that follows the traditional TCP/IP model with clear separation of concerns between layers. Key components are:
- **Transport Layer**: TCP sender/receiver with reliable data transmission, flow control, and retransmission
- **Network Layer**: IP routing with longest-prefix matching and packet forwarding
- **Data Link Layer**: Ethernet framing with ARP resolution for MAC address mapping
- **Data Management**: Efficient stream processing with circular buffering and reference management [1](#0-0) 
## Architecture
### Layered Design
The system implements a complete TCP/IP protocol stack with clear layer separation:
<img src="https://github.com/LuminaScript/TCP-IP-Protocol-Stack/blob/main/images/01.png?raw=true" alt="Screenshot" width="650"/>
### Key Components

| Component | Purpose | Key Files |
|-----------|---------|-----------|
| `ByteStream` | Circular buffer for efficient data streaming | `src/byte_stream.hh` | [3](#0-2) 
| `TCPSender` | Reliable data transmission with retransmission | `src/tcp_sender.hh`, `src/tcp_sender.cc` |
| `TCPReceiver` | Segment processing and acknowledgment generation | `src/tcp_receiver.hh` |
| `Reassembler` | Out-of-order segment reassembly | `src/reassembler.hh` |
| `Router` | Packet forwarding with routing table lookup | `src/router.hh`, `src/router.cc` |
| `NetworkInterface` | Ethernet framing and ARP resolution | `src/network_interface.hh` |

## Features

### TCP Implementation
- **Reliable Transmission**: Sequence numbering, acknowledgments, and retransmission with exponential backoff
- **Flow Control**: Sliding window protocol with receiver window management
- **Connection Management**: SYN/FIN handshake handling and connection state tracking
- **Error Handling**: RST flag processing and error propagation [4](#0-3) 

### Network Layer
- **IP Routing**: Longest-prefix matching for route selection
- **Packet Forwarding**: TTL decrement and validation
- **Multiple Interfaces**: Support for multi-homed routing configurations

### Data Link Layer
- **ARP Protocol**: Automatic IP-to-MAC address resolution with caching
- **Ethernet Framing**: Proper frame encapsulation and processing
- **Timeout Management**: ARP request expiration and mapping lifetime management [5](#0-4) 

## Configuration

The system uses configurable parameters for various protocol aspects: [6](#0-5) 

- **Buffer Capacity**: Default 64KB for send/receive buffers
- **Maximum Payload**: 1000 bytes per TCP segment
- **Retransmission Timeout**: 1 second default with exponential backoff
- **Maximum Retransmissions**: 8 attempts before connection failure

