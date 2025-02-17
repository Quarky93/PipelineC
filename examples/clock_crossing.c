/*
Two async clocks, slow and fast
where fast is R times faster than slow (slow=100Mhz,fast=250MHz, R=2.5)
Stream data elements bidirectionally across this domain crossing
(fast to slow and slow to fast) without bandwidth loss (no flow control on write side required)
This requires a wider bus on the slow side
For fast to slow:
round R up to next integer, ex. N=3
Over N fast cycles buffer up N elements to write once into an async fifo
On the read side in the slow domain units of N data elements are read at a time
```
      fast domain              d0                  d0
d0,d1,d2 -> deserialized to -> d1 -> async fifo -> d1  slow domain
                               d2                  d2
```
For slow to fast:
round R down to the next integer, ex. N=2
Write N elements at once into an async fifo in the slow domain
On the read side in the fast domain units of N data elements are read at a time and serialized
```
            d0                  d0       fast domain
slow domain d1 -> async fifo -> d1  -> serialized to -> d0,d1
```
*/

#include "compiler.h"
#include "wire.h"
#include "arty/src/leds/led0_3.c"

#pragma MAIN_MHZ fast 100.0
#pragma MAIN_MHZ slow 25.0

// Stream of uint64_t values
#include "uintN_t.h"
typedef struct uint64_s
{
  uint64_t data;
  uint1_t valid;  
} uint64_s;
#include "uint64_s_array_N_t.h" // Auto generated

volatile uint64_s fast_to_slow;
#include "fast_to_slow_clock_crossing.h" // Auto generated

volatile uint64_s slow_to_fast;
#include "slow_to_fast_clock_crossing.h" // Auto generated

void fast(uint1_t reset)
{  
  // Drive leds with state, default lit
  static uint1_t test_failed = 0;
  uint1_t led = 1;
  if(test_failed)
  {
    led = 0;
  }
  WIRE_WRITE(uint1_t, led0, led)
  WIRE_WRITE(uint1_t, led1, !reset)
  
  // Send a test pattern into slow
  static uint64_t test_data = 0;
  // Incrementing words
  uint64_s to_slow;
  to_slow.data = test_data;
  to_slow.valid = 1;
  test_data += 1;
  // Reset statics and outputs?
  if(reset)
  {
    test_data = 0;
    to_slow.valid = 0;
  }
  // Send data into slow domain
  fast_to_slow_write_t to_slow_array;
  to_slow_array.data[0] = to_slow;
  fast_to_slow_WRITE(to_slow_array);
  
  // Receive test pattern from slow
  static uint64_t expected = 0;
  // Get data from slow domain
  slow_to_fast_read_t from_slow_array = slow_to_fast_READ();
  uint64_s from_slow = from_slow_array.data[0];
  if(from_slow.valid)
  {
    if(from_slow.data != expected)
    {
      // Failed test
      test_failed = 1;
    }
    else
    {
      // Continue checking test pattern
      expected += 1;
    }
  }
  // Reset statics
  if(reset)
  {
    test_failed = 0;
    expected = 0;
  }
}

void slow(uint1_t reset)
{
  // Drive leds with state, default lit
  static uint1_t test_failed = 0;
  uint1_t led = 1;
  if(test_failed)
  {
    led = 0;
  }
  WIRE_WRITE(uint1_t, led2, led)
  WIRE_WRITE(uint1_t, led3, !reset)
  
  // Send test pattern into fast
  static uint64_t test_data = 0;
  slow_to_fast_write_t to_fast_array;
  uint32_t i;
  for(i=0;i<slow_to_fast_RATIO;i+=1)
  {
    to_fast_array.data[i].data = test_data + i;
    to_fast_array.data[i].valid = 1;
  }
  test_data += slow_to_fast_RATIO;
  // Reset statics and outputs?
  if(reset)
  {
    test_data = 0;
    for(i=0;i<slow_to_fast_RATIO;i+=1)
    {
      to_fast_array.data[i].valid = 0;
    }
  }
  // Send data into fast domain
  slow_to_fast_WRITE(to_fast_array);
  
  // Receive test pattern from fast
  static uint64_t expected = 0;
  // Get datas from fast domain
  fast_to_slow_read_t from_fast_array;
  from_fast_array = fast_to_slow_READ();
  for(i=0;i<fast_to_slow_RATIO;i+=1)
  {
    if(from_fast_array.data[i].valid)
    {
      if(from_fast_array.data[i].data != expected)
      {
        test_failed = 1;
      }
      else
      {
        expected += 1;
      }
    }
  }
  // Reset statics
  if(reset)
  {
    test_failed = 0;
    expected = 0;
  }
}
