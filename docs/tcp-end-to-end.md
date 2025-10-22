# Complete End-to-End TCP Packet Flow

## Overview: Full Network Stack Journey
```
Application Data → ByteStream → TCP → IPv4 → Ethernet → UDP (Real) → Bounce Server → UDP → Ethernet → IPv4 → TCP → ByteStream → Application
     │              │           │      │        │          │             │              │        │      │      │           │
     └──────────────┴───────────┴──────┴────────┴──────────┘             └──────────────┴────────┴──────┴──────┴───────────┘
              VIRTUAL (Simulated in  code)                                    VIRTUAL (Simulated in peer's code)
                                                  └──────────┘
                                              REAL (OS UDP socket)
```

## 🎭 Virtual vs Real: What's What?

### ⚙️ **VIRTUAL (Hardcoded/Simulated)** - Lives in C++ process
- **TCP layer** -  `tcp_sender.cc`, `tcp_receiver.cc`
- **IPv4 addresses** - `192.168.0.50`, `172.16.0.100` (private addresses, configured in code)
- **Ethernet** - MAC addresses generated randomly, frames exist only in memory
- **Router** -  `router.cc`, routing tables hardcoded in `endtoend.cc`
- **Network segments** - Simulated with Unix socket pairs

### 🌐 **REAL** - Handled by OS network stack
- **UDP socket** - System call to OS, real UDP/IP
- **IP addresses** -  machine's actual IP, bounce server's public IP (104.196.238.229)
- **Port numbers** - Real UDP ports (e.g., 3002 for bounce server)
- **Internet routing** - Real routers, ISP infrastructure

---

