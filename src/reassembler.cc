#include "reassembler.hh"
#include "debug.hh"

using namespace std;
/*
1. Bytes that are the next bytes in the stream. The Reassembler should push these to
 the stream (output
 .writer()) as soon as they are known.
 2. Bytes that fit within the stream’s available capacity but can’t yet be written, because
 earlier bytes remain unknown. These should be stored internally in the Reassembler.
 3. Bytes that lie beyond the stream’s available capacity. These should be discarded. The
 Reassembler’s will not store any bytes that can’t be pushed to the ByteStream either
 immediately, or as soon as earlier bytes become known.
*/
void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring)
{
  // Handle end-of-stream marker case
  if (data.empty() && is_last_substring) {
    if (first_index <= next_byte_idx) {
      output_.writer().close();
    } else {
      last = true;
      last_idx_ = first_index - 1;
    }
    return;
  }

  // Mark last substring if needed
  if (is_last_substring) {
    last = true;
    last_idx_ = first_index + data.size() - 1;
  }

  // Store bytes that are within capacity
  uint64_t capacity = output_.writer().available_capacity();
  
  // Skip if completely out of window
  if (first_index >= next_byte_idx + capacity) {
    return;
  }
  
  // Process only bytes we haven't seen yet and that fit within capacity
  for (size_t i = 0; i < data.size(); i++) {
    uint64_t abs_index = first_index + i;
    
    // Skip bytes that are already assembled
    if (abs_index < next_byte_idx) {
      continue;
    }
    
    // Skip bytes beyond our capacity
    if (abs_index >= next_byte_idx + capacity) {
      break;
    }
    
    // Add to buffer if not already there
    if (buf_.find(abs_index) == buf_.end()) {
      buf_[abs_index] = data[i];
      pending_bytes_++;
    }
  }
  
  // Write contiguous bytes to output
  while (buf_.find(next_byte_idx) != buf_.end()) {
    string byte_to_write(buf_[next_byte_idx]);
    output_.writer().push(byte_to_write);
    buf_.erase(next_byte_idx);
    pending_bytes_--;
    next_byte_idx++;
  }
  
  // Close the stream if we've written all bytes up to last_idx_
  if (last && next_byte_idx > last_idx_) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  // debug( "unimplemented count_bytes_pending() called" );
  return pending_bytes_;
}
