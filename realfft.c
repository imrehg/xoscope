/*
 *     Program: REALFFT.C
 *      Author: Philip VanBaren
 *        Date: 2 September 1993
 *
 * Description: These routines perform an FFT on real data.
 *              On a 486/33 compiled using Borland C++ 3.1 with full
 *              speed optimization and a small memory model, a 1024 point 
 *              FFT takes about 16ms.
 *              This code is for integer data, but could be converted
 *              to float or double simply by changing the data types
 *              and getting rid of the bit-shifting necessary to prevent
 *              overflow/underflow in fixed-point calculations.
 *
 *  Note: Output is BIT-REVERSED! so you must use the BitReversed to
 *        get legible output, (i.e. Real_i = buffer[ BitReversed[i] ]
 *                                  Imag_i = buffer[ BitReversed[i]+1 ] )
 *        Input is in normal order.
 *
 *  Copyright (C) 1995  Philip VanBaren
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * For Borland C++/TASM you may define the flag ASMFFTCODE in order to use the
 * assembly code for the FFT.  This will skip the compilation of the C version
 * of the routine; you must be sure to add realffta.asm to your makefile
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

int *BitReversed;
short *SinTable;
int Points = 0;

/*
 *  Initialize the Sine table and Twiddle pointers (bit-reversed pointers)
 *  for the FFT routine.
 */
void InitializeFFT(int fftlen)
{
   int i;
   int temp;
   int mask;

   /*
    *  FFT size is only half the number of data points
    *  The full FFT output can be reconstructed from this FFT's output.
    *  (This optimization can be made since the data is real.)
    */
   Points = fftlen;

   if((SinTable=(short *)malloc(Points*sizeof(short)))==NULL)
   {
      puts("Error allocating memory for Sine table.");
      exit(1);
	}
   if((BitReversed=(int *)malloc(Points/2*sizeof(int)))==NULL)
   {
      puts("Error allocating memory for BitReversed.");
      exit(1);
   }

   for(i=0;i<Points/2;i++)
   {
      temp=0;
      for(mask=Points/4;mask>0;mask >>= 1)
         temp=(temp >> 1) + (i&mask ? Points/2 : 0);

      BitReversed[i]=temp;
   }

   for(i=0;i<Points/2;i++)
	{
      register double s,c;
      s=floor(-32768.0*sin(2*M_PI*i/(Points))+0.5);
      c=floor(-32768.0*cos(2*M_PI*i/(Points))+0.5);
      if(s>32767.5) s=32767;
      if(c>32767.5) c=32767;
      SinTable[BitReversed[i]  ]=(short)s;
      SinTable[BitReversed[i]+1]=(short)c;
   }
}

/*
 *  Free up the memory allotted for Sin table and Twiddle Pointers
 */
void EndFFT()
{
   free(BitReversed);
	free(SinTable);
	Points=0;
}

/* Include this only if you don't use the REALFFTA.ASM routine */
/* This is for DOS only */

#ifndef ASMFFTCODE

short *A,*B;
short *sptr;
short *endptr1,*endptr2;
int *br1,*br2;
long HRplus,HRminus,HIplus,HIminus;

/*
 *  Actual FFT routine.  Must call InitializeFFT(fftlen) first!
 */
void RealFFT(short *buffer)
{
   int ButterfliesPerGroup=Points/4;

   endptr1=buffer+Points;

   /*
    *  Butterfly:
    *     Ain-----Aout
    *         \ /
    *         / \
    *     Bin-----Bout
    */

   while(ButterfliesPerGroup>0)
   {
      A=buffer;
      B=buffer+ButterfliesPerGroup*2;
      sptr=SinTable;

      while(A<endptr1)
      {
         register short sin=*sptr;
         register short cos=*(sptr+1);
         endptr2=B;
         while(A<endptr2)
         {
            long v1=((long)*B*cos + (long)*(B+1)*sin) >> 15;
            long v2=((long)*B*sin - (long)*(B+1)*cos) >> 15;
	    *B=(*A+v1)>>1;
            *(A++)=*(B++)-v1;
	    *B=(*A-v2)>>1;
            *(A++)=*(B++)+v2;
         }
         A=B;
         B+=ButterfliesPerGroup*2;
         sptr+=2;
      }
      ButterfliesPerGroup >>= 1;
   }
   /*
    *  Massage output to get the output for a real input sequence.
    */
   br1=BitReversed+1;
   br2=BitReversed+Points/2-1;

   while(br1<=br2)
   {
      register long temp1,temp2;
      short sin=SinTable[*br1];
      short cos=SinTable[*br1+1];
      A=buffer+*br1;
      B=buffer+*br2;
      HRplus = (HRminus = *A     - *B    ) + (*B << 1);
      HIplus = (HIminus = *(A+1) - *(B+1)) + (*(B+1) << 1);
      temp1  = ((long)sin*HRminus - (long)cos*HIplus) >> 15;
      temp2  = ((long)cos*HRminus + (long)sin*HIplus) >> 15;
      *B     = (*A     = (HRplus  + temp1) >> 1) - temp1;
      *(B+1) = (*(A+1) = (HIminus + temp2) >> 1) - HIminus;

      br1++;
      br2--;
   }
   /*
    *  Handle DC bin separately
    */
   buffer[0]+=buffer[1];
   buffer[1]=0;
}
#endif