## Network Topology

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CLIENT MACHINE                                    │
│                                                                              │
│  ╔══════════════════════════════════════════════════════════════════════╗  │
│  ║                    VIRTUAL NETWORK (In-Memory)                        ║  │
│  ╚══════════════════════════════════════════════════════════════════════╝  │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │ Application Layer: "Hello World"                                     │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ ByteStream (Reliable Byte Stream) 🟦 VIRTUAL                         │   │
│  │ • Buffered data: "Hello World"                                       │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ TCP Sender (tcp_sender.cc) 🟦 VIRTUAL                                │   │
│  │ • SEQ: 1000 (simulated TCP state)                                    │   │
│  │ • Payload: "Hello World"                                             │   │
│  │ • Creates TCPSegment                                                 │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ TCP Layer (TCPMessage) 🟦 VIRTUAL                                    │   │
│  │ ┌───────────────────────────────────────────────────────────────┐   │   │
│  │ │ TCP Header:                                                    │   │   │
│  │ │   SRC Port: 54321  🟦 HARDCODED in endtoend.cc                │   │   │
│  │ │   DST Port: 1234   🟦 HARDCODED in endtoend.cc                │   │   │
│  │ │   SEQ: 1000                                                   │   │   │
│  │ │   ACK: 5000                                                   │   │   │
│  │ │   Flags: ACK, PSH                                             │   │   │
│  │ │   Window: 65535                                               │   │   │
│  │ │   Checksum: 0xABCD                                            │   │   │
│  │ ├───────────────────────────────────────────────────────────────┤   │   │
│  │ │ Payload: "Hello World" (11 bytes)                             │   │   │
│  │ └───────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Network Layer (IPv4 - InternetDatagram) 🟦 VIRTUAL                  │   │
│  │ ┌───────────────────────────────────────────────────────────────┐   │   │
│  │ │ IPv4 Header:                                                   │   │   │
│  │ │   Version: 4                                                  │   │   │
│  │ │   IHL: 5 (20 bytes)                                           │   │   │
│  │ │   TOS: 0                                                      │   │   │
│  │ │   Total Length: 51 (20 + 20 + 11)                            │   │   │
│  │ │   ID: 12345                                                   │   │   │
│  │ │   TTL: 64 🟦 HARDCODED initial value                          │   │   │
│  │ │   Protocol: 6 (TCP)                                           │   │   │
│  │ │   SRC IP: 192.168.0.50  🟦 HARDCODED in endtoend.cc          │   │   │
│  │ │   DST IP: 172.16.0.100  🟦 HARDCODED in endtoend.cc          │   │   │
│  │ │   Checksum: 0x1234                                            │   │   │
│  │ ├───────────────────────────────────────────────────────────────┤   │   │
│  │ │ TCP Segment (31 bytes)                                         │   │   │
│  │ └───────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │ send_datagram(dgram, next_hop=192.168.0.1)    │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Network Interface (network_interface.cc) 🟦 VIRTUAL                 │   │
│  │ • My MAC: 02:XX:XX:XX:XX:01  🟦 RANDOM generated at runtime         │   │
│  │ • My IP: 192.168.0.50        🟦 HARDCODED in endtoend.cc            │   │
│  │ • ARP Cache lookup for 192.168.0.1 (router) 🟦 VIRTUAL ARP          │   │
│  │   - Found: MAC = 02:00:00:YY:YY:01                                   │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Link Layer (Ethernet Frame) 🟦 VIRTUAL                               │   │
│  │ ┌───────────────────────────────────────────────────────────────┐   │   │
│  │ │ Ethernet Header:                                               │   │   │
│  │ │   DST MAC: 02:00:00:YY:YY:01  🟦 From VIRTUAL ARP cache        │   │   │
│  │ │   SRC MAC: 02:XX:XX:XX:XX:01  🟦 RANDOM generated              │   │   │
│  │ │   Type: 0x0800 (IPv4)                                         │   │   │
│  │ ├───────────────────────────────────────────────────────────────┤   │   │
│  │ │ IPv4 Datagram (51 bytes)                                       │   │   │
│  │ └───────────────────────────────────────────────────────────────┘   │   │
│  │ Total: 14 + 51 = 65 bytes                                            │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │ transmit(frame) via OutputPort                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Virtual Network Segment (NetworkInterfaceAdapter) 🟦 VIRTUAL         │   │
│  │ • Unix socket pair for frame transport (simulated wire)              │   │
│  │ • Writes Ethernet frame (65 bytes) to socket                         │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Router (Client Side) 🟦 VIRTUAL                                      │   │
│  │ • Interface eth0: 192.168.0.1  🟦 HARDCODED in endtoend.cc          │   │
│  │ • Interface eth1: 10.0.0.192   🟦 HARDCODED in endtoend.cc          │   │
│  │ • Receives frame on eth0                                             │   │
│  │ • Routing table lookup: 🟦 HARDCODED routes                          │   │
│  │   Destination: 172.16.0.100 → Route via 10.0.0.172 on eth1          │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│                            │                                                 │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ Router's Internet Interface (10.0.0.192) 🟦 VIRTUAL                  │   │
│  │ • Need MAC for next hop: 10.0.0.172                                  │   │
│  │ • ARP resolution (if needed) 🟦 VIRTUAL                              │   │
│  │ • Create new Ethernet frame:                                         │   │
│  │   SRC MAC: Router's eth1 MAC 🟦 RANDOM                               │   │
│  │   DST MAC: 10.0.0.172's MAC (from ARP) 🟦 VIRTUAL                    │   │
│  │ • Decrement TTL: 64 → 63 🟦  router.cc code                      │   │
│  │ • Recompute IPv4 checksum                                            │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
│  ╔═════════════════════════▼═══════════════════════════════════════════╗  │
│  ║              TRANSITION TO REAL NETWORK                              ║  │
│  ╚══════════════════════════════════════════════════════════════════════╝  │
│  ┌─────────────────────────▼───────────────────────────────────────────┐   │
│  │ REAL UDP Socket (internet_socket) 🟢 REAL OS SOCKET                  │   │
│  │ • Type: AF_INET, SOCK_DGRAM (OS system call)                         │   │
│  │ • Connected to: cs144.keithw.org:3002  🟢 REAL DNS + IP              │   │
│  │   Resolved IP: 104.196.238.229         🟢 REAL public IP             │   │
│  │ • Local port: 54782 (assigned by OS)   🟢 REAL ephemeral port        │   │
│  │ • Sends Ethernet frame (65 bytes) as UDP payload                     │   │
│  │   📦 Payload = VIRTUAL Ethernet frame (all virtual layers inside!)   │   │
│  └─────────────────────────┬───────────────────────────────────────────┘   │
└────────────────────────────┼─────────────────────────────────────────────────┘
                             │
                             │ REAL INTERNET (UDP Transport) 🟢
                             │ Physical routers, switches, ISP infrastructure
                             │
                    ┌────────▼─────────┐
                    │  Real UDP Packet │  🟢 REAL
                    │  ┌──────────────┐│
                    │  │ Real IP Hdr: ││  🟢 OS adds this
                    │  │  SRC:    ││  🟢  real IP
                    │  │   public IP  ││
                    │  │  DST: 104.   ││  🟢 Bounce server's
                    │  │   196.238.229││     real public IP
                    │  ├──────────────┤│
                    │  │ Real UDP Hdr:││  🟢 OS adds this
                    │  │  SRC Port:   ││  🟢 OS-assigned
                    │  │    54782     ││     ephemeral port
                    │  │  DST Port:   ││  🟢 Specified by
                    │  │    3002      ││      code
                    │  │  Length: 73  ││
                    │  ├──────────────┤│
                    │  │ 🟦 VIRTUAL   ││  🟦  code created
                    │  │ Ethernet     ││     this entire 65-byte
                    │  │ Frame        ││     blob, OS treats it
                    │  │ (65 bytes)   ││     as opaque payload
                    │  └──────────────┘│
                    └────────┬─────────┘
                             │ 🟢 REAL Internet routing
                             │    Real routers, ISPs, BGP
