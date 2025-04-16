#include "byte_stream.hh"

inline uint64_t abs_u64( uint64_t a, uint64_t b )
{
  return ( a > b ) ? ( a - b ) : ( b - a );
}

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity )
  , available_capacity_( capacity )
  , buf_start_( 0 )
  , buf_end_( 0 )
  , byte_popped_( 0 )
  , byte_pushed_( 0 )
  , eof_( false )
{
  buf_.resize( capacity_ );
}

bool Writer::is_closed() const
{
  return eof_;
}

void Writer::push( string data )
{
  if ( eof_ )
    return;

  uint64_t len = min( data.size(), available_capacity() );
  if ( !len )
    return;

  for ( uint64_t i = 0; i < len; i++ ) {
    buf_[buf_end_] = data[i];
    buf_end_ = ( buf_end_ + 1 ) % capacity_;
  }

  available_capacity_ -= len;
  byte_pushed_ += len;
}

void Writer::close()
{
  eof_ = true;
}

uint64_t Writer::available_capacity() const
{
  return available_capacity_;
}

uint64_t Writer::bytes_pushed() const
{
  return byte_pushed_;
}

// string closed and fully popped
bool Reader::is_finished() const
{
  return eof_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return byte_popped_;
}

string_view Reader::peek() const
{
  if ( bytes_buffered() == 0 )
    return std::string_view {}; // nothing in buf

  return std::string_view( &buf_[buf_start_], 1 );
}

// reader consume from buffer
void Reader::pop( uint64_t len )
{
  if ( bytes_buffered() == 0 )
    return;

  len = min( bytes_buffered(), len );
  buf_start_ = ( buf_start_ + len ) % capacity_;
  byte_popped_ += len;
  available_capacity_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  return capacity_ - available_capacity_;
}