# CS39006 Networks Lab – Assignment 4 Documentation

## Overview

This assignment implements an end-to-end reliable flow-control protocol (named KTP) over an unreliable channel (using UDP sockets). The solution builds a library that provides functions similar to standard socket APIs – namely, `k_socket()`, `k_bind()`, `k_sendto()`, `k_recvfrom()`, and `k_close()`. In addition, the library implements a window-based flow control mechanism with a sender window (swnd) and receiver window (rwnd) to guarantee ordered delivery despite UDP’s unreliability. The library also includes simulated packet drops via a `dropMessage()` function and a retransmission mechanism managed by a dedicated sender thread. The receiver thread handles incoming packets, processes ACKs, updates the receiver window, and sends ACK messages (piggybacking available receive buffer space).

## Data Structures

### data_buffer

- **Purpose:** Implements a circular queue used for both the send buffer and the receive buffer.
- **Fields:**
  - `_buf[BUFFER_SIZE][MSSG_SIZE+1]`: Two-dimensional character array to hold messages.
  - `head`: Index of the first (oldest) element.
  - `tail`: Index where the next new message will be inserted.
  - `count`: Current number of messages in the buffer.
- **Functions:**  
  `initCircularArray()`, `isFull()`, `isEmpty()`, `enqueue()`, `dequeue()`, `sizeOfCircularArray()`, `print_buff()`, and `getKMessages()` provide queue functionality.

### swnd (Sender Window)

- **Purpose:** Tracks information for the sender side of the sliding window.
- **Fields:**
  - `next_seq_num`: The next sequence number to be used for outgoing packets.
  - `window_size`: The maximum number of outstanding messages permitted; initialized to the constant `WINDOW_SIZE`.
  - `available_rwnd`: Represents the current free capacity of the receiver’s buffer (as piggybacked via ACKs).
  - `sent_seq_nums[MAX_SEQ_NUM+1]`: An array used to track outstanding packets. A value of 0 indicates an outstanding packet; 1 indicates that it has been acknowledged.
  - `send_times[MAX_SEQ_NUM+1]`: Records the send timestamp for each packet, used to determine timeouts.

### rwnd (Receiver Window)

- **Purpose:** Manages the receive side of the sliding window.
- **Fields:**
  - `next_expected_seq`: The next expected sequence number; used to check if a packet is in order.
  - `window_size`: Fixed size representing the capacity of the receiver buffer.
  - `seq_nums_map[MAX_SEQ_NUM+1]`: Maps sequence numbers to a status (1 if the packet has been received, 0 otherwise).
  - `stash_buffer[MAX_SEQ_NUM+1][MSSG_SIZE+1]`: Temporarily stores out-of-order packets.
  - `free_space`: The number of additional messages that can be accepted in the receiver buffer.
  - `last_ack_sent`: Holds the sequence number of the last in-order message that was acknowledged.
  
### ktp_sockaddr

- **Purpose:** Represents a KTP socket; holds all the information for a single KTP connection.
- **Fields:**
  - `process_id`: PID of the process that created the socket.
  - `udp_fd`: Underlying UDP socket descriptor.
  - `des_port`, `des_ip`: Destination port and IP address.
  - `src_port`, `src_ip`: Source port and IP address.
  - `send_buf` and `recv_buf`: Data buffers (of type `data_buffer`) for outgoing and incoming messages.
  - `swnd`: The sender window structure.
  - `rwnd`: The receiver window structure.
  - `last_send_time`: Timestamp of the last packet sent.
  - `nospace_flag`: Flag used for flow control when the receive buffer is full.
  - `bind_status`: Indicates the bind state (e.g., UNBINDED, AWAIT_BIND, BINDED).
  - `close_status`: Indicates whether the socket is open or waiting to be closed.

### ktp_header

- **Purpose:** Defines the header for KTP packets.
- **Fields:**
  - `seq_num`: 8-bit sequence number of the packet.
  - `ack_num`: 8-bit acknowledgment number.
  - `is_ack`: Flag indicating if the packet is an ACK (1) or data (0).
  - `rwnd_size`: Piggybacked receiver window size (used for flow control).

## Function Descriptions

### Library Interface Functions