┌────────────────────────────▼─────────────────────────────────────────────────┐
│                  BOUNCE SERVER (cs144.keithw.org:3002) 🟢 REAL               │
│  IP: 104.196.238.229  🟢 Real public IP                                      │
│  Port: 3002           🟢 Real UDP port                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ UDP Socket (Port 3002) 🟢 REAL OS SOCKET                              │  │
│  │ • Receives UDP datagram from OS                                        │  │
│  │ • Extracts Ethernet frame from UDP payload                             │  │
│  │ • Packet size: 65 bytes (treats as opaque blob)                        │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Bounce Logic (Relay State Machine) 🟢 REAL relay software              │  │
│  │ • Client Registry:                                                      │  │
│  │   - Client A: 1.2.3.4:54782   🟢  real IP:port                     │  │
│  │   - Client B: 5.6.7.8:54783   🟢 Peer's real IP:port                   │  │
│  │                                                                         │  │
│  │ • Does NOT parse Ethernet frame content! 🟦                            │  │
│  │ • Just maintains UDP address mappings                                   │  │
│  │                                                                         │  │
│  │ • Relay Decision:                                                       │  │
│  │   IF both clients connected → Forward blob to peer                     │  │
│  │   ELSE → Buffer blob, wait for peer to connect                         │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Relay to Peer (Client B) 🟢 REAL UDP send                               │  │
│  │ • Wrap same 65-byte blob in new UDP packet                              │  │
│  │ • Send to: 5.6.7.8:54783  🟢 Real destination                           │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
└────────────────────────────┼───────────────────────────────────────────────────┘
                             │ 🟢 REAL Internet routing (return path)
                             │
                    ┌────────▼─────────┐
                    │  Real UDP Packet │  🟢 REAL
                    │  (to Server)     │
                    │  ┌──────────────┐│
                    │  │ Real IP Hdr: ││  🟢 OS adds
                    │  │  SRC: Bounce ││  🟢 Bounce server IP
                    │  │   104.196... ││
                    │  │  DST: Server ││  🟢 Server's real IP
                    │  │   public IP  ││
                    │  ├──────────────┤│
                    │  │ Real UDP Hdr:││  🟢 OS adds
                    │  │  SRC Port:   ││
                    │  │    3002      ││
                    │  │  DST Port:   ││  🟢 Server's port
                    │  │    54783     ││     (OS-assigned)
                    │  ├──────────────┤│
                    │  │ 🟦 VIRTUAL   ││  🟦 Same 65-byte
                    │  │ Ethernet     ││     blob relayed
                    │  │ Frame        ││     unchanged!
                    │  │ (65 bytes)   ││
                    │  └──────────────┘│
                    └────────┬─────────┘
                             │ 🟢 REAL Internet routing
