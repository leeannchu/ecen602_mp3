#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>

//define TFTP packets with their opcodes
#define TFTP_RRQ 1
#define TFTP_WRQ 2
#define TFTP_DATA 3
#define TFTP_ACK 4
#define TFTP_ERROR 5
#define MAX_DATA_SIZE 512
#define MAX_PKT (4 + MAX_DATA_SIZE)   // 516: largest legal TFTP packet


//define RRQ and WRQ(bonus) packet structure
char rrq_buffer[516]; // buffer to hold the RRQ packet
char wrq_buffer[516]; // buffer to hold the WRQ packet
//decode an RRQ packet
void decode_rrq_packet(const char* buffer, int packet_length, uint16_t* opcode, char* filename, char* mode)
{
    uint16_t network_opcode;
    memcpy(&network_opcode, buffer, 2);
    *opcode = ntohs(network_opcode);
    strcpy(filename, buffer + 2);
    strcpy(mode, buffer + 2 + strlen(filename) + 1);
}
//decode a WRQ packet
void decode_wrq_packet(const char* buffer, int packet_length, uint16_t* opcode, char* filename, char* mode)
{
    uint16_t network_opcode;
    memcpy(&network_opcode, buffer, 2);
    *opcode = ntohs(network_opcode);
    strcpy(filename, buffer + 2);
    strcpy(mode, buffer + 2 + strlen(filename) + 1);
}
//define DATA packet structure
struct __attribute__((__packed__)) tftp_data_packet
{
    uint16_t opcode; // 2 bytes for opcode
    uint16_t block_number; // 2 bytes for block number
    char data[MAX_DATA_SIZE];
};
// Decode a DATA packet. 
void decode_data_packet(const char* buffer, int packet_length, uint16_t* opcode, uint16_t* block_number, char* data, int* data_length)
{
    uint16_t network_opcode, network_block;
    memcpy(&network_opcode, buffer, 2);
    memcpy(&network_block, buffer + 2, 2);
    *opcode = ntohs(network_opcode);
    *block_number = ntohs(network_block);
    *data_length = packet_length - 4;
    memcpy(data, buffer + 4, *data_length);
}
// Encode a DATA packet.
void encode_data_packet(char* buffer, uint16_t block_number, const char* data, int data_length, int* packet_length)
{
    uint16_t network_opcode = htons(TFTP_DATA);
    uint16_t network_block = htons(block_number);
    memcpy(buffer, &network_opcode, 2);
    memcpy(buffer + 2, &network_block, 2);
    memcpy(buffer + 4, data, data_length);
    *packet_length = 4 + data_length;
}
// Check the final DATA packet. If the data length is less than 512 bytes, it indicates the final packet.
bool is_final_data_packet(int data_length)
{
    if (data_length < MAX_DATA_SIZE && data_length >= 0) {
        return true; // Final packet
    } else {
        return false; // Not final packet
    }
}



