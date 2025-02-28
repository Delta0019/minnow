#include "tcp_receiver.hh"
#include "debug.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }

  if ( !status && message.SYN ) {
    zero_point_ = message.seqno;
    status = 1;
  }

  if ( status == 1 || status == 2 ) {

    uint64_t first_index = message.seqno.unwrap( zero_point_, ackno_ ) - ( status == 2 );
    uint64_t origin_bytes_pushed = reassembler_.writer().bytes_pushed();
    reassembler_.insert( first_index, message.payload, message.FIN );

    if ( reassembler_.writer().has_error() ) {
      return;
    }

    uint64_t push_len = reassembler_.writer().bytes_pushed() - origin_bytes_pushed;

    ackno_ = ackno_ + push_len + message.SYN;

    status = 2;
    if ( reassembler_.writer().is_closed() ) {
      status = 3;
      ackno_++;
    }
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  std::optional<Wrap32> ackno;
  if ( status > 0 ) {
    ackno = Wrap32::wrap( ackno_, zero_point_ );
  }

  uint16_t windeow_size = min( writer().available_capacity(), (uint64_t)UINT16_MAX );

  TCPReceiverMessage message { ackno, windeow_size, reassembler_.writer().has_error() };
  return message;
}
