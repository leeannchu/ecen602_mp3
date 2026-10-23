#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include "netinet/in.h"

//define TFTP packets with their opcodes
#define TFTP_RRQ 1
#define TFTP_WRQ 2
#define TFTP_DATA 3
#define TFTP_ACK 4
#define TFTP_ERROR 5
#define MAX_DATA_SIZE 512

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