┌────────────────────────────▼─────────────────────────────────────────────────┐
│                            SERVER MACHINE                                     │
│  ╔══════════════════════════════════════════════════════════════════════╗   │
│  ║              TRANSITION FROM REAL TO VIRTUAL                          ║   │
│  ╚══════════════════════════════════════════════════════════════════════╝   │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ REAL UDP Socket 🟢 REAL OS SOCKET                                     │  │
│  │ • Local address: Server's real IP:54783  🟢 REAL                      │  │
│  │ • Receives UDP from bounce server (OS handles)                        │  │
│  │ • Extracts 65-byte payload →  code sees this as Ethernet frame    │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│  ╔═════════════════════════▼═══════════════════════════════════════════╗   │
│  ║                    VIRTUAL NETWORK (In-Memory)                        ║   │
│  ╚══════════════════════════════════════════════════════════════════════╝   │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Router (Server Side) 🟦 VIRTUAL                                        │  │
│  │ • Interface eth0: 10.0.0.172   🟦 HARDCODED in endtoend.cc            │  │
│  │ • Interface eth1: 172.16.0.1   🟦 HARDCODED in endtoend.cc            │  │
│  │ • Receives frame on eth0                                               │  │
│  │ • Routing table lookup: 🟦 HARDCODED routes                            │  │
│  │   Destination: 172.16.0.100 → Direct delivery on eth1                 │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Router's Host Interface (172.16.0.1) 🟦 VIRTUAL                        │  │
│  │ • Need MAC for: 172.16.0.100                                            │  │
│  │ • ARP resolution for host 🟦 VIRTUAL ARP                               │  │
│  │ • Create new Ethernet frame:                                            │  │
│  │   SRC MAC: Router's eth1 MAC 🟦 RANDOM                                 │  │
│  │   DST MAC: Server host's MAC 🟦 RANDOM                                 │  │
│  │ • Decrement TTL: 63 → 62 🟦  router.cc code                        │  │
│  │ • Recompute IPv4 checksum                                               │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Network Interface (172.16.0.100) 🟦 VIRTUAL                            │  │
│  │ • My MAC: 02:ZZ:ZZ:ZZ:ZZ:02  🟦 RANDOM generated                       │  │
│  │ • My IP: 172.16.0.100        🟦 HARDCODED in endtoend.cc               │  │
│  │ • Receive Ethernet frame                                                │  │
│  │ • Check: DST MAC == My MAC? YES                                         │  │
│  │ • Extract IPv4 datagram                                                 │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │ recv_frame(frame)                                │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Network Layer (IPv4 Processing) 🟦 VIRTUAL                             │  │
│  │ • Verify checksum: OK                                                   │  │
│  │ • Check: DST IP == My IP? YES (172.16.0.100) 🟦 HARDCODED             │  │
│  │ • Protocol: 6 (TCP)                                                     │  │
│  │ • Extract TCP segment                                                   │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ TCP Layer (TCP Receiver) 🟦 VIRTUAL                                    │  │
│  │ • Parse TCP header:                                                     │  │
│  │   - SRC Port: 54321 🟦, DST Port: 1234 🟦 (HARDCODED in endtoend.cc)  │  │
│  │   - SEQ: 1000                                                           │  │
│  │   - ACK: 5000                                                           │  │
│  │   - Flags: ACK, PSH                                                     │  │
│  │ • Verify TCP checksum                                                   │  │
│  │ • Check sequence number (in window?)                                    │  │
│  │ • Pass to Reassembler                                                   │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Reassembler (reassembler.cc) 🟦 VIRTUAL                                │  │
│  │ • Insert data at index: 1000                                            │  │
│  │ • Data: "Hello World" (11 bytes)                                        │  │
│  │ • Check for gaps                                                        │  │
│  │ • If contiguous → Push to ByteStream                                    │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ ByteStream (Inbound) 🟦 VIRTUAL                                        │  │
│  │ • Buffered data: "Hello World"                                          │  │
│  │ • Available for read: 11 bytes                                          │  │
│  └─────────────────────────┬─────────────────────────────────────────────┘  │
│                            │                                                  │
│  ┌─────────────────────────▼─────────────────────────────────────────────┐  │
│  │ Application Layer                                                       │  │
│  │ • Read from socket: "Hello World"                                       │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔑 Legend

| Symbol | Meaning | Where Defined/Generated |
|--------|---------|-------------------------|
| 🟦 | **VIRTUAL** - Simulated in  code |  C++ implementation |
| 🟢 | **REAL** - Actual network operation | OS network stack |
| HARDCODED | Fixed value in source code | `apps/endtoend.cc`, line numbers below |
| RANDOM | Generated at runtime | `random_device()()` in test files |

---

## 📍 Where Things Are Hardcoded (endtoend.cc)

```cpp
// Line ~210-230: Client configuration
if ( is_client ) {
  // HARDCODED: Client virtual network addresses
  host_side = router.add_interface(..., Address { "192.168.0.1" } );      // 🟦
  internet_side = router.add_interface(..., Address { "10.0.0.192" } );   // 🟦
  
  // HARDCODED: Client routing table
  router.add_route( Address { "192.168.0.0" }.ipv4_numeric(), 16, {}, host_side );
  router.add_route( Address { "10.0.0.0" }.ipv4_numeric(), 8, {}, internet_side );
  router.add_route( Address { "172.16.0.0" }.ipv4_numeric(), 12, 
                    Address { "10.0.0.172" }, internet_side );  // 🟦 Route to server
} else {
  // HARDCODED: Server virtual network addresses
  host_side = router.add_interface(..., Address { "172.16.0.1" } );      // 🟦
  internet_side = router.add_interface(..., Address { "10.0.0.172" } );  // 🟦
  
  // HARDCODED: Server routing table
  router.add_route( Address { "172.16.0.0" }.ipv4_numeric(), 12, {}, host_side );
  router.add_route( Address { "10.0.0.0" }.ipv4_numeric(), 8, {}, internet_side );
  router.add_route( Address { "192.168.0.0" }.ipv4_numeric(), 16, 
                    Address { "10.0.0.192" }, internet_side );  // 🟦 Route to client
}

// Line ~240: HARDCODED virtual host addresses
TCPSocketEndToEnd sock = is_client 
  ? TCPSocketEndToEnd { Address { "192.168.0.50" }, Address { "192.168.0.1" } }  // 🟦
  : TCPSocketEndToEnd { Address { "172.16.0.100" }, Address { "172.16.0.1" } };  // 🟦

// Line ~190-200: REAL network configuration
UDPSocket internet_socket;
Address bounce_address { bounce_host, bounce_port };  // 🟢 From command line args

// Example: ./endtoend client cs144.keithw.org 3002
//          bounce_host = "cs144.keithw.org"  🟢 REAL DNS name
//          bounce_port = "3002"               🟢 REAL port number

internet_socket.connect( bounce_address );  // 🟢 REAL OS socket connect

// Line ~320: HARDCODED virtual server endpoint
if ( is_client ) {
  sock.connect( Address { "172.16.0.100", 1234 } );  // 🟦 Virtual destination
} else {
  sock.bind( Address { "172.16.0.100", 1234 } );     // 🟦 Virtual bind
  sock.listen_and_accept();
}
```

