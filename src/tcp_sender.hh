#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <deque>
#include <functional>
class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) )
    , isn_( isn )
    , next_seqno_( isn )
    , last_ackno_( isn )
    , initial_RTO_ms_( initial_RTO_ms )
    , current_RTO_( initial_RTO_ms )
    , outstanding_segments_()
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }
  void start_timer();

  ByteStream input_;
  Wrap32 isn_;
  Wrap32 next_seqno_;
  Wrap32 last_ackno_;
  uint64_t initial_RTO_ms_;
  uint64_t current_RTO_;
  bool sender_syn { false };
  bool fin_sent_ { false };
  uint16_t window_size { 1 };
  // Timer state
  bool timer_running_ { false };
  uint64_t timer_elapsed_ { 0 };
  // Retransmission tracking
  uint64_t consecutive_retrans_count_ { 0 };
  std::deque<TCPSenderMessage> outstanding_segments_;
};
