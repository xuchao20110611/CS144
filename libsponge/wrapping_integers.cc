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
    uint64_t  isn_64=isn.raw_value();
    uint32_t  wrap_32=0;
    n=n-((n>>32)<<32);
    n=n+isn_64;

    if(n>static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())){
        n=n-static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())-1;
    }
    if(n>static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())){
        throw("wrong algo for wrap");
    }
    wrap_32=n;
    return WrappingInt32{wrap_32};
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
    uint64_t n_64=n.raw_value();
    uint64_t isn_64=isn.raw_value();
    uint64_t remainder_64=0;
    uint64_t candidate1_64=0;
    uint64_t candidate2_64=0;
    uint64_t ans_64;

    if(n_64>=isn_64){
        remainder_64=n_64-isn_64;
    } else {
        remainder_64=n_64+static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())+1;
        remainder_64=remainder_64-isn_64;
    }
    candidate1_64=remainder_64+((checkpoint>>32)<<32);
    if(candidate1_64>checkpoint){
        if(candidate1_64<=static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())){
            ans_64=candidate1_64;
        } else {
            candidate2_64=candidate1_64-static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())-1;
            if(candidate2_64>checkpoint||candidate1_64<checkpoint){
                throw("bad algo unwrap: 1");
            }
            if(candidate1_64-checkpoint>checkpoint-candidate2_64){
                ans_64=candidate2_64;
            } else {
                ans_64=candidate1_64;
            }
        }
    } else {
        candidate2_64=candidate1_64+static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())+1;
        if(candidate2_64<checkpoint||candidate1_64>checkpoint){
            throw("bad algo unwrap: 2");
        }
        if(candidate2_64-checkpoint>checkpoint-candidate1_64){
            ans_64=candidate1_64;
        } else {
            ans_64=candidate2_64;
        }
    }
    return ans_64;
}