---

## Detailed Step-by-Step Breakdown

### Phase 1: CLIENT - Application to Ethernet Frame (🟦 ALL VIRTUAL)

#### Step 1: Application writes data
```cpp
// Application code ( code or test)
string message = "Hello World";
sock.write(message);
// ✓ This is just in-memory data in  process
```

#### Step 2: ByteStream buffers data (🟦 VIRTUAL)
```cpp
// byte_stream.cc -  implementation
ByteStream outbound;
outbound.write("Hello World");  // 11 bytes buffered in std::queue
// ✓ Memory buffer, no OS involvement yet
```

#### Step 3: TCP Sender creates segment (🟦 VIRTUAL)
```cpp
// tcp_sender.cc -  implementation
TCPSenderMessage msg;
msg.seqno = Wrap32::wrap(1000, isn);  // Sequence number from  TCP state
msg.payload = "Hello World";
msg.SYN = false;
msg.FIN = false;
msg.RST = false;
// ✓ All state maintained in  C++ objects
// ✓ No kernel TCP stack involved!
```

#### Step 4: TCP wraps in TCPMessage (🟦 VIRTUAL)
```cpp
// TCPMessage structure -  virtual TCP
TCPMessage tcp_msg {
  .sender_message = msg,
  .receiver_message = {
    .ackno = Wrap32::wrap(5000, peer_isn),
    .window_size = 65535
  }
};

// Serialize to TCP header + payload
// SRC:54321 🟦 HARDCODED (line ~315 in endtoend.cc)
// DST:1234  🟦 HARDCODED (line ~320 in endtoend.cc)
// + "Hello World"
```

#### Step 5: IPv4 wraps TCP segment (🟦 VIRTUAL)
```cpp
// network_interface.cc → send_datagram()
InternetDatagram dgram;
dgram.header.src = 0xC0A80032;  // 192.168.0.50  🟦 HARDCODED (line ~240)
dgram.header.dst = 0xAC100064;  // 172.16.0.100  🟦 HARDCODED (line ~320)
dgram.header.proto = 6;          // TCP
dgram.header.ttl = 64;           // 🟦 Initial value
dgram.payload = serialize(tcp_msg);
dgram.header.len = 20 + tcp_msg.size();
dgram.header.compute_checksum();

// ✓ This IPv4 packet exists only in memory
// ✓ OS will never see this IP header!
```

#### Step 6: Network Interface creates Ethernet frame (🟦 VIRTUAL)
```cpp
// network_interface.cc -  implementation
void NetworkInterface::send_datagram(const InternetDatagram& dgram, 
                                     const Address& next_hop) {
  // next_hop = 192.168.0.1 (router) 🟦 VIRTUAL address
  uint32_t next_hop_ip = next_hop.ipv4_numeric();
  
  // ARP lookup (🟦 VIRTUAL ARP cache)
  if (ARP_cache_.contains(next_hop_ip)) {
    EthernetAddress dst_mac = ARP_cache_[next_hop_ip].first;  // 🟦 VIRTUAL MAC
    
    EthernetFrame frame;
    frame.header.dst = dst_mac;              // Router's MAC (🟦 RANDOM)
    frame.header.src = ethernet_address_;    // My MAC (🟦 RANDOM)
    frame.header.type = EthernetHeader::TYPE_IPv4;  // 0x0800
    frame.payload = serialize(dgram);
    
    transmit(frame);  // Send to OutputPort (Unix socket, not real wire!)
  } else {
    // Queue datagram, send ARP request (🟦 ALL VIRTUAL)
    dgrams_waitting_[next_hop_ip].push_back(dgram);
    ARPMessage arp_req = make_arp(ARPMessage::OPCODE_REQUEST, {}, next_hop_ip);
    transmit({
      {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP},
      serialize(arp_req)
    });
  }
}

// ✓ Ethernet frame exists only in memory
// ✓ No real NIC (network card) involved!
```

