#include "tcp_receiver.hh"
#include "debug.hh"
#include <iostream>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.writer().set_error(); // Signal error on the stream
    return;
  }

  // If this is the first segment with SYN flag, set the ISN
  if ( message.SYN && !isn_set_ ) {
    isn_ = message.seqno;
    isn_set_ = true;
  }

  // If we haven't received a SYN yet, ignore the segment
  if ( !isn_set_ ) {
    return;
  }

  // Calculate the absolute sequence number for the start of the segment
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t abs_seqno = message.seqno.unwrap( isn_, checkpoint );

  // Calculate the stream index for the first byte of the payload
  uint64_t stream_index = abs_seqno - 1 + message.SYN;

  // Insert the payload into the reassembler
  reassembler_.insert( stream_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;

  // Only set ackno if we've received the SYN
  if ( isn_set_ ) {
    // The ackno is the sequence number of the next byte we need
    // It's the first unassembled index + 1 (for SYN) wrapped as a sequence number
    uint64_t abs_ackno = reassembler_.writer().bytes_pushed() + 1;

    // Add 1 more if we've received FIN and it's been reassembled
    if ( reassembler_.writer().is_closed() ) {
      abs_ackno += 1;
    }

    // Convert the absolute sequence number back to a wrapped sequence number
    msg.ackno = Wrap32::wrap( abs_ackno, isn_ );
  }

  // Set the window size to the available capacity, bounded by UINT16_MAX
  uint64_t window = reassembler_.writer().available_capacity();
  msg.window_size = window > UINT16_MAX ? UINT16_MAX : window;
  msg.RST = reassembler_.writer().has_error();
  return msg;
}
