#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.

  return Wrap32{static_cast<uint32_t>(zero_point.raw_value_ + n)};
  debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  return Wrap32 { 0 };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  //how to unwrap a code
  const uint64_t  CYCLE = 1ULL << 32;
  // which means the cycle is 2*32
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t base = (checkpoint / CYCLE) * CYCLE + offset;
  // then we check the base + cycle base - cycle and base 
  if(base <= checkpoint){
    if(checkpoint - base < base + CYCLE - checkpoint){
      return base;
    }else{
      return base + CYCLE;
    }
  }else{
    if(base - checkpoint < checkpoint - base + CYCLE || (base < CYCLE)){
      return base;
    }else{
      return base - CYCLE;
    }
  }
  
  debug( "unimplemented unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );
  return {};
}
