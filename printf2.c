/*******************************************************************************
 Copyright 2001, 2002 Georges Menie (<URL snipped>)
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/*******************************************************************************
 putchar is the only external dependency for this file, 
 if you have a working putchar, just remove the following
 define. If the function should be called something else,
 replace outbyte(c) by your own function call.
*/
//*******************************************************************************
//  Updated by Daniel D Miller.  Changes to the original Menie code are
//  Copyright 2009-2013 Daniel D Miller
//  All such changes are distributed under the same license as the original,
//  as described above.
//  11/06/09 - adding floating-point support
//  03/19/10 - pad fractional portion of floating-point number with 0s
//  07/20/10 - Fix a round-off bug in floating-point conversions
//             ( 0.99 with %.1f did not round to 1.0 )
//  10/25/11 - Add support for %+ format (always show + on positive numbers)
//  05/10/13 - Add stringfn() function, which takes a maximum-output-buffer
//             length as an argument.  Similar to snprintf()
//  10/08/13 - Add support for signed/unsigned long long (u64/i64)
//*******************************************************************************
//  compile command for stand-alone mode (TEST_PRINTF)
//  gcc -Wall -O2 -DTEST_PRINTF -s printf2.c -o printf2.exe
//*******************************************************************************

//lint -esym(752, debug_output)
//lint -esym(766, stdio.h)

// #define  TEST_PRINTF    1

#ifdef TEST_PRINTF
#include <stdio.h>
#include <string.h>

#undef  USE_INTERNALS
#else
//  USE_INTERNALS implements code for company-specific linkages
//  at the company that I currently work for.
//  Other users will *not* want this defined!!
#define  USE_INTERNALS
#endif

#ifdef   USE_INTERNALS
#include <stdio.h>
#include <string.h>

#include <remap.h>
#include <91x_lib.h>

#include "arm_lib.h"
// #include "depends.h"

#else

// ASCII character set... 
#define NUL         0x00
#define STX         0x02
#define ETX         0x03
#define EOT         0x04
#define ACK         0x06
#define BS          0x08
#define LF          0x0A
#define CR          0x0D
#define DLE         0x10
#define ESC         0x1B
#define SPC         0x20
#define MAX_PRN     0x7E
#define DEL         0x7F

extern int putchar (int c);
//lint -e534  Ignoring return value of function 
//lint -e539  Did not expect positive indentation from line ...
//lint -e525  Negative indentation from line ...

typedef  unsigned char  u8 ;
typedef  unsigned int   uint ;
#endif

static uint use_leading_plus = 0 ;

static int max_output_len = -1 ;
static int curr_output_len = 0 ;

uint fixed_overrun = 0 ;
int last_bad_char = 0 ;
uint invalid_char = 0 ;

//****************************************************************************
#ifndef TEST_PRINTF
static void printchar (char **str, int c) HIGHSEG
#endif
static void printchar (char **str, int c)
{
   if (max_output_len >= 0  &&  curr_output_len >= max_output_len) {
      fixed_overrun++ ;
      return ;
   }

   //  check for overrun of our stuff_talkf[] buffer
//    if (curr_output_len >= TALK_LINE_LEN) {
//       fixed_overrun++ ;
//       return ;
//    }
   if (c > MAX_PRN) {
      invalid_char++ ;
      last_bad_char = c ;
      c = '?' ;
   } else
   if (c < SPC) {  //  ASCII 32 = Space
      switch (c) {
      case CR:
      case LF:
      case STX:
      case ETX:
         break;

      default:
         invalid_char++ ;
         last_bad_char = c ;
         c = '?' ;
         break;
      }
   }

	if (str) {
      **str = (char) c;
		++(*str);
      curr_output_len++ ;
	}
#ifdef TEST_PRINTF
   else {
      curr_output_len++ ;
      (void) putchar (c);
   }
#endif
}

