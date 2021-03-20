/* Checks that recent_cpu is calculated properly for the case of
   a single ready process.

   The expected output is this (some margin of error is allowed):

   After 2 seconds, recent_cpu is 6.40, load_avg is 0.03.
   After 4 seconds, recent_cpu is 12.60, load_avg is 0.07.
   After 6 seconds, recent_cpu is 18.61, load_avg is 0.10.
   After 8 seconds, recent_cpu is 24.44, load_avg is 0.13.
   After 10 seconds, recent_cpu is 30.08, load_avg is 0.15.
   After 12 seconds, recent_cpu is 35.54, load_avg is 0.18.
   After 14 seconds, recent_cpu is 40.83, load_avg is 0.21.
   After 16 seconds, recent_cpu is 45.96, load_avg is 0.24.
   After 18 seconds, recent_cpu is 50.92, load_avg is 0.26.
   After 20 seconds, recent_cpu is 55.73, load_avg is 0.29.
   After 22 seconds, recent_cpu is 60.39, load_avg is 0.31.
   After 24 seconds, recent_cpu is 64.90, load_avg is 0.33.
   After 26 seconds, recent_cpu is 69.27, load_avg is 0.35.
   After 28 seconds, recent_cpu is 73.50, load_avg is 0.38.
   After 30 seconds, recent_cpu is 77.60, load_avg is 0.40.
   After 32 seconds, recent_cpu is 81.56, load_avg is 0.42.
   After 34 seconds, recent_cpu is 85.40, load_avg is 0.44.
   After 36 seconds, recent_cpu is 89.12, load_avg is 0.45.
   After 38 seconds, recent_cpu is 92.72, load_avg is 0.47.
   After 40 seconds, recent_cpu is 96.20, load_avg is 0.49.
   After 42 seconds, recent_cpu is 99.57, load_avg is 0.51.
   After 44 seconds, recent_cpu is 102.84, load_avg is 0.52.
   After 46 seconds, recent_cpu is 106.00, load_avg is 0.54.
   After 48 seconds, recent_cpu is 109.06, load_avg is 0.55.
   After 50 seconds, recent_cpu is 112.02, load_avg is 0.57.
   After 52 seconds, recent_cpu is 114.89, load_avg is 0.58.
   After 54 seconds, recent_cpu is 117.66, load_avg is 0.60.
   After 56 seconds, recent_cpu is 120.34, load_avg is 0.61.
   After 58 seconds, recent_cpu is 122.94, load_avg is 0.62.
   After 60 seconds, recent_cpu is 125.46, load_avg is 0.64.
   After 62 seconds, recent_cpu is 127.89, load_avg is 0.65.
   After 64 seconds, recent_cpu is 130.25, load_avg is 0.66.
   After 66 seconds, recent_cpu is 132.53, load_avg is 0.67.
   After 68 seconds, recent_cpu is 134.73, load_avg is 0.68.
   After 70 seconds, recent_cpu is 136.86, load_avg is 0.69.
   After 72 seconds, recent_cpu is 138.93, load_avg is 0.70.
   After 74 seconds, recent_cpu is 140.93, load_avg is 0.71.
   After 76 seconds, recent_cpu is 142.86, load_avg is 0.72.
   After 78 seconds, recent_cpu is 144.73, load_avg is 0.73.
   After 80 seconds, recent_cpu is 146.54, load_avg is 0.74.
   After 82 seconds, recent_cpu is 148.29, load_avg is 0.75.
   After 84 seconds, recent_cpu is 149.99, load_avg is 0.76.
   After 86 seconds, recent_cpu is 151.63, load_avg is 0.76.
   After 88 seconds, recent_cpu is 153.21, load_avg is 0.77.
   After 90 seconds, recent_cpu is 154.75, load_avg is 0.78.
   After 92 seconds, recent_cpu is 156.23, load_avg is 0.79.
   After 94 seconds, recent_cpu is 157.67, load_avg is 0.79.
   After 96 seconds, recent_cpu is 159.06, load_avg is 0.80.
   After 98 seconds, recent_cpu is 160.40, load_avg is 0.81.
   After 100 seconds, recent_cpu is 161.70, load_avg is 0.81.
   After 102 seconds, recent_cpu is 162.96, load_avg is 0.82.
   After 104 seconds, recent_cpu is 164.18, load_avg is 0.83.
   After 106 seconds, recent_cpu is 165.35, load_avg is 0.83.
   After 108 seconds, recent_cpu is 166.49, load_avg is 0.84.
   After 110 seconds, recent_cpu is 167.59, load_avg is 0.84.
   After 112 seconds, recent_cpu is 168.66, load_avg is 0.85.
   After 114 seconds, recent_cpu is 169.69, load_avg is 0.85.
   After 116 seconds, recent_cpu is 170.69, load_avg is 0.86.
   After 118 seconds, recent_cpu is 171.65, load_avg is 0.86.
   After 120 seconds, recent_cpu is 172.58, load_avg is 0.87.
   After 122 seconds, recent_cpu is 173.49, load_avg is 0.87.
   After 124 seconds, recent_cpu is 174.36, load_avg is 0.88.
   After 126 seconds, recent_cpu is 175.20, load_avg is 0.88.
   After 128 seconds, recent_cpu is 176.02, load_avg is 0.88.
   After 130 seconds, recent_cpu is 176.81, load_avg is 0.89.
   After 132 seconds, recent_cpu is 177.57, load_avg is 0.89.
   After 134 seconds, recent_cpu is 178.31, load_avg is 0.89.
   After 136 seconds, recent_cpu is 179.02, load_avg is 0.90.
   After 138 seconds, recent_cpu is 179.72, load_avg is 0.90.
   After 140 seconds, recent_cpu is 180.38, load_avg is 0.90.
   After 142 seconds, recent_cpu is 181.03, load_avg is 0.91.
   After 144 seconds, recent_cpu is 181.65, load_avg is 0.91.
   After 146 seconds, recent_cpu is 182.26, load_avg is 0.91.
   After 148 seconds, recent_cpu is 182.84, load_avg is 0.92.
   After 150 seconds, recent_cpu is 183.41, load_avg is 0.92.
   After 152 seconds, recent_cpu is 183.96, load_avg is 0.92.
   After 154 seconds, recent_cpu is 184.49, load_avg is 0.92.
   After 156 seconds, recent_cpu is 185.00, load_avg is 0.93.
   After 158 seconds, recent_cpu is 185.49, load_avg is 0.93.
   After 160 seconds, recent_cpu is 185.97, load_avg is 0.93.
   After 162 seconds, recent_cpu is 186.43, load_avg is 0.93.
   After 164 seconds, recent_cpu is 186.88, load_avg is 0.94.
   After 166 seconds, recent_cpu is 187.31, load_avg is 0.94.
   After 168 seconds, recent_cpu is 187.73, load_avg is 0.94.
   After 170 seconds, recent_cpu is 188.14, load_avg is 0.94.
   After 172 seconds, recent_cpu is 188.53, load_avg is 0.94.
   After 174 seconds, recent_cpu is 188.91, load_avg is 0.95.
   After 176 seconds, recent_cpu is 189.27, load_avg is 0.95.
   After 178 seconds, recent_cpu is 189.63, load_avg is 0.95.
   After 180 seconds, recent_cpu is 189.97, load_avg is 0.95.
*/   


