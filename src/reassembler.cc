#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::write_to_output( string& data, Writer& writer )
{
  next_index_ += data.size();
  writer.push( move( data ) );
}

bool Reassembler::cantain_next_index( uint64_t first_index, uint64_t data_len )
{
  if ( data_len == 0 ) {
    return first_index == next_index_;
  }
  return first_index <= next_index_ && first_index + data_len > next_index_;
}

void Reassembler::pop_from_reassemble_buffer()
{
  auto [buffer_first_index, buffer_data_len, buffer_data] = reassember_buffer_.front();

  reassember_buffer_.pop_front();

  write_to_output( buffer_data, output_.writer() );

  if ( had_last_ && reassember_buffer_.empty() ) {
    output_.writer().close();
  }
}

void Reassembler::insert_and_merge( uint64_t first_index, string& data )
{
  auto it = reassember_buffer_.begin();
  while ( it != reassember_buffer_.end() ) {
    uint64_t buffer_first_index = get<0>( *it );
    uint64_t buffer_data_len = get<1>( *it );
    string buffer_data = get<2>( *it );

    if ( first_index + data.size() < buffer_first_index ) {
      reassember_buffer_.insert( it, make_tuple( first_index, data.size(), data ) );
      return;
    }
    // data与buffer_data正好可以合并
    else if ( first_index + data.size() == buffer_first_index ) {
      string new_data = data + buffer_data;
      it = reassember_buffer_.erase( it );
      insert_and_merge( first_index, new_data );
      return;
    }
    // data与buffer_data有重叠且data在buffer_data之前
    else if ( first_index < buffer_first_index ) {
      // data后半部分无突出
      if ( first_index + data.size() < buffer_first_index + buffer_data_len ) {
        string new_data = data.substr( 0, buffer_first_index - first_index );
        new_data = new_data + buffer_data;
        it = reassember_buffer_.erase( it );
        reassember_buffer_.insert( it, make_tuple( first_index, new_data.size(), new_data ) );
        return;
      }
      // data包含buffer_data
      else {
        it = reassember_buffer_.erase( it );
        continue;
      }
    }
    // buffer_data包含data的前半部分
    else if ( first_index <= buffer_first_index + buffer_data_len ) {
      if ( first_index + data.size() > buffer_first_index + buffer_data_len ) {
        string new_data = data.substr( buffer_first_index + buffer_data_len - first_index );
        new_data = buffer_data + new_data;
        it = reassember_buffer_.erase( it );
        insert_and_merge( buffer_first_index, new_data );
      }
      return;
    } else {
      it++;
    }
  }
  reassember_buffer_.push_back( make_tuple( first_index, data.size(), data ) );
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{

  uint64_t data_len = data.size();
  auto end_index = first_index + data_len;
  auto last_index = next_index_ + output_.writer().available_capacity();

  if ( ( data_len > 0 && end_index <= next_index_ ) || ( data_len == 0 && end_index < next_index_ )
       || first_index >= last_index ) {
    return;
  }

  if ( is_last_substring ) {
    had_last_ = true;
  }

  if ( first_index + data_len > last_index ) {
    data = data.substr( 0, last_index - first_index );
    had_last_ = false;
  }

  if ( first_index < next_index_ ) {
    data = data.substr( next_index_ - first_index );
    first_index = next_index_;
  }

  data_len = data.size();

  insert_and_merge( first_index, data );

  if ( cantain_next_index( first_index, data_len ) ) {
    pop_from_reassemble_buffer();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t buffer_size_ = 0;
  for ( auto& [first_index, data_len, data] : reassember_buffer_ ) {
    buffer_size_ += data_len;
  }
  return buffer_size_;
}
