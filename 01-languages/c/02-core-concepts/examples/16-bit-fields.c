#include <stdio.h>
#include <stdint.h>

/* Bit-field structure modeling hardware network packet header */
typedef struct PacketHeader {
    unsigned int version     : 4;  /* 4 bits (0-15) */
    unsigned int header_len  : 4;  /* 4 bits (0-15) */
    unsigned int dscp        : 6;  /* 6 bits (0-63) */
    unsigned int ecn         : 2;  /* 2 bits (0-3)  */
    unsigned int total_len   : 16; /* 16 bits */
} PacketHeader;

int main(void) {
    printf("=== C Bit-Fields Demo ===\n");

    PacketHeader hdr = {0};
    hdr.version = 4;
    hdr.header_len = 5;
    hdr.dscp = 46; /* Expedited Forwarding */
    hdr.ecn = 1;
    hdr.total_len = 1500;

    printf("PacketHeader sizeof: %zu bytes\n", sizeof(PacketHeader));
    printf("Header fields: Version=%u, IHL=%u, DSCP=%u, ECN=%u, Length=%u\n",
           hdr.version, hdr.header_len, hdr.dscp, hdr.ecn, hdr.total_len);

    /* Demonstrate dynamic runtime truncation masking in bitfields */
    unsigned int raw_input = 17; /* 17 = 0b10001; mask to 4 bits */
    hdr.version = raw_input & 0x0F;
    printf("4-bit version masked from %u: %u\n", raw_input, hdr.version);

    return 0;
}
