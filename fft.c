/*
 * @(#)$Id: fft.c,v 2.0 2008/12/17 17:35:46 baccala Exp $
 *
 * Copyright (C) 1996 Tim Witham <twitham@pcocd2.intel.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements internal & external FFT function for oscope.
 * It is mostly pieces cut and pasted from freq's freq.c and
 * setupsub.c for easier synchronization with that program.
 *
 */

#include <math.h>
#include "oscope.h"
#include "fft.h"

/* these are all constants in oscope's context */
#define WINDOW_RIGHT		540
#define WINDOW_LEFT		100
#define fftlen			FFTLEN
#define freq_scalefactor	1
#define freq_base		0
#define SampleRate		44100
#define shift			0

/* for the fft function: x position to bin number map, and data buffer */
int x[WINDOW_RIGHT-WINDOW_LEFT+1];     /* Array of bin #'s displayed */
int x2[WINDOW_RIGHT-WINDOW_LEFT+1];    /* Array of terminal bin #'s */
int *pX,*pX2;
short fftdata[FFTLEN];
short displayval[FFTLEN/2];
int *bri;
short *pDisplayval;
short *pOut;

/* Fast Fourier Transform of in to out */
void
fft(short *in, short *out)
{
  int i;
  long a2;		/* Variables for computing Sqrt/Log of Amplitude^2 */
  short *p1,*p2;	/* Various indexing pointers */
  long y=0;

  p1=fftdata;
  p2=in;
  for(i=0;i<fftlen;i++)
    {
      *p1++=*p2++;
    }
  RealFFT(fftdata);

  /* Use sqrt(a2) in averaging mode and linear-amplitude mode */
  /* Use pointers for indexing to speed things up a bit. */
  bri=BitReversed;
  pDisplayval=displayval;
  for(i=0;i<fftlen/2;i++)
    {
      /* Compute the magnitude */
      register long re=fftdata[*bri];
      register long im=fftdata[(*bri)+1];
      if((a2=re*re+im*im)<0) a2=0; /* Watch for possible overflow */
      if (a2 >= (1<<22))		/* avoid overflowing the short */
	*pDisplayval  = (1<<15)-1;	/* max short = 2^15 = sqrt(2^22)*16 */
      else
	*pDisplayval = sqrt(a2)*16;
      bri++;
      pDisplayval++;
    }

  pX=x;
  pX2=x2;
  pOut=out;

  {
    int index,xval,lasty=0;
    for(i=WINDOW_LEFT;i<WINDOW_RIGHT+1;i++)
      {
	/*
	 *  If this line is the same as the previous one,
	 *  just use the previous y value.
	 *  Else go ahead and compute the value.
	 */
	index=*pX;
	if(index!=-1)
	  {
	    register long dv=displayval[index];
	    if(*pX2)  /* Take the maximum of a set of bins */
	      {
		for(xval=index;xval<*pX2;xval++)
		  {
		    if(displayval[xval]>dv)
		      {
			dv=displayval[xval];
			index=xval;
		      }
		  }
	      }
	    y=dv;
	  }
	*pOut=y;
	lasty=y;
	pDisplayval++;
	pX++;
	pX2++;
	pOut++;
      }
  }
}

/* initialize global buffers for FFT */
void
init_fft()
{
  int i;

  /*
   *  Initialize graph x scale (linear or logarithmic).
   *  This array points to the bin to be plotted on a given line.
   */
  for(i=0;i<=(WINDOW_RIGHT-WINDOW_LEFT+1);i++)
    {
      int val;
      val=floor(((i-0.45)/(double)(WINDOW_RIGHT-WINDOW_LEFT)
		 *(double)fftlen/2.0/freq_scalefactor)
		+(freq_base/(double)SampleRate*(double)fftlen)+0.5);
	 
      if(val<0) val=0;
      if(val>=fftlen/2) val=fftlen/2-1;
	 
      if(i>0)
	x2[i-1]=val;
      if(i<=(WINDOW_RIGHT-WINDOW_LEFT))
	x[i]=val;
    }
  x[0]=x[1];			/* fix zero bin */

  /* Compute the ending locations for lines holding multiple bins */
  for(i=0;i<=(WINDOW_RIGHT-WINDOW_LEFT);i++)
    {
      if(x2[i]<=(x[i]+1))
	x2[i]=0;
    }

  /*
   *  If lines are repeated on the screen, flag this so that we don't
   *  have to recompute the y values.
   */
  for(i=(WINDOW_RIGHT-WINDOW_LEFT);i>0;i--)
    {
      if(x[i]==x[i-1])
	{
	  x[i]=-1;
	  x2[i]=0;
	}
    }

  InitializeFFT(fftlen);

}
