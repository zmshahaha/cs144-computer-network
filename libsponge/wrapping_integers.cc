#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn+static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    const uint64_t cycle_size=1ul<<32;
    
    uint64_t abs_seqno;
    abs_seqno=(n<isn)?(cycle_size-(isn-n)):n-isn;

    uint64_t cycles= (checkpoint-abs_seqno+cycle_size)/cycle_size-1;//avoid negative int
    if(cycles!=UINT64_MAX||abs_seqno>=1ul<<32)abs_seqno+=cycles*cycle_size;//avoid lower flow
    else return abs_seqno;

    if(checkpoint-abs_seqno>abs_seqno+cycle_size-checkpoint) 
        return abs_seqno+cycle_size;
    return abs_seqno;
}