/*
 2     4.92  =  2.95     
     4     6.77  =  4.84     
     6     8.56  =  6.66     
     8    11.14  =  8.42     
    10    12.79  =  10.13    
    12    15.16  =  11.78    
    14    16.67  =  13.37    
    16    18.14  =  14.91    
    18    20.25 >>> 16.40    Too big, by 0.35.
    20    21.59 >>> 17.84    Too big, by 0.25.
    22    23.53 >>> 19.24    Too big, by 0.79.
    24    24.77 >>> 20.58    Too big, by 0.69.
    26    25.97 >>> 21.89    Too big, by 0.58.
    28    27.69 >>> 23.15    Too big, by 1.04.
    30    28.79 >>> 24.37    Too big, by 0.92.
    32    30.38 >>> 25.54    Too big, by 1.34.
    34    31.39 >>> 26.68    Too big, by 1.21.
    36    32.37 >>> 27.78    Too big, by 1.09.
    38    33.77 >>> 28.85    Too big, by 1.42.
    40    34.67 >>> 29.88    Too big, by 1.29.
    42    35.97 >>> 30.87    Too big, by 1.60.
    44    36.80 >>> 31.84    Too big, by 1.46.
    46    37.60 >>> 32.77    Too big, by 1.33.
    48    38.75 >>> 33.67    Too big, by 1.58.
    50    38.75 >>> 34.54    Too big, by 0.71.
    52    38.75  =  35.38    
    54    38.75  =  36.19    
    56    38.75  =  36.98    
    58    38.75  =  37.74    
    60    38.75  =  37.48    
    62    37.47  =  36.24    
    64    36.23  =  35.04    
    66    35.03  =  33.88    
    68    33.87  =  32.76    
    70    32.75  =  31.68    
    72    31.67  =  30.63    
    74    30.62  =  29.62    
    76    29.61  =  28.64    
    78    28.63  =  27.69    
    80    27.68  =  26.78    
    82    26.77  =  25.89    
    84    25.88  =  25.04    
    86    25.03  =  24.21    
    88    24.20  =  23.41    
    90    23.40  =  22.64    
    92    22.63  =  21.89    
    94    21.88  =  21.16    
    96    21.16  =  20.46    
    98    20.46  =  19.79    
   100    19.78  =  19.13    
   102    19.13  =  18.50    
   104    18.49  =  17.89    
   106    17.88  =  17.30    
   108    17.29  =  16.73    
   110    16.72  =  16.17    
   112    16.17  =  15.64    
   114    15.63  =  15.12    
   116    15.12  =  14.62    
   118    14.62  =  14.14    
   120    14.13  =  13.67    
   122    13.67  =  13.22    
   124    13.21  =  12.78    
   126    12.78  =  12.36    
   128    12.35  =  11.95    
   130    11.95  =  11.56    
   132    11.55  =  11.17    
   134    11.17  =  10.80    
   136    10.80  =  10.45    
   138    10.44  =  10.10    
   140    10.10  =  9.77     
   142     9.76  =  9.45     
   144     9.44  =  9.13     
   146     9.13  =  8.83     
   148     8.83  =  8.54     
   150     8.54  =  8.26     
   152     8.25  =  7.98     
   154     7.98  =  7.72     
   156     7.72  =  7.47     
   158     7.46  =  7.22     
   160     7.21  =  6.98     
   162     6.98  =  6.75     
   164     6.75  =  6.53     
   166     6.52  =  6.31     
   168     6.31  =  6.10     
   170     6.10  =  5.90     
   172     5.90  =  5.70     
   174     5.70  =  5.52     
   176     5.51  =  5.33     
   178     5.33  =  5.16     
*/

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

