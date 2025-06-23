#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <algorithm>
#include <iostream>
using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  if ( !sender_syn ) {
    return 0;
  }

  uint64_t bytes_in_flight = 0;
  for ( const auto& seg : outstanding_segments_ ) {
    bytes_in_flight += seg.sequence_length();
  }
  return bytes_in_flight;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans_count_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Handle error case
  if ( reader().has_error() ) {
    TCPSenderMessage msg;
    msg.RST = true;
    msg.seqno = sender_syn ? next_seqno_ : isn_;
    transmit( msg );
    return;
  }

  // Send SYN if not already sent
  if ( !sender_syn ) {
    TCPSenderMessage msg;
    msg.SYN = true;
    msg.seqno = isn_;
    sender_syn = true;

    // Track as outstanding and start timer
    uint64_t effective_window_size = window_size - msg.sequence_length();
    if ( reader().bytes_buffered() > 0 && effective_window_size > 0 ) {
      size_t read_size = min( { TCPConfig::MAX_PAYLOAD_SIZE,
                                effective_window_size,
                                static_cast<uint64_t>( reader().bytes_buffered() ) } );

      if ( read_size > 0 ) {
        read( reader(), read_size, msg.payload );
      }
    }

    // Check if we should also include FIN with SYN
    if ( reader().is_finished() && !fin_sent_ ) {
      msg.FIN = true;
      fin_sent_ = true;
    }

    transmit( msg );
    outstanding_segments_.push_back( msg );
    if ( !timer_running_ ) {
      start_timer();
    }
    next_seqno_ = isn_ + msg.sequence_length();
    return;
  }

  // Calculate window size and space
  uint64_t bytes_in_flight = sequence_numbers_in_flight();
  uint64_t effective_window_size = window_size > 0 ? window_size : 1;

  // Only proceed if we have window space
  if ( bytes_in_flight >= effective_window_size ) {
    return; // Window is full
  }

  uint64_t remaining_window = effective_window_size - bytes_in_flight;

  // Continue sending data and FIN while we have window space
  while ( remaining_window > 0 ) {
    TCPSenderMessage msg;
    msg.seqno = next_seqno_;
    bool segment_sent = false;

    // Send data if available and window has space
    if ( reader().bytes_buffered() > 0 && remaining_window > 0 ) {
      size_t read_size = min(
        { TCPConfig::MAX_PAYLOAD_SIZE, remaining_window, static_cast<uint64_t>( reader().bytes_buffered() ) } );

      if ( read_size > 0 ) {
        read( reader(), read_size, msg.payload );
        segment_sent = true;
      }
    }

    // Add FIN if stream is finished and we have room and haven't sent FIN yet
    if ( writer().is_closed() && !fin_sent_ && msg.sequence_length() + 1 <= remaining_window
         && reader().bytes_buffered() == 0 ) {
      msg.FIN = true;
      fin_sent_ = true;
      segment_sent = true;
    }

    // Send the segment if it contains data or flags
    if ( segment_sent ) {
      uint64_t seg_length = msg.sequence_length();
      next_seqno_ = next_seqno_ + seg_length;

      // Track as outstanding and start timer if needed
      outstanding_segments_.push_back( msg );
      if ( !timer_running_ ) {
        start_timer();
      }

      transmit( msg );
      remaining_window -= seg_length;

      // Break after sending FIN - nothing more to send
      if ( msg.FIN ) {
        break;
      }
    } else {
      break; // No more data to send
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = sender_syn ? next_seqno_ : isn_;

  // FIXED: Set RST flag if the stream has an error
  if ( reader().has_error() ) {
    msg.RST = true;
  }

  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    writer().set_error();
    return;
  }

  // Update window size
  window_size = msg.window_size;

  // If no ackno is present, nothing else to process
  if ( !msg.ackno.has_value() ) {
    return;
  }

  Wrap32 ackno = msg.ackno.value();
  uint64_t abs_next_seqno = next_seqno_.unwrap( isn_, 0 );
  uint64_t abs_ackno = ackno.unwrap( isn_, abs_next_seqno );

  // Check if this ackno is valid (not acknowledging unsent data)
  if ( abs_ackno > abs_next_seqno ) {
    return; // Invalid ackno, ignore
  }

  // Check if this ackno acknowledges new data
  bool acknowledged_new_data = false;
  uint64_t abs_last_ackno = last_ackno_.unwrap( isn_, abs_next_seqno );

  if ( abs_ackno > abs_last_ackno ) {
    acknowledged_new_data = true;
    last_ackno_ = ackno;
  }

  // Remove fully acknowledged segments
  auto it = outstanding_segments_.begin();
  while ( it != outstanding_segments_.end() ) {
    uint64_t seg_start = it->seqno.unwrap( isn_, abs_next_seqno );
    uint64_t seg_end = seg_start + it->sequence_length();

    if ( seg_end <= abs_ackno ) {
      // Segment is fully acknowledged
      it = outstanding_segments_.erase( it );
    } else {
      ++it;
    }
  }

  // Handle timer and RTO based on new acknowledgments
  if ( acknowledged_new_data ) {
    // Reset RTO to initial value
    current_RTO_ = initial_RTO_ms_;

    // Reset consecutive retransmissions
    consecutive_retrans_count_ = 0;

    // Restart timer if we still have outstanding data
    if ( !outstanding_segments_.empty() ) {
      start_timer();
    } else {
      timer_running_ = false;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer_elapsed_ += ms_since_last_tick;

  // Check if timer has expired'
  if ( timer_running_ && timer_elapsed_ >= current_RTO_ ) {

    // Timer expired - retransmit earliest outstanding segment
    if ( !outstanding_segments_.empty() ) {
      // Find earliest segment (should be first in queue)
      TCPSenderMessage earliest_seg = outstanding_segments_.front();

      // Retransmit it
      transmit( earliest_seg );
      // Handle backoff only if window size is nonzero
      if ( window_size > 0 ) {
        consecutive_retrans_count_++;
        current_RTO_ *= 2; // Exponential backoff
      }

      // Restart timer
      start_timer();
    }
  }
}

void TCPSender::start_timer()
{
  timer_running_ = true;
  timer_elapsed_ = 0;
}