### Phase 2: CLIENT Router Processing (🟦 ALL VIRTUAL)

#### Step 7: Router receives frame (🟦 VIRTUAL routing)
```cpp
// endtoend.cc → Router processing
// Unix socket read simulates "receiving" frame from virtual wire
event_loop.add_rule("frames from host to router", 
  sock.adapter().frame_fd(), Direction::In, [&] {
  
  EthernetFrame frame = receive_frame();  // Read from Unix socket
  // Frame received on VIRTUAL interface eth0 (192.168.0.1) 🟦
  
  router.interface(host_side)->recv_frame(frame);
  router.route();
});

// ✓ This is inter-process communication (Unix socket)
// ✓ Not real Ethernet!
```

#### Step 8: Router routes packet (🟦 VIRTUAL routing table)
```cpp
// router.cc -  implementation
void Router::route() {
  for (auto& interface : interfaces_) {
    while (!interface->datagrams_received().empty()) {
      InternetDatagram dgram = interface->datagrams_received().front();
      
      // Routing table lookup (🟦 HARDCODED in endtoend.cc line ~210-230)
      // DST: 172.16.0.100 → Match route: via 10.0.0.172 on eth1
      
      optional<RouteEntry> match = longest_prefix_match(dgram.header.dst);
      
      if (match) {
        // Decrement TTL (🟦  CODE modifies virtual IP header)
        dgram.header.ttl--;  // 64 → 63
        if (dgram.header.ttl == 0) {
          // Drop packet (🟦 virtual drop, just don't forward)
          continue;
        }
        dgram.header.compute_checksum();  // Recompute for modified header
        
        // Send out internet-facing interface (🟦 VIRTUAL forwarding)
        Address next_hop = match->next_hop.has_value() 
                          ? match->next_hop.value() 
                          : Address::from_ipv4_numeric(dgram.header.dst);
        
        interfaces_[match->interface_id]->send_datagram(dgram, next_hop);
      }
      
      interface->datagrams_received().pop();
    }
  }
}

// ✓ All routing happens in  code
// ✓ OS routing table not involved!
```

### Phase 3: Real UDP Transport to Bounce Server (🟢 REAL NETWORK!)

#### Step 9: Send via real UDP socket (🟢 FIRST REAL NETWORK OPERATION!)
```cpp
// endtoend.cc
event_loop.add_rule("frames from router to Internet",
  internet_socket, Direction::Out, [&] {
  
  EthernetFrame& frame = router_to_internet->frames.front();
  
  // This is a REAL UDP socket! 🟢
  // Connected to: cs144.keithw.org:3002 (🟢 REAL DNS, REAL IP)
  internet_socket.write(serialize(frame));  // 🟢 OS system call!
  // Writes 65-byte Ethernet frame as UDP payload
  
  router_to_internet->frames.pop();
});

// What the OS does (🟢 REAL):
// 1. Takes 65-byte buffer ( Ethernet frame)
// 2. Adds REAL UDP header (8 bytes)
//    - SRC Port: OS-assigned ephemeral port (e.g., 54782)
//    - DST Port: 3002 (from  connect() call)
// 3. Adds REAL IPv4 header (20 bytes)
//    - SRC IP:  machine's real IP
//    - DST IP: 104.196.238.229 (bounce server, from DNS resolution)
// 4. Adds REAL Ethernet header (if on Ethernet)
//    - SRC MAC:  NIC's real MAC
//    - DST MAC: Next hop router's MAC (from OS ARP)
// 5. Sends on REAL network interface (eth0/wlan0/etc.)
```

**Real UDP Packet Structure (🟢 OS creates this):**
```
┌─────────────────────────┐
│ Real Ethernet Header    │ 🟢 Added by OS
│  SRC:  NIC MAC      │
│  DST: Gateway MAC       │
├─────────────────────────┤
│ Real IP Header          │ 🟢 Added by OS
│  SRC:  public IP    │ 🟢 From  network config
│  DST: 104.196.238.229   │ 🟢 Bounce server (DNS resolved)
│  Protocol: 17 (UDP)     │
├─────────────────────────┤
│ Real UDP Header         │ 🟢 Added by OS
│  SRC Port: 54782        │ 🟢 OS-assigned
│  DST Port: 3002         │ 🟢 From  code
│  Length: 73 (8+65)      │
├─────────────────────────┤
│ Payload (65 bytes):     │ 🟦 From  code
│  ┌──────────────────┐   │
│  │ Ethernet Header  │   │ 🟦 VIRTUAL ( code made this)
│  │  DST: Router MAC │   │ 🟦 Random virtual MAC
│  │  SRC: Host MAC   │   │ 🟦 Random virtual MAC
│  │  Type: IPv4      │   │
│  ├──────────────────┤   │
│  │ IPv4 Header      │   │ 🟦 VIRTUAL ( code made this)
│  │  SRC: 192.168..  │   │ 🟦 Hardcoded virtual IP
│  │  DST: 172.16..   │   │ 🟦 Hardcoded virtual IP
│  │  Proto: TCP      │   │
│  ├──────────────────┤   │
│  │ TCP Header+Data  │   │ 🟦 VIRTUAL ( code made this)
│  │  "Hello World"   │   │
│  └──────────────────┘   │
└─────────────────────────┘

IMPORTANT: 
- Outer layers (Real Ethernet/IP/UDP): OS handles, uses REAL addresses 🟢
- Inner layers (Virtual Ethernet/IP/TCP):  code created, uses VIRTUAL addresses 🟦
- OS treats inner 65 bytes as opaque binary data!
```

