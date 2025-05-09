#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;
// absolute seqno (n) -> seqno
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  return {zero_point + static_cast<uint32_t>(n)};

}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
  // Define the cycle size (2^32)
  const uint64_t CYCLE = 1ULL << 32;
  
  // Get the offset from ISN (in 32-bit arithmetic)
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  
  // Find the "candidate" absolute sequence number in the checkpoint's cycle
  uint64_t cycle_base = checkpoint - (checkpoint % CYCLE);
  uint64_t candidate = cycle_base + offset;
  
  // Calculate the two other candidates (one cycle up and one cycle down)
  uint64_t candidate_prev = (candidate >= CYCLE) ? candidate - CYCLE : UINT64_MAX - (CYCLE - 1 - offset);
  uint64_t candidate_next = candidate + CYCLE;
  
  // Find the candidate closest to the checkpoint
  uint64_t dist_curr = (candidate > checkpoint) ? (candidate - checkpoint) : (checkpoint - candidate);
  uint64_t dist_prev = (candidate_prev > checkpoint) ? (candidate_prev - checkpoint) : (checkpoint - candidate_prev);
  uint64_t dist_next = (candidate_next > checkpoint) ? (candidate_next - checkpoint) : (checkpoint - candidate_next);
  
  if (dist_curr <= dist_prev && dist_curr <= dist_next) {
    return candidate;
  } else if (dist_prev <= dist_next) {
    return candidate_prev;
  } else {
    return candidate_next;
  }
}