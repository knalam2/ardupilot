/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  expanding string for easy construction of text buffers
 */

#include "ExpandingString.h"

extern const AP_HAL::HAL& hal;

#define EXPAND_INCREMENT 512

/*
  expand the string buffer
 */
bool ExpandingString::expand(uint32_t min_extra_space_needed)
{
    // expand a reasonable amount
    uint32_t newsize = (5*buflen/4) + EXPAND_INCREMENT;
    if (newsize - used < min_extra_space_needed) {
        newsize = used + min_extra_space_needed;
    }
    
    // add one to ensure we are always null terminated
    void *newbuf = hal.util->std_realloc(buf, newsize+1);

    if (newbuf == nullptr) {
        allocation_failed = true;
        return false;
    }

    buflen = newsize;
    buf = (char *)newbuf;

    return true;
}

/*
  print into the buffer, expanding if needed
 */
void ExpandingString::printf(const char *format, ...)
{
    if (allocation_failed) {
        return;
    }
    if (buflen == used && !expand(0)) {
        return;
    }
    int n;

    /*
      print into the buffer, expanding the buffer if needed
     */
    while (true) {
        va_list arg;
        va_start(arg, format);
        n = hal.util->vsnprintf(&buf[used], buflen-used, format, arg);
        va_end(arg);
        if (n < 0) {
            return;
        }
        if (uint32_t(n) < buflen - used) {
            break;
        }
        if (!expand(n+1)) {            
            return;
        }
    }
    used += n;
}

/*
  print into the buffer, expanding if needed
 */
bool ExpandingString::append(const char *s, uint32_t len)
{
    if (allocation_failed) {
        return false;
    }
    if (buflen - used < len && !expand(len)) {
        return false;
    }
    if (s != nullptr) {
        memcpy(&buf[used], s, len);
    }
    used += len;
    return true;
}

ExpandingString::~ExpandingString()
{
    free(buf);
}