### Phase 4: Bounce Server Processing (🟢 REAL UDP, 🟦 VIRTUAL relay)

#### Step 10: Bounce server receives and relays
```python
# Conceptual bounce server logic (Stanford's server, not  Python script)
# This is a REAL server running on REAL hardware! 🟢

class BounceServer:
    def __init__(self):
        # REAL socket 🟢
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', 3002))  # 🟢 REAL port binding
        
        self.clients = {}  # Maps: (real_ip, real_port) → (peer_real_ip, peer_real_port)
        
    def handle_packet(self, data, addr):
        # data = 65-byte blob ( Ethernet frame) 🟦
        # addr = (client_real_ip, client_real_port) 🟢 From OS
        
        # Bounce server does NOT parse Ethernet/IP/TCP! 🟦
        # It just treats data as opaque 65-byte blob
        
        # Client registration (happens when first packet arrives)
        if addr not in self.clients:
            # Wait for peer to connect
            self.pending[addr] = data
            return
        
        # Find peer (based on UDP address pairing) 🟢
        peer_addr = self.clients[addr]  # (peer_real_ip, peer_real_port)
        
        # Relay: Send the SAME 65-byte blob to peer via REAL UDP 🟢
        self.sock.sendto(data, peer_addr)  # OS system call
        
    def register_clients(self, client_a_addr, client_b_addr):
        # Pair clients so they can communicate
        # These are REAL IP:port pairs! 🟢
        self.clients[client_a_addr] = client_b_addr
        self.clients[client_b_addr] = client_a_addr
```

**Key Insight:** The bounce server:
1. ✅ Uses REAL UDP sockets (OS-managed) 🟢
2. ✅ Knows REAL client IP addresses and ports 🟢
3. ❌ Does NOT understand the 65-byte payload (Ethernet frame) 🟦
4. ✅ Simply relays bytes between paired clients
5. ❌ Cannot read  "Hello World" (inside virtual TCP, inside virtual IP, inside virtual Ethernet!)

### Phase 5: SERVER - Receive and Process

#### Step 11: Server receives UDP from bounce (🟢 REAL → 🟦 VIRTUAL transition)
```cpp
// Server's endtoend.cc
event_loop.add_rule("frames from Internet to router",
  internet_socket, Direction::In, [&] {
  
  // Real UDP receive (🟢 OS gives us data)
  // OS has stripped off: Real Ethernet, Real IP, Real UDP headers
  // We get: 65-byte payload (our virtual Ethernet frame) 🟦
  
  EthernetFrame frame = receive_frame(internet_socket);
  
  // Give to router's internet-facing interface (🟦 back to virtual world)
  router.interface(internet_side)->recv_frame(frame);
  router.route();  // Virtual routing
});

// ✓ Transition point: Real network → Virtual network
```

#### Step 12: Server router processes (🟦 VIRTUAL routing)
```cpp
// router.cc on server
// Frame arrives on VIRTUAL eth0 (10.0.0.172) 🟦
// DST IP: 172.16.0.100 🟦 (from virtual IP header)
// Routing table: 172.16.0.0/12 → direct on eth1 (172.16.0.1) 🟦 HARDCODED

void Router::route() {
  // ... same routing logic as client router ...
  // Decrement TTL: 63 → 62 🟦
  // Forward to host-side interface (🟦 VIRTUAL)
  interfaces_[host_side]->send_datagram(dgram, Address{"172.16.0.100"});
}
```

#### Step 13: Server network interface receives (🟦 VIRTUAL)
```cpp
// network_interface.cc on server (172.16.0.100)
void NetworkInterface::recv_frame(const EthernetFrame& frame) {
  // Check destination
  if (frame.header.dst != ethernet_address_ && 
      frame.header.dst != ETHERNET_BROADCAST) {
    return;  // Not for me
  }
  
  // IPv4 datagram
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) {
      // Verify: dgram.header.dst == my IP (172.16.0.100)
      // Protocol: 6 (TCP)
      datagrams_received_.push(dgram);
    }
  }
}
```

