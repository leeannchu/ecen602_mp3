# ECEN 602 MP3: UDP TFTP Server

This project implements a teaching implementation of a TFTP server in C++.
The server uses UDP sockets and follows the basic TFTP packet format defined by
RFC 1350. The current completed transfer path is a read request (RRQ), which
allows a client to download a file from the server.

## Architecture

The implementation is contained in [`server.cpp`](C:/Users/chu05/ecen602/ecen602_mp3/server.cpp).

- **Request listener:** `main` creates and binds a UDP socket, validates incoming
  datagrams, and accepts only RRQ and WRQ opcodes.
- **Concurrent transfers:** Each valid request is handled by a forked child
  process. The parent returns immediately to the listening loop. A `SIGCHLD`
  handler reaps completed children.
- **Transfer socket/TID:** A child closes the listening socket and binds an
  ephemeral UDP port for the individual transfer, as required by TFTP's
  transfer identifier (TID) behavior.
- **Packet helpers:** The code encodes and decodes RRQ, WRQ, DATA, ACK, and
  ERROR packets. DATA packets contain at most 512 bytes of file data.
- **Flow control and reliability:** RRQ uses stop-and-wait transmission.
  Blocks begin at 1, each DATA packet must receive a matching ACK, and a
  packet is retransmitted after a one-second timeout. A transfer is abandoned
  after ten consecutive timeouts.
- **File modes:** `octet` reads raw bytes. `netascii` translates local
  newlines to TFTP netascii while sending.
- **Validation and errors:** Requests are checked for a valid filename/mode,
  malformed packets are rejected, path separators are blocked, and TFTP
  ERROR packets are returned for conditions such as missing files, access
  violations, and unknown transfer IDs.

## Building

The project uses `g++`, POSIX sockets, and `make`, so it should be built on a
Linux/Unix environment or a compatible environment such as WSL:

```sh
make
```

The executable is named `server`. To remove generated files:

```sh
make clean
```

## Usage

Start the server with a UDP port number:

```sh
./server <port>
```

For example:

```sh
./server 6969
```

Run the server from the directory containing the files that should be
downloadable. A TFTP client can then issue an RRQ using either `octet` or
`netascii` mode. The server prints the listening port, request information,
child-process information, and transfer completion/timeout messages to
standard output.

The port must be an integer from 1 through 65535. The server does not daemonize
itself, so press `Ctrl+C` to stop it.

## Team roles

- **Lian Chu :** Designed and implemented the initial TFTP
  packet structures and encoding/decoding helpers, including the RRQ/WRQ
  request handling foundation and file-mode translation helpers. Also
  integrated and documented the protocol behavior.
- **Daniel Shyu:** Implemented the Makefile/build setup, completed the
  server's main receive/bind/fork flow, and integrated the basic RRQ transfer
  path, timeout handling, validation, and error handling.