- **k_socket()**  
  Creates a new KTP socket entry (allocating an underlying UDP socket and updating shared structures). Returns a socket index or -1 on error.

- **k_bind()**  
  Binds the created KTP socket to the provided local IP/port and sets the expected remote IP/port.

- **k_sendto()**  
  Appends a message (with a proper KTP header) to the send buffer and transmits it over the UDP socket. Returns the number of bytes sent, or -1 if there is no space in the buffer or if the destination does not match the bound address.

- **k_recvfrom()**  
  Retrieves a message from the receiver buffer. Returns the length of the message or -1 if no message is available.

- **k_close()**  
  Cleans up a KTP socket entry and marks it as closed.

### Helper Functions

- **initCircularArray(), isFull(), isEmpty(), enqueue(), dequeue(), sizeOfCircularArray(), print_buff(), getKMessages()**  
  Implement the circular queue functionality used by send and receive buffers.

- **pkt_create() and extract_pkt()**  
  Serialize and deserialize the KTP header along with the message payload.

- **print_header()**  
  Displays the fields of a KTP packet header for debugging.

- **init_semaphore()**  
  Opens the semaphore used to coordinate access to shared structures.

- **setCustomError(), getCustomError(), getCustomErrorMessage()**  
  Provide error handling and messaging.

### Thread Functions

- **R (Receiver Thread):**  
  Monitors all active UDP sockets using `select()`. When a packet is received:
  - Simulates packet loss (via `dropMessage()`).
  - Extracts the packet header and message.
  - For ACK packets:
    - If the ACK equals `swnd.next_seq_num`, it is considered in-order. The thread updates `swnd.next_seq_num`, marks the corresponding entry in `sent_seq_nums` as acknowledged (set to 1), updates the sender window size, and dequeues the acknowledged message.
    - If the ACK is within the window (using circular arithmetic), the thread iterates from `swnd.next_seq_num` up to the received ACK number, marking each outstanding packet as acknowledged, dequeuing each message, and updating `swnd.next_seq_num` accordingly.
  - For data packets:
    - The thread calls `update_rwnd()` to update the receiver window, process in-order or out-of-order packets, and then sends an ACK with the updated receiver window size.

- **S (Sender Thread):**  
  Periodically (every T/2 seconds) checks for packets that have timed out and need retransmission. It retrieves outstanding packets from the send buffer (using `getKMessages()`), retransmits those packets, and updates the send timestamps.

- **Garbage Collection (within the main loop):**  
  The main function monitors socket entries (using `kill(…, 0)`) to detect if a process has terminated without calling `k_close()`. Such sockets are then cleaned up to maintain system stability.

## Testing and Observations

### Test Setup

The library was tested using multiple user programs:
- **UserA (Sender):** Sends messages or file chunks sequentially. It waits and retries when the send buffer is full (i.e. when `k_sendto()` returns -1).
- **UserB (Receiver):** Continuously receives messages, writes them to an output file, and sends ACKs back to the sender.

### Edge Cases

- **Wrap-Around:**  
  The circular sequence number space (1 to MAX_SEQ_NUM) is handled using modular arithmetic. Test cases cover scenarios where sequence numbers wrap from near MAX_SEQ_NUM back to 1.
- **Out-of-Order Packets:**  
  The `update_rwnd()` function stashes out-of-order packets and later delivers them to the receiver buffer once the missing packet arrives.
- **Timeout and Retransmission:**  
  The sender thread monitors and retransmits packets that exceed the timeout threshold, ensuring reliable delivery.

| P Value | Total Messages | Transmissions | Ratio  |
|--------:|---------------:|--------------:|-------:|
|   0.05  |            276 |           316 | 1.14   |
|   0.10  |            276 |           382 | 1.38   |
|   0.15  |            276 |           467 | 1.69   |
|   0.20  |            276 |           531 | 1.92   |
|   0.25  |            276 |           633 | 2.29   |
|   0.30  |            276 |           687 | 2.49   |
|   0.35  |            276 |           957 | 3.46   |
|   0.40  |            276 |          1130 | 4.09   |
|   0.45  |            276 |          1534 | 5.56   |
|   0.50  |            276 |          1528 | 5.53   |