#### Step 14: TCP receiver processes
```cpp
// tcp_receiver.cc
void TCPReceiver::receive(const TCPSenderMessage& msg) {
  // msg.seqno = 1000
  // msg.payload = "Hello World"
  
  if (msg.RST) {
    // Handle reset
    return;
  }
  
  // Convert seqno to stream index
  uint64_t checkpoint = bytes_pushed;
  uint64_t abs_seqno = msg.seqno.unwrap(isn, checkpoint);
  uint64_t stream_index = abs_seqno - 1;  // -1 for SYN
  
  // Pass to reassembler
  reassembler_.insert(stream_index, msg.payload, msg.FIN);
}
```

#### Step 15: Reassembler processes
```cpp
// reassembler.cc
void Reassembler::insert(uint64_t first_index, 
                        string data, 
                        bool is_last_substring) {
  // first_index = 999 (stream index)
  // data = "Hello World"
  
  // Check if in window
  uint64_t next_expected = output_.writer().bytes_pushed();
  uint64_t available_capacity = output_.writer().available_capacity();
  
  if (first_index >= next_expected && 
      first_index < next_expected + available_capacity) {
    
    // Store in buffer if not contiguous
    // Or push directly if next expected
    if (first_index == next_expected) {
      output_.writer().push(data);
      bytes_pending_ -= data.size();
    } else {
      // Buffer for later
      pending_data_[first_index] = data;
      bytes_pending_ += data.size();
    }
  }
  
  if (is_last_substring) {
    output_.writer().close();
  }
}
```

#### Step 16: Application reads data
```cpp
// Application on server
string received = sock.read(11);
// received = "Hello World"

cout << "Received: " << received << endl;
```

---

## Return Path (ACK)

The ACK follows the **exact reverse path**:

```
Server ByteStream → TCP Sender (ACK) → IPv4 → Ethernet → 
Router → UDP → Bounce Server → UDP → 
Router → Ethernet → IPv4 → TCP Receiver (ACK) → Client
```

**TCP ACK Message:**
```cpp
TCPMessage ack_msg {
  .sender_message = {
    .seqno = Wrap32::wrap(5000, server_isn),
    .payload = "",  // No data, just ACK
    .FIN = false,
  },
  .receiver_message = {
    .ackno = Wrap32::wrap(1011, client_isn),  // 1000 + 11 bytes
    .window_size = 65535
  }
};
```

---

## Key Address Translations

### Client Side:
| Layer | Source | Destination |
|-------|--------|-------------|
| Application | - | - |
| TCP | Port 54321 | Port 1234 |
| IPv4 (virtual) | 192.168.0.50 | 172.16.0.100 |
| Ethernet (virtual) | 02:XX:XX:XX:XX:01 | 02:00:00:YY:YY:01 (router) |
| **Router transforms** | | |
| Ethernet (virtual) | 02:00:00:YY:YY:02 (router) | 02:AA:AA:AA:AA:01 |
| IPv4 (virtual) | 192.168.0.50 | 172.16.0.100 |
| **Real UDP wrapping** | | |
| UDP (real) | Port 54782 | Port 3002 |
| IPv4 (real) |  public IP | 104.196.238.229 |

### Bounce Server:
- **Receives:** Real UDP from Client A
- **Extracts:** Virtual Ethernet frame
- **Forwards:** Same Ethernet frame in Real UDP to Client B
- **No modification** to inner layers!

### Server Side:
| Layer | Source | Destination |
|-------|--------|-------------|
| IPv4 (real) | 104.196.238.229 | Server public IP |
| UDP (real) | Port 3002 | Port 54783 |
| **Real UDP unwrapping** | | |
| Ethernet (virtual) | 02:AA:AA:AA:AA:01 | 02:BB:BB:BB:BB:02 (router) |
| **Router transforms** | | |
| Ethernet (virtual) | 02:BB:BB:BB:BB:01 (router) | 02:ZZ:ZZ:ZZ:ZZ:02 |
| IPv4 (virtual) | 192.168.0.50 | 172.16.0.100 |
| TCP | Port 54321 | Port 1234 |
| Application | - | - |

---

## Summary

1. **Virtual TCP/IP stack** runs completely inside  process
2. **Virtual Ethernet frames** are serialized and sent via **real UDP**
3. **Bounce server** simply relays Ethernet frames between paired clients
4. **No middle box** (except bounce server) sees the TCP/IP details
5. **End-to-end encryption** possible at TCP layer (bounce server can't read)

This is essentially **VPN/tunnel** technology - encapsulating one protocol inside another! 🚀