/* Sensitive to assumption that recent_cpu updates happen exactly
   when timer_ticks() % TIMER_FREQ == 0. */

void
test_mlfqs_recent_1 (void) 
{
  int64_t start_time;
  int last_elapsed = 0;
  
  ASSERT (thread_mlfqs);

  do 
    {
      msg ("Sleeping 10 seconds to allow recent_cpu to decay, please wait...");
      start_time = timer_ticks ();
      timer_sleep (DIV_ROUND_UP (start_time, TIMER_FREQ) - start_time
                   + 10 * TIMER_FREQ);
    }
  while (thread_get_recent_cpu () > 700);

  start_time = timer_ticks ();
  for (;;) 
    {
      int elapsed = timer_elapsed (start_time);
      if (elapsed % (TIMER_FREQ * 2) == 0 && elapsed > last_elapsed) 
        {
          int recent_cpu = thread_get_recent_cpu ();
          int load_avg = thread_get_load_avg ();
          int elapsed_seconds = elapsed / TIMER_FREQ;
          msg ("After %d seconds, recent_cpu is %d.%02d, load_avg is %d.%02d.",
               elapsed_seconds,
               recent_cpu / 100, recent_cpu % 100,
               load_avg / 100, load_avg % 100);
          if (elapsed_seconds >= 180)
            break;
        } 
      last_elapsed = elapsed;
    }
}
