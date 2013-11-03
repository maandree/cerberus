/**
 * cerberus – Minimal login program
 * 
 * Copyright © 2013  Mattias Andrée (maandree@member.fsf.org)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "passphrase.h"


#define START_PASSPHRASE_LIMIT  32


/**
 * The original TTY settings
 */
static struct termios saved_stty;


/**
 * Reads the passphrase from stdin
 * 
 * @return  The passphrase, should be `free`:ed
 */
char* get_passphrase(void)
{
  /* malloc and realloc returns NULL if we run out of memory,
     we will not do that under normal usecases, if we do, it
     okay to segfault on null derefencing and quit on that. */
  
  char* rc = malloc(START_PASSPHRASE_LIMIT);
  long size = START_PASSPHRASE_LIMIT;
  long len = 0;
  int c;
  
  /* Read password until EOF or Enter, skip all ^0 as that
     is probably not a part of the passphrase (good luck typing
     that in X.org) and can be echoed into stdin by the kernel. */
  for (;;)
    {
      c = getchar();
      if ((c < 0) || (c == '\n'))
	break;
      if (c != 0)
        {
	  *(rc + len++) = c;
	  if (len == size)
	    rc = realloc(rc, size <<= 1L);
	}
    }
  
  /* NUL-terminate passphrase */
  *(rc + len) = 0;
  
  return rc;
}


/**
 * Disable echoing and do anything else to the terminal settnings `get_passphrase` requires
 */
void disable_echo()
{
  struct termios stty;
  
  tcgetattr(STDIN_FILENO, &saved_stty);
  stty = saved_stty;
  stty.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &stty);
}


/**
 * Undo the actions of `disable_echo`
 */
void reenable_echo()
{
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
}