//****************************************************************************
//  This version returns the length of the output string.
//  It is more useful when implementing a walking-string function.
//****************************************************************************
#ifdef TEST_PRINTF
static const double round_nums[8] = {
#else   
//lint -esym(728, round_nums)
static const double round_nums[8] HIGHRO = {
#endif   
   0.5,
   0.05,
   0.005,
   0.0005,
   0.00005,
   0.000005,
   0.0000005,
   0.00000005
} ;

#ifndef TEST_PRINTF
static unsigned dbl2stri(char *outbfr, double dbl, unsigned dec_digits) HIGHSEG
#endif
static unsigned dbl2stri(char *outbfr, double dbl, unsigned dec_digits)
{
   static char local_bfr[128] ;
   char *output = (outbfr == 0) ? local_bfr : outbfr ;

   //*******************************************
   //  extract negative info
   //*******************************************
   if (dbl < 0.0) {
      *output++ = '-' ;
      dbl *= -1.0 ;
   } else {
      if (use_leading_plus) {
         *output++ = '+' ;
      }
      
   }

   //  handling rounding by adding .5LSB to the floating-point data
   if (dec_digits < 8) {
      dbl += round_nums[dec_digits] ;
   }

   //**************************************************************************
   //  construct fractional multiplier for specified number of digits.
   //**************************************************************************
   uint mult = 1 ;
   uint idx ;
   for (idx=0; idx < dec_digits; idx++)
      mult *= 10 ;

   // printf("mult=%u\n", mult) ;
   uint wholeNum = (uint) dbl ;
   uint decimalNum = (uint) ((dbl - wholeNum) * mult);

   //*******************************************
   //  convert integer portion
   //*******************************************
   char tbfr[40] ;
   idx = 0 ;
   while (wholeNum != 0) {
      tbfr[idx++] = '0' + (wholeNum % 10) ;
      wholeNum /= 10 ;
   }
   // printf("%.3f: whole=%s, dec=%d\n", dbl, tbfr, decimalNum) ;
   if (idx == 0) {
      *output++ = '0' ;
   } else {
      while (idx > 0) {
         *output++ = tbfr[idx-1] ;  //lint !e771
         idx-- ;
      }
   }
   if (dec_digits > 0) {
      *output++ = '.' ;

      //*******************************************
      //  convert fractional portion
      //*******************************************
      idx = 0 ;
      while (decimalNum != 0) {
         tbfr[idx++] = '0' + (decimalNum % 10) ;
         decimalNum /= 10 ;
      }
      //  pad the decimal portion with 0s as necessary;
      //  We wouldn't want to report 3.093 as 3.93, would we??
      while (idx < dec_digits) {
         tbfr[idx++] = '0' ;
      }
      // printf("decimal=%s\n", tbfr) ;
      if (idx == 0) {
         *output++ = '0' ;
      } else {
         while (idx > 0) {
            *output++ = tbfr[idx-1] ;
            idx-- ;
         }
      }
   }
   *output = 0 ;

   //  prepare output
   output = (outbfr == 0) ? local_bfr : outbfr ;
#ifdef TEST_PRINTF
   return strlen(output) ;
#else
   return my_strlen(output) ;
#endif
}

//****************************************************************************
#define  PAD_RIGHT   1
#define  PAD_ZERO    2

#ifndef TEST_PRINTF
static int prints (char **out, const char *string, int width, int pad) HIGHSEG
#endif
static int prints (char **out, const char *string, int width, int pad)
{
   // [831326121984], width=0, pad=0
   // printf("[%s], width=%u, pad=%u\n", string, width, pad) ;
	register int pc = 0, padchar = ' ';
	if (width > 0) {
      int len = 0;
      const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (pad & PAD_ZERO)
			padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for (; width > 0; --width) {
			printchar (out, padchar);
			++pc;
		}
	}
	for (; *string; ++string) {
		printchar (out, *string);
		++pc;
	}
	for (; width > 0; --width) {
		printchar (out, padchar);
		++pc;
	}
	return pc;
}

//****************************************************************************
/* the following should be enough for 32 bit int */
//  -2,147,483,648
#define PRINT_BUF_LEN 12
#ifndef TEST_PRINTF
static int printi (char **out, int i, uint base, int sign, int width, int pad, int letbase) HIGHSEG
#endif
static int printi (char **out, int i, uint base, int sign, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
   char *s;
   int t, neg = 0, pc = 0;
   unsigned u = (unsigned) i;
	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}
   if (sign && base == 10 && i < 0) {
		neg = 1;
      u = (unsigned) -i;
	}
   //  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) {
      t = u % base;  //lint !e573 !e737 !e713 Warning 573: Signed-unsigned mix with divide
		if (t >= 10)
			t += letbase - '0' - 10;
      *--s = (char) t + '0';
      u /= base;  //lint !e573 !e737  Signed-unsigned mix with divide
	}
	if (neg) {
		if (width && (pad & PAD_ZERO)) {
			printchar (out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
   } else {
      if (use_leading_plus) {
         *--s = '+';
      }
   }
	return pc + prints (out, s, width, pad);
}

//****************************************************************************
/* the following should be enough for 64 bit int, without commas */
// _UI64_MAX
// Maximum value for a variable of type unsigned __int64
//                         xx..xx..xx..xx..
// 18446744073709551615 (0xffffffffffffffff)
// -9223372036854775808
// 18,446,744,073,709,551,615
#define PRINT_LLBUF_LEN 22
#ifndef TEST_PRINTF
static int printlli (char **out, long long i, uint base, int sign, int width, int pad, int letbase) HIGHSEG
#endif
static int printlli (char **out, long long i, uint base, int sign, int width, int pad, int letbase)
{
   char print_buf[PRINT_LLBUF_LEN];
   char *s;
   int t, neg = 0, pc = 0;
   unsigned long long u = (unsigned long long) i;
   if (i == 0) {
      print_buf[0] = '0';
      print_buf[1] = '\0';
      return prints (out, print_buf, width, pad);
   }
   if (sign && base == 10 && i < 0) {
      neg = 1;
      u = (unsigned long long) -i;
   }
   //  make sure print_buf is NULL-term
   s = print_buf + PRINT_LLBUF_LEN - 1;
   *s = '\0';

   while (u) {
      t = u % base;  //lint !e573 !e737 !e713 Warning 573: Signed-unsigned mix with divide
      if (t >= 10)
         t += letbase - '0' - 10;
      *--s = (char) t + '0';
      u /= base;  //lint !e573 !e737  Signed-unsigned mix with divide
   }
   if (neg) {
      if (width && (pad & PAD_ZERO)) {
         printchar (out, '-');
         ++pc;
         --width;
      }
      else {
         *--s = '-';
      }
   } else {
      if (use_leading_plus) {
         *--s = '+';
      }
   }
   return pc + prints (out, s, width, pad);
}

//****************************************************************************
#ifndef TEST_PRINTF
static int print (char **out, int *varg) HIGHSEG
#endif
static int print (char **out, int *varg)
{
   int post_decimal ;
   int width, pad ;
   unsigned dec_width = 6 ;
   int pc = 0;
   char *format = (char *) (*varg++);
   char scr[2];
   use_leading_plus = 0 ;  //  start out with this clear
	for (; *format != 0; ++format) {
		if (*format == '%') {
         dec_width = 6 ;
			++format;
			width = pad = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
            goto out_lbl;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
         if (*format == '+') {
            ++format;
            use_leading_plus = 1 ;
         }
         while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
         post_decimal = 0 ;
         if (*format == '.'  ||
            (*format >= '0' &&  *format <= '9')) {

            while (1) {
               if (*format == '.') {
                  post_decimal = 1 ;
                  dec_width = 0 ;
                  format++ ;
               } else if ((*format >= '0' &&  *format <= '9')) {
                  if (post_decimal) {
                     dec_width *= 10;
                     dec_width += (uint) (u8) (*format - '0');
                  } else {
                     width *= 10;
                     width += *format - '0';
                  }
                  format++ ;
               } else {
                  break;
               }
            }
         }
         uint use_longlong = 0 ;
         long long *uvarg ;
         long long llvalue ;
         if (*format == 'l') {
            ++format;
            if (*format == 'l') {
               ++format;
               use_longlong = 1 ;
            }
         }
         switch (*format) {
         case 's':
            {
            // char *s = *((char **) varg++);   //lint !e740
            char *s = (char *) *varg++ ;  //lint !e740 !e826  
            pc += prints (out, s ? s : "(null)", width, pad);
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            }
            break;
         case 'd':
            if (use_longlong) {
               uvarg = (long long *) varg ;
               llvalue = *uvarg++ ;
               varg = (int *) uvarg ;
               pc += printlli(out, llvalue, 10, 1, width, pad, 'a');
            }
            else {
               pc += printi  (out, *varg++, 10, 1, width, pad, 'a');
            }
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;
         case 'x':
            if (use_longlong) {
               uvarg = (long long *) varg ;
               llvalue = *uvarg++ ;
               varg = (int *) uvarg ;
               pc += printlli(out, llvalue, 16, 0, width, pad, 'a');
            }
            else {
               pc += printi  (out, *varg++, 16, 0, width, pad, 'a');
            }
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;
         case 'X':
            if (use_longlong) {
               uvarg = (long long *) varg ;
               llvalue = *uvarg++ ;
               varg = (int *) uvarg ;
               pc += printlli(out, llvalue, 16, 0, width, pad, 'A');
            }
            else {
               pc += printi  (out, *varg++, 16, 0, width, pad, 'A');
            }
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;
         case 'p':
         case 'u':
            if (use_longlong) {
               uvarg = (long long *) varg ;
               llvalue = *uvarg++ ;
               varg = (int *) uvarg ;
               pc += printlli(out, llvalue, 10, 0, width, pad, 'a');
            }
            else {
               pc += printi  (out, *varg++, 10, 0, width, pad, 'a');
            }
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;
         case 'c':
            /* char are converted to int then pushed on the stack */
            scr[0] = (char) *varg++;
            scr[1] = '\0';
            pc += prints (out, scr, width, pad);
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;

         case 'f':
            {
            // http://wiki.debian.org/ArmEabiPort#Structpackingandalignment
            // Stack alignment
            // 
            // The ARM EABI requires 8-byte stack alignment at public function entry points, 
            // compared to the previous 4-byte alignment.
#ifdef USE_NEWLIB               
            char *cptr = (char *) varg ;  //lint !e740 !e826  convert to double pointer
            uint caddr = (uint) cptr ;
            if ((caddr & 0xF) != 0) {
               cptr += 4 ;
            }
            double *dblptr = (double *) cptr ;  //lint !e740 !e826  convert to double pointer
#else
            double *dblptr = (double *) varg ;  //lint !e740 !e826  convert to double pointer
#endif
            double dbl = *dblptr++ ;   //  increment double pointer
            varg = (int *) dblptr ;    //lint !e740  copy updated pointer back to base pointer
            char bfr[81] ;
            // unsigned slen =
            dbl2stri(bfr, dbl, dec_width) ;
            // stuff_talkf("[%s], width=%u, dec_width=%u\n", bfr, width, dec_width) ;
            pc += prints (out, bfr, width, pad);
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            }
            break;

         default:
            printchar (out, '%');
            printchar (out, *format);
            use_leading_plus = 0 ;  //  reset this flag after printing one value
            break;
         }
      } else 
      // if (*format == '\\') {
      //    
      // } else 
      {
out_lbl:
			printchar (out, *format);
			++pc;
		}
   }  //  for each char in format string
   if (out) //lint !e850
		**out = '\0';
	return pc;
}

//****************************************************************************
//  assuming sizeof(void *) == sizeof(int)
//  This function is not valid in embedded projects which don't
//  have a meaningful stdout device.
//****************************************************************************
#ifdef TEST_PRINTF
int termf (const char *format, ...)
{
   max_output_len = -1 ;
   curr_output_len = 0 ;
   int *varg = (int *) (char *) (&format);
   return print (0, varg);
}  //lint !e715
#endif

//****************************************************************************
//  assuming sizeof(void *) == sizeof(int)
//  This function is not valid in embedded projects which don't
//  have a meaningful stdout device.
//****************************************************************************
#ifdef TEST_PRINTF
int termfn(uint max_len, const char *format, ...)
{
   max_output_len = (int) max_len ;
   curr_output_len = 0 ;
   int *varg = (int *) (char *) (&format);
   return print (0, varg);
}  //lint !e715
#endif

//****************************************************************************
#ifndef TEST_PRINTF
int stringf (char *out, const char *format, ...) HIGHSEG
#endif
int stringf (char *out, const char *format, ...)
{
   max_output_len = -1 ;
   curr_output_len = 0 ;
   //  create a pointer into the stack.
   //  Thematically this should be a void*, since the actual usage of the
   //  pointer will vary.  However, int* works fine too.
   //  Either way, the called function will need to re-cast the pointer
   //  for any type which isn't sizeof(int)
   int *varg = (int *) (char *) (&format);
   return print (&out, varg);
}

//****************************************************************************
//lint -esym(714, stringfn)
//lint -esym(759, stringfn)
//lint -esym(765, stringfn)
#ifndef TEST_PRINTF
int stringfn(char *out, uint max_len, const char *format, ...) HIGHSEG
#endif
int stringfn(char *out, uint max_len, const char *format, ...)
{
   max_output_len = (int) max_len ;
   curr_output_len = 0 ;
   //  create a pointer into the stack.
   //  Thematically this should be a void*, since the actual usage of the
   //  pointer will vary.  However, int* works fine too.
   //  Either way, the called function will need to re-cast the pointer
   //  for any type which isn't sizeof(int)
   int *varg = (int *) (char *) (&format);
   return print (&out, varg);
}

//********************************************************************
//  On Windows platform, try to redefine printf/fprintf
//  so we can output code to a debug window.
//  Also, shadow syslog() within OutputDebugStringA()
//  Note: printf() remapping was unreliable,
//  but syslog worked great.
//********************************************************************
#ifdef   USE_INTERNALS
#ifndef TEST_PRINTF
int stuff_talkf(const char *format, ...) HIGHSEG
#endif
int stuff_talkf(const char *format, ...)
{
   max_output_len = (int) TALK_LINE_LEN ;
   curr_output_len = 0 ;
   char consoleBuffer[TALK_LINE_LEN+1] ;
   char *out = consoleBuffer ;
   int *varg = (int *) (char *) (&format);
   print (&out, varg);
   stuff_talk(consoleBuffer) ;
   return 1;
}
#endif

//****************************************************************************
#ifdef TEST_PRINTF
int main (void)
{
   int slen ;
   char *ptr = "Hello world!";
   char *np = 0;
   char buf[256];
   termf ("ptr=%s, %s is null pointer, char %c='a'\n", ptr, np, 'a');
   termf ("hex %x = ff, hex02=%02x\n", 0xff, 2);   //  hex handling
   termf ("signed %d = %uU = 0x%X\n", -3, -3, -3);   //  int formats
   termf ("%d %s(s) with %%\n", 0, "message");

   slen = stringf (buf, "justify: left=\"%-10s\", right=\"%10s\"\n", "left", "right");
   termf ("[len=%d] %s", slen, buf);
   
   stringf(buf, "     padding (pos): zero=[%04d], left=[%-4d], right=[%4d]\n", 3, 3, 3) ;
   termf ("%s", buf);

   //  test walking string builder
   slen = 0 ;
   slen += stringf(buf+slen, "padding (neg): zero=[%04d], ", -3) ;   //lint !e845
   slen += stringf(buf+slen, "left=[%-4d], ", -3) ;
   slen += stringf(buf+slen, "right=[%4d]\n", -3) ;
   termf ("[%d] %s", slen, buf);
   termf("+ format: int: %+d, %+d, double: %+.1f, %+.1f, reset: %d, %.1f\n", 3, -3, 3.0, -3.0, 3, 3.0) ;

   stringf (buf, "%.2f is a double\n", 3.31) ;
   termf ("%s", buf);

   stringf (buf, "multiple unsigneds: %u %u %2u %X\n", 15, 5, 23, 0xB38F) ;
   termf ("%s", buf);

   stringf (buf, "multiple chars: %c %c %c %c\n", 'a', 'b', 'c', 'd') ;
   termf ("%s", buf);

   stringf (buf, "multiple doubles: %f %.1f %2.0f %.2f %.3f %.2f [%-8.3f]\n",
                  3.45, 3.93, 2.45, -1.1, 3.093, 13.72, -4.382) ;
   termf ("%s", buf);
   stringf (buf, "double special cases: %f %.f %.0f %2f %2.f %2.0f\n",
                  3.14159, 3.14159, 3.14159, 3.14159, 3.14159, 3.14159) ;
   termf ("%s", buf);
   stringf (buf, "rounding doubles: %.1f %.1f %.3f %.2f [%-8.3f]\n",
                  3.93, 3.96, 3.0988, 3.999, -4.382) ;
   termf ("%s", buf);

   stringf (buf, "long long: %lld, %llu, 0x%llX\n", -831326121984LL, 831326121984LLU, 831326121984LLU) ;
   // stringf (buf, "long long: %llu\n", 831326121984LLU) ;
   termf ("%s", buf);

   termfn(20, "%s", "no more than 20 bytes of this string should be displayed!");
   termf ("\n");
   stringfn(buf, 25, "only 25 buffered bytes should be displayed from this string") ;
   termf ("%s", buf);
   termf ("\n");

   //  test case from our Yagarto ARM9 problem
   // char *cmd = "adc_vref " ;
   // float fvalue = 3.30 ;
   // stringf (buf, "%s%.2f", cmd, (double) fvalue);
   // printf("%s\n", buf);

   return 0;
}
#endif