//define ACK packet structure
struct __attribute__((__packed__)) tftp_ack_packet
{
    uint16_t opcode; // 2 bytes for opcode
    uint16_t block_number; // 2 bytes for block number
};
//decode an ACK packet
void decode_ack_packet(const char* buffer, int packet_length, uint16_t* opcode, uint16_t* block_number)
{
    uint16_t network_opcode, network_block;
    memcpy(&network_opcode, buffer, 2);
    memcpy(&network_block, buffer + 2, 2);
    *opcode = ntohs(network_opcode);
    *block_number = ntohs(network_block);
}
// Encode an ACK packet.
void encode_ack_packet(char* buffer, uint16_t block_number)
{
    uint16_t network_opcode = htons(TFTP_ACK);
    uint16_t network_block = htons(block_number);
    memcpy(buffer, &network_opcode, 2);
    memcpy(buffer + 2, &network_block, 2);
}
//define ERROR packet structure
char error_buffer[516]; // buffer to hold the ERROR packet
// Decode an ERROR packet
void decode_error_packet(const char* buffer, int packet_length, uint16_t* opcode, uint16_t* error_code, char* error_message)
{
    uint16_t network_opcode, network_error_code;
    memcpy(&network_opcode, buffer, 2);
    memcpy(&network_error_code, buffer + 2, 2);
    *opcode = ntohs(network_opcode);
    *error_code = ntohs(network_error_code);
    strcpy(error_message, buffer + 4);
}
// Encode an ERROR packet.
void encode_error_packet(char* buffer, uint16_t error_code, const char* error_message, int* packet_length)
{
    uint16_t network_opcode = htons(TFTP_ERROR);
    uint16_t network_error_code = htons(error_code);
    memcpy(buffer, &network_opcode, 2);
    memcpy(buffer + 2, &network_error_code, 2);
    strcpy(buffer + 4, error_message);
    *packet_length = 4 + strlen(error_message) + 1;
}
// netascii translation functions
int read_netascii_file(FILE *fp, char* buffer)
{
    int count = 0, c;
    static int nextchar = -1; //static variable to hold the next char
    for (count = 0; count < MAX_DATA_SIZE; count++){
        if(nextchar >= 0){ //if there is a next char, write it to buffer
            buffer[count] = nextchar;
            nextchar = -1;
            continue;
        }
        c = getc(fp);
        if (c == EOF) { //EOF　return means eof or error
            if (ferror(fp)) {
                perror("read err from getc on local file");
                return(count);
            }
            break;
        }else if (c == '\n') {
            c = '\r'; //newline: '\n' -> '\r\n'
            nextchar = '\n';
        } else if (c == '\r') {
            nextchar = '\0'; //'\r' -> '\r\0'
        } else {
            nextchar = -1;
        }
        buffer[count] = c;
    }
    return count;
}
// octets translation functions
int read_octets_file(FILE *fp, char* buffer)
{
    int bytes_read = fread(buffer, 1, MAX_DATA_SIZE, fp);
    if (ferror(fp)) {
        perror("read err from fread on local file");
        return -1;
    }
    return bytes_read;
}
// write netascii file (Bonus)
int write_netascii_file(FILE *fp, const char* buffer, int data_length)
{
    int count = 0, c;
    static int prevchar = -1; //static variable to hold the previous char
    for (count = 0; count < data_length; count++){
        c = buffer[count];
        if (prevchar == '\r') {
            if (c == '\n') {
                putc('\n', fp); //'\r\n' -> '\n'
            } else if (c == '\0') {
                putc('\r', fp); //'\r\0' -> '\r'
            } else {
                putc('\r', fp); //'\r' -> '\r'
                putc(c, fp);
            }
            prevchar = -1;
        } else {
            if (c == '\r') {
                prevchar = c;
            } else {
                putc(c, fp);
            }
        }
        if (ferror(fp)) {
            perror("write err from putc on local file");
            return -1;
        }
    }
    return data_length;
}
// write octets file (Bonus)
int write_octets_file(FILE *fp, const char* buffer, int data_length)
{
    int bytes_written = fwrite(buffer, 1, data_length, fp);
    if (ferror(fp)) {
        perror("write err from fwrite on local file");
        return -1;
    }
    return bytes_written;
}
// Check that an RRQ/WRQ really holds "filename\0mode\0" inside its length,
// so decode_rrq_packet()/decode_wrq_packet() (which use strcpy) are safe.
bool is_valid_request(const char *buf, int len)
{
    if (len < 4) return false;
    const char *p   = buf + 2;
    const char *end = buf + len;
    const char *z1  = (const char *)memchr(p, '\0', end - p);            // end of filename
    if (z1 == NULL || z1 == p) return false;                             // missing or empty
    const char *z2  = (const char *)memchr(z1 + 1, '\0', end - (z1 + 1)); // end of mode
    return z2 != NULL && z2 != z1 + 1;                                   // mode present and non-empty
}
 
