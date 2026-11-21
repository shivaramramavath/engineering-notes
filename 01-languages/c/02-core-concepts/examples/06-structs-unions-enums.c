#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Enum definition with custom values */
typedef enum Status {
    STATUS_OK = 0,
    STATUS_PENDING = 1,
    STATUS_ERROR = -1
} Status;

/* Struct alignment and offset demonstration */
typedef struct Header {
    uint8_t  magic;       /* 1 byte */
    /* 3 bytes padding */
    uint32_t payload_len; /* 4 bytes */
    uint16_t checksum;    /* 2 bytes */
    /* 2 bytes padding at end to align whole struct */
} Header;

/* Discriminated (tagged) union */
typedef enum ValueType {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING
} ValueType;

typedef struct DynamicValue {
    ValueType type;
    union {
        int64_t     as_int;
        double      as_float;
        char        as_str[16];
    } data;
} DynamicValue;

/* Flexible Array Member */
typedef struct Packet {
    size_t length;
    uint8_t payload[]; /* Flexible array member: C99 onwards */
} Packet;

int main(void) {
    printf("=== C Structs, Unions, and Enums Demo ===\n");

    /* 1. Enum usage */
    Status s = STATUS_PENDING;
    printf("Enum Status value: %d\n", (int)s);

    /* 2. Struct layout and offsetof */
    printf("Header sizeof: %zu bytes\n", sizeof(Header));
    printf("Header.magic offset: %zu\n", offsetof(Header, magic));
    printf("Header.payload_len offset: %zu\n", offsetof(Header, payload_len));
    printf("Header.checksum offset: %zu\n", offsetof(Header, checksum));

    /* Designated initializer */
    Header hdr = {
        .magic = 0xAA,
        .payload_len = 1024,
        .checksum = 0xF00D
    };
    printf("Header initialized: magic=0x%X, len=%u, sum=0x%X\n",
           hdr.magic, (unsigned int)hdr.payload_len, hdr.checksum);

    /* 3. Discriminated Union */
    DynamicValue v1;
    v1.type = VAL_INT;
    v1.data.as_int = 9876543210LL;

    DynamicValue v2;
    v2.type = VAL_STRING;
    strncpy(v2.data.as_str, "Hello Union", sizeof(v2.data.as_str) - 1);
    v2.data.as_str[sizeof(v2.data.as_str) - 1] = '\0';

    printf("DynamicValue sizeof: %zu bytes\n", sizeof(DynamicValue));
    if (v1.type == VAL_INT) {
        printf("v1 integer: %lld\n", (long long)v1.data.as_int);
    }
    if (v2.type == VAL_STRING) {
        printf("v2 string: %s\n", v2.data.as_str);
    }

    /* 4. Flexible Array Member sizeof check */
    printf("Packet header base sizeof: %zu bytes\n", sizeof(Packet));

    return 0;
}
