#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  uint32_t n32 = n % ( 1UL << 32 );
  return zero_point + n32;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t pow232 = ( 1UL << 32 );
  uint64_t seq_num = ( this->raw_value_ - zero_point.raw_value_ ) % pow232;
  uint64_t order = checkpoint / pow232;

  uint64_t result1 = seq_num + order * pow232;
  uint64_t result2 = result1 + pow232;
  uint64_t result0 = ( result1 >= pow232 ) ? ( result1 - pow232 ) : ( result1 + pow232 );

  uint64_t diff1 = ( result1 > checkpoint ) ? ( result1 - checkpoint ) : ( checkpoint - result1 );
  uint64_t diff2 = ( result2 > checkpoint ) ? ( result2 - checkpoint ) : ( checkpoint - result2 );
  uint64_t diff0 = ( result0 > checkpoint ) ? ( result0 - checkpoint ) : ( checkpoint - result0 );

  // 返回差异最小的值对应的 result
  if ( diff0 <= diff1 && diff0 <= diff2 ) {
    return result0;
  } else if ( diff1 <= diff2 ) {
    return result1;
  } else {
    return result2;
  }
}
