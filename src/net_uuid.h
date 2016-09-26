
#include <stdint.h>

typedef uint64_t uuid_time_t;

typedef struct {
    char nodeID[6];
} uuid_node_t;

typedef struct {
    uint32_t  time_low;
    uint16_t  time_mid;
    uint16_t  time_hi_and_version;
    uint8_t   clock_seq_hi_and_reserved;
    uint8_t   clock_seq_low;
    uint8_t   node[6];
} netperf_uuid_t;

/**
 * Generates UUID RFC 4122
 * Example:
 *    char compileUUID[38];
 *    get_uuid_string(compileUUID,sizeof(compileUUID));
 *
 */
void get_uuid_string(char *uuid_str, size_t size);
