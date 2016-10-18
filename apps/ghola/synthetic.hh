#ifndef GHOLA_SYNTHETIC_HH
#define GHOLA_SYNTHETIC_HH

#include <cstdint>

static constexpr uint64_t REQ_DELAY_SLEEP = (1UL << 63);
static constexpr uint64_t REQ_DELAY_MASK = (~(REQ_DELAY_SLEEP));
static constexpr uint32_t REQ_MAX_DELAYS = 16;

/**
 * Synthetic protocol request type.
 */
struct SynReq {
    uint32_t nr;                     /* the number of delays */
    uint32_t nores;                  /* should server reply? */
    uint64_t tag;                    /* a unique indentifier for the request */
    uint64_t delays[REQ_MAX_DELAYS]; /* an array of delays */
} __attribute__((packed));

/**
 * Synthetic protocol response type.
 */
struct SynRsp {
    uint64_t tag;
} __attribute__((packed));

#endif /* GHOLA_SYNTHETIC_HH */
