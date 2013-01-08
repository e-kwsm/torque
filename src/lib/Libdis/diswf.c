/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
 * Synopsis:
 *  int diswf(int stream, float value)
 *
 * Converts <value> into a Data-is-Strings floating point number and sends
 * it to <stream>.  The converted number consists of two consecutive signed
 * integers.  The first is the coefficient, at most <ndigs> long, with its
 * implied decimal point at the low-order end.  The second is the exponent
 * as a power of 10.
 *
 * Returns DIS_SUCCESS if everything works well.  Returns an error code
 * otherwise.  In case of an error, no characters are sent to <stream>.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "dis.h"
#include "dis_internal.h"
#include "tcp.h"
#undef diswf

int diswf(
    
  struct tcp_chan *chan,
  double           value)
  
  {
  int       c;
  int       expon;
  unsigned  ndigs;
  int       negate;
  int       retval;
  unsigned  pow2;
  char     *cp;
  char     *ocp;
  double    dval;
  char      scratch[DIS_BUFSIZ];

  memset(scratch, 0, sizeof(scratch));
  /* Make zero a special case.  If we don't it will blow exponent  */
  /* calculation.        */

  if (value == 0.0)
    {
    retval = tcp_puts(chan, "+0+0", 4) != 4 ?
             DIS_PROTO : DIS_SUCCESS;
    return ((tcp_wcommit(chan, retval == DIS_SUCCESS) < 0) ?
            DIS_NOCOMMIT : retval);
    }

  /* Extract the sign from the coefficient.    */
  dval = (negate = value < 0.0) ? -value : value;

  /* Detect and complain about the infinite form.    */
  if (dval > FLT_MAX)
    return (DIS_HUGEVAL);

  /* Compute the integer part of the log to the base 10 of dval.  As a */
  /* byproduct, reduce the range of dval to the half-open interval,       */
  /* [1, 10).        */
  if (dis_dmx10 == 0)
    disi10d_();

  expon = 0;

  pow2 = dis_dmx10 + 1;

  if (dval < 1.0)
    {
    do
      {
      if (dval < dis_dn10[--pow2])
        {
        dval *= dis_dp10[pow2];
        expon += 1 << pow2;
        }
      }
    while (pow2);

    dval *= 10.0;

    expon = -expon - 1;
    }
  else
    {
    do
      {
      if (dval >= dis_dp10[--pow2])
        {
        dval *= dis_dn10[pow2];
        expon += 1 << pow2;
        }
      }
    while (pow2);
    }

  /* Round the value to the last digit     */
  dval += 5.0 * disp10d_(-FLT_DIG);

  if (dval >= 10.0)
    {
    expon++;
    dval *= 0.1;
    }

  /* Starting in the middle of the buffer, convert coefficient digits, */
  /* most significant first.      */
  ocp = cp = &scratch[sizeof(scratch) - 1 - FLT_DIG];

  ndigs = FLT_DIG;

  do
    {
    c = dval;
    dval = (dval - c) * 10.0;
    *ocp++ = c + '0';
    }
  while (--ndigs);

  /* Eliminate trailing zeros.      */
  while (*--ocp == '0');

  /* The decimal point is at the low order end of the coefficient  */
  /* integer, so adjust the exponent for the number of digits in the */
  /* coefficient.        */
  ndigs = ++ocp - cp;

  expon -= ndigs - 1;

  /* Put the coefficient sign into the buffer, left of the coefficient. */
  *--cp = negate ? '-' : '+';

  /* Insert the necessary number of counts on the left.   */
  while (ndigs > 1)
    cp = discui_(cp, ndigs, &ndigs);

  /* The complete coefficient integer is done.  Put it out.  */
  retval = tcp_puts(chan, cp, (size_t)(ocp - cp)) < 0 ?
           DIS_PROTO : DIS_SUCCESS;

  /* If that worked, follow with the exponent, commit, and return. */
  if (retval == DIS_SUCCESS)
    return (diswsi(chan, expon));

  /* If coefficient didn't work, negative commit and return the error. */
  return ((tcp_wcommit(chan, FALSE) < 0)  ? DIS_NOCOMMIT : retval);
  }