// Build and send an ERROR packet 
void send_error(int sock, const struct sockaddr_in *to, uint16_t code, const char *msg)
{
    char pkt[MAX_PKT];
    int len;
    encode_error_packet(pkt, code, msg, &len);
    sendto(sock, pkt, len, 0, (const struct sockaddr *)to, sizeof(*to));
}
// Reap every finished child so none become zombies.
void sigchld_handler(int signo){
    (void)signo;
    int saved_errno = errno;              // waitpid may change errno; restore it
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}
int main(int argc, char *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    char *end;
    //check port valid or not 
    long port = strtol(argv[1], &end, 10);
    if (*end != '\0' || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        exit(1);
    }
    // print each line immediately; no duplicates after fork()
    setvbuf(stdout, NULL, _IOLBF, 0);     
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;             // restart recvfrom() automatically after the signal
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
    //open a listen socket 
    int listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenfd < 0) {
        perror("socket");
        exit(1);
    }
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons((uint16_t)port);
    //bind socket to the port 
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        exit(1);
    }
    printf("TFTP server listening on UDP port %ld\n", port); 
    char buf[MAX_PKT + 1];            
    //infinitely receive request from any client, if valid request, fork a child to handle the request, parent will go back to listen for next request
    for (;;) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        ssize_t n = recvfrom(listenfd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&cliaddr, &clilen);
        //sanity check for receive frame size, refuse obviously invalid packets.
        if (n < 0) {
            // interrupted by a signal (matters from Step 2)
            if (errno == EINTR) continue;     
            perror("recvfrom");
            continue;
        }
        if (n == (ssize_t)sizeof(buf)) {      
            // larger than any legal TFTP packet
            fprintf(stderr, "Oversized datagram ignored\n");
            continue;
        }
        if (n < 2) {                          
            // not an opcode
            fprintf(stderr, "Runt datagram ignored\n");
            continue;
        }
        //terminator for strcpy
        buf[n] = '\0';                       
        
        // connection handle for client.
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
        // decode the opcode from the request packet, check validity
        uint16_t opcode;
        memcpy(&opcode, buf, 2);
        opcode = ntohs(opcode);
        // Only RRQ/WRQ may start a transfer.
        if (opcode != TFTP_RRQ && opcode != TFTP_WRQ) {
            printf("opcode %u from %s:%d is not a request -> ERROR 4\n",
                   opcode, ip, ntohs(cliaddr.sin_port));
            send_error(listenfd, &cliaddr, 4, "Illegal TFTP operation");
            continue;
        }
        if (!is_valid_request(buf, (int)n)) {
            printf("malformed request from %s:%d -> ERROR 4\n", ip, ntohs(cliaddr.sin_port));
            send_error(listenfd, &cliaddr, 4, "Malformed request");
            continue;
        }
        //filename handle
        char filename[MAX_PKT], mode[MAX_PKT];
        if (opcode == TFTP_RRQ)
            decode_rrq_packet(buf, (int)n, &opcode, filename, mode);
        else
            decode_wrq_packet(buf, (int)n, &opcode, filename, mode);
 
        printf("%s from %s:%d  filename=\"%s\"  mode=\"%s\"\n",
               opcode == TFTP_RRQ ? "RRQ" : "WRQ", ip, ntohs(cliaddr.sin_port), filename, mode);
        //fork a child and handle the transfer request.
        pid_t pid = fork();
        //fork failed for whatever reason, parent go back and wait for the next transfer request.
        if (pid < 0) {
            perror("fork");
            continue;
        }
        // child forked. Parent go back and wait for the next request
        if (pid > 0)                 
            continue;

        // child process starts here.

        close(listenfd);  // good habit to close this :)   
        // open send sokect             
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("socket");
            exit(1);
        }
        struct sockaddr_in myaddr;
        memset(&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family      = AF_INET;
        myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        // use whatever available port
        myaddr.sin_port        = htons(0);            
        if (bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
            perror("bind");
            exit(1);
        }
        socklen_t mylen = sizeof(myaddr);
        getsockname(sock, (struct sockaddr *)&myaddr, &mylen);   // which port did we get?
        printf("[child %d] serving %s:%d on port %d\n",
               getpid(), ip, ntohs(cliaddr.sin_port), ntohs(myaddr.sin_port));

        // 
        // Temporary: tell the client we are not done yet, so it stops waiting.
        send_error(sock, &cliaddr, 0, "Not implemented yet");

        close(sock);
        exit(0);
    }
    
}