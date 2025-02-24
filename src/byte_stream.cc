#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( is_closed() )
    return;

  uint64_t writable = available_capacity();
  uint64_t write_size = min( writable, static_cast<uint64_t>( data.size() ) );
  if ( write_size == 0 )
    return;

  buffer_.push_back( data.substr( 0, write_size ) );
  if ( buffer_.size() == 1 )
    buffer_view_ = buffer_.front();
  bytes_pushed_ += write_size;
}

void Writer::close()
{
  close_ = true;
}

bool Writer::is_closed() const
{
  return close_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - (uint64_t)reader().bytes_buffered();
}

uint64_t Writer::bytes_pushed() const
{
  return ByteStream::bytes_pushed_;
}

string_view Reader::peek() const
{
  return buffer_view_;
}

void Reader::pop( uint64_t len )
{
  if ( len > bytes_buffered() ) {
    return;
  }
  bytes_popped_ += len;

  while ( len > 0 ) {
    if ( len >= buffer_view_.size() ) {
      len -= buffer_view_.size();
      buffer_.pop_front();
      if ( buffer_.empty() ) {
        buffer_view_ = string_view();
        break;
      }
      buffer_view_ = buffer_.front();
    } else {
      buffer_view_.remove_prefix( len );
      break;
    }
  }
}

bool Reader::is_finished() const
{
  return writer().is_closed() && ( bytes_buffered() == 0 );
}

uint64_t Reader::bytes_buffered() const
{
  return writer().bytes_pushed() - bytes_popped();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
