#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  Wrap32 had_ackno = had_ackno_ == Wrap32( 0 ) ? isn_ : had_ackno_;
  return abs_seqno_ - had_ackno.unwrap( isn_, abs_seqno_ );
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( is_closed_ ) {
    return;
  }

  uint16_t had_push = sequence_numbers_in_flight();
  while ( had_push < receiver_window_size_ ) {

    uint16_t msg_size = uint16_t( receiver_window_size_ - had_push );

    bool RST = reader().has_error();
    bool SYN {};
    if ( abs_seqno_ == 0 ) {
      SYN = true;
      had_ackno_ = isn_;
    }
    bool FIN = ( reader().bytes_buffered() < (uint64_t)( msg_size - SYN ) ) && writer().is_closed();
    uint16_t payload_size = msg_size - SYN - FIN;
    if ( payload_size > TCPConfig::MAX_PAYLOAD_SIZE && reader().bytes_buffered() > TCPConfig::MAX_PAYLOAD_SIZE ) {
      FIN = false;
      payload_size = TCPConfig::MAX_PAYLOAD_SIZE;
    }
    string payload;
    read( reader(), payload_size, payload );

    TCPSenderMessage message { Wrap32::wrap( abs_seqno_, isn_ ), SYN, payload, FIN, RST };
    if ( message.sequence_length() == 0 ) {
      break;
    }
    messages_in_flight_.emplace_back( current_RTO_ms_, message );
    transmit( message );
    had_push += message.sequence_length();
    abs_seqno_ += message.sequence_length();

    if ( message.FIN ) {
      is_closed_ = true;
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  Wrap32 seqno = Wrap32::wrap( abs_seqno_, isn_ );
  bool RST = reader().has_error();
  return TCPSenderMessage { seqno, false, "", false, RST };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    reader().set_error();
    receiver_window_size_ = 1;
    messages_in_flight_.clear();
  }

  uint16_t window_size = msg.window_size;
  if ( msg.ackno.has_value() ) {
    uint64_t ackno = msg.ackno.value().unwrap( isn_, abs_seqno_ );
    uint64_t had_ackno = had_ackno_.unwrap( isn_, abs_seqno_ );
    if ( ( ackno > had_ackno && ackno <= abs_seqno_ ) ) {

      if ( window_size == 0 ) {
        is_fill_ = true;
      }
      receiver_window_size_ = window_size == 0 ? 1 : window_size;
      current_RTO_ms_ = initial_RTO_ms_;
      consecutive_retransmissions_ = 0;
      had_ackno_ = msg.ackno.value();

      while ( !messages_in_flight_.empty() ) {
        auto& [remain_time, message] = messages_in_flight_.front();
        uint64_t seqno = message.seqno.unwrap( isn_, abs_seqno_ );
        if ( seqno + message.sequence_length() <= ackno ) {
          messages_in_flight_.pop_front();
        } else {
          break;
        }
      }
      if ( !messages_in_flight_.empty() ) {
        auto& [remain_time, message] = messages_in_flight_.front();
        remain_time = current_RTO_ms_;
      }
    }
  }

  if ( window_size > receiver_window_size_ ) {
    receiver_window_size_ = window_size;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  uint64_t double_RTO = current_RTO_ms_ * 2;

  if ( !messages_in_flight_.empty() ) {
    auto& [remain_time, message] = messages_in_flight_.front();
    if ( remain_time <= ms_since_last_tick ) {
      transmit( message );
      if ( !is_fill_ ) {
        current_RTO_ms_ = double_RTO;
        consecutive_retransmissions_++;
      }
      remain_time = current_RTO_ms_;
    } else {
      remain_time -= ms_since_last_tick;
    }
  }
}
