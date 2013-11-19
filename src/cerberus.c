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
#include "cerberus.h"

/* TODO use log */


#ifdef USE_TTY_GROUP
static gid_t tty_group = 0;
#endif
static struct passwd* entry;
static pid_t child_pid;


void do_login(int argc, char** argv);


/**
 * Mane method
 * 
 * @param   argc  The number of command line arguments
 * @param   argv  The command line arguments
 * @return        Return code
 */
int main(int argc, char** argv)
{
  int _status;
  
  do_login(argc, argv);
  
  /* Ignore signals */
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);
  
  /* Wait for the login shell to exit  */
  waitpid(child_pid, &_status, 0);
  
  /* Reset terminal ownership and mode */
  chown_tty(0, tty_group, 0);
  
  return 0;
}


/**
 * Do everything before the fork and do everything the exec fork
 * 
 * @param   argc  The number of command line arguments
 * @param   argv  The command line arguments
 */
void do_login(int argc, char** argv)
{
  char* username = NULL;
  char* hostname = NULL;
  char* passphrase = NULL;
  char preserve_env = 0;
  char skip_auth = 0;
  #ifdef USE_TTY_GROUP
  struct group* group;
  #endif
  
  
  /* Disable echoing */
  disable_echo();
  /* This should be done as early and quickly as possible so as little
     as possible of the passphrase gets leaked to the output if the user
     begins entering the passphrase directly after the username. */
  
  
  /* Set process group ID */
  setpgrp();
  
  
  /* Parse command line arguments */
  {
    char double_dashed = 0;
    char hostname_on_next = 0;
    int i;
    for (i = 1; i < argc; i++)
      {
	char *arg = *(argv + i);
	char c;
	
	if (*arg == 0)
	  ;
	else if ((*arg == '-') && (double_dashed == 0))
	  while ((c = *(++arg)))
	    if ((c == 'V') || (c == 'H'))
	      ;
	    else if (c == 'p')
	      preserve_env = 1;
	    else if (c == 'h')
	      {
		if (*(arg + 1))
		  hostname = arg + 1;
		else
		  hostname_on_next = 1;
		break;
	      }
	    else if (c == 'f')
	      {
		if (*(arg + 1))
		  username = arg + 1;
		skip_auth = 1;
		break;
	      }
	    else if (c == '-')
	      {
		double_dashed = 1;
		break;
	      }
	    else
	      printf("%s: unrecognised options: -%c\n", *argv, c);
	else if (hostname_on_next)
	  {
	    hostname = arg;
	    hostname_on_next = 0;
	  }
	else
	  username = arg;
      }
  }
  
  
  /* Change that a username has been specified */
  if (username == 0)
    {
      printf("%s: no username specified\n", *argv);
      sleep(ERROR_SLEEP);
      _exit(2);
    }
  
  
  /* Print ant we want a passphrase, if -f has not been used */
  if (skip_auth == 0)
    {
      printf("Passphrase: ");
      fflush(stdout);
    }
  /* Done early to make to program look like it is even faster than it is */
  
  
  /* Make sure nopony is spying */
  #ifdef USE_TTY_GROUP
  if ((group = getgrnam(TTY_GROUP)))
    tty_group = group->gr_gid;
  #endif
  secure_tty(tty_group);
  
  /* Redisable echoing */
  disable_echo();
  
  
  /* Set up clean quiting and time out */
  signal(SIGALRM, timeout_quit);
  signal(SIGQUIT, user_quit);
  signal(SIGINT, user_quit);
  siginterrupt(SIGALRM, 1);
  siginterrupt(SIGQUIT, 1);
  siginterrupt(SIGINT, 1);
  alarm(TIMEOUT_SECONDS);
  
  
  /* Get user information */
  if ((entry = getpwnam(username)) == NULL)
    {
      if (errno)
	perror("getpwnam");
      else
	printf("User does not exist\n");
      sleep(ERROR_SLEEP);
      _exit(1);
    }
  username = entry->pw_name;
  
  
  /* Get the passphrase, if -f has not been used */
  if (skip_auth == 0)
    {
      passphrase = get_passphrase();
      printf("\n");
    }
  
  /* Passphrase entered, turn off timeout */
  alarm(0);
  
  /* TODO verify passphrase */
  
  /* Wipe and free the passphrase from the memory */
  if (skip_auth == 0)
    {
      long i;
      for (i = 0; *(passphrase + i); i++)
	*(passphrase + i) = 0;
      free(passphrase);
    }
  
  
  /* Reset terminal settings */
  reenable_echo();
  
  
  /* Partial login */
  /* TODO verify that user is enabled */
  chown_tty(entry->pw_uid, tty_group, 0);
  chdir_home(entry);
  ensure_shell(entry);
  set_environ(entry, preserve_env);
  
  
  /* Stop signal handling */
  signal(SIGALRM, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_IGN);
  
  
  child_pid = fork();
  /* vfork cannot be used as the child changes the user,
     the parent would not be able to chown the TTY */
    
  if (child_pid == -1)
    {
      perror("fork");
      sleep(ERROR_SLEEP);
      _exit(1);
    }
  else if (child_pid == 0)
    {
      int ret;
      
      /* In case the shell does not do this */
      setsid();
      
      /* Set controlling terminal */
      if (ioctl(STDIN_FILENO, TIOCSCTTY, 1))
	perror("TIOCSCTTY");
      signal(SIGINT, SIG_DFL);
      
      /* Partial login */
      ret = entry->pw_uid
	? initgroups(username, entry->pw_gid) /* supplemental groups for user, can require network     */
	: setgroups(0, NULL);                 /* supplemental groups for root, does not require netork */
      if (ret == -1)
	{
	  perror(entry->pw_uid ? "initgroups" : "setgroups");
	  sleep(ERROR_SLEEP);
	  _exit(1);
	}
      set_user(entry);
      exec_shell(entry);
    }
}

