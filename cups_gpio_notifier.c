/*
CUPS GPIO Notifier daemon for Raspberry Pi
Copyright (C) 2013 Lukasz Skalski <lukasz.skalski@op.pl>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* Tested on 2013-09-25-wheezy-raspbian */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <glib.h>
#include <gio/gio.h>

/* 
 * Globals
 */
GMainLoop *loop = NULL;
gint gpio_num = 23;
gint time_to_off = 180;
guint source_tag = 0;

/*
 * Functions prototypes 
 */
int export_GPIO (gint gpio_pin);
int unexport_GPIO (gint gpio_pin);
int set_output_GPIO (gint gpio_pin);
int set_GPIO (gint gpio_pin, gint value);
void unix_signal_handler (int signo);
int printer_off_timeout (gpointer user_data);
void demonize (void);
void dbus_signal_callback (GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data);

/* 
 * Commandline options 
 */
GOptionEntry entries[] =
{
  { "gpio", 'g', 0, G_OPTION_ARG_INT, &gpio_num, "GPIO Output Number [defaut: 23]", NULL },
  { "time", 't', 0, G_OPTION_ARG_INT, &time_to_off, "number of seconds to turn off the printer [default: 180]", NULL },
  { NULL }
};

/*
 * unix_signal_handler
 */
void unix_signal_handler (int signo)
{
  if (signo == SIGINT)
    g_main_loop_quit (loop);
}

/*
 * printer_off_timeout
 */
int printer_off_timeout (gpointer user_data)
{
  set_GPIO (gpio_num,1);
  return FALSE;
}

/*
 * dbus_signal_callback
 */
void dbus_signal_callback (GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data)
{
  if (source_tag > 0)
    g_source_remove (source_tag);
  
  source_tag = g_timeout_add_seconds (time_to_off, printer_off_timeout,NULL);
  set_GPIO (gpio_num,0);
}

/*
 * export_GPIO
 */
int export_GPIO (gint gpio_pin){

  FILE *fd;

  fd = fopen ("/sys/class/gpio/export", "w");

  if (!fd)
    return -1;
  else
    fprintf (fd,"%d", gpio_pin);

  fclose (fd);
  return 0;
}

/*
 * unexport_GPIO
 */
int unexport_GPIO (gint gpio_pin){	

  FILE *fd;

  fd = fopen ("/sys/class/gpio/unexport", "w");

  if (!fd)
    return -1;
  else
    fprintf (fd,"%d", gpio_pin);

  fclose (fd);
  return 0;
}

/*
 * set_output_GPIO
 */
int set_output_GPIO (gint gpio_pin){

  FILE *fd;
  GString *path;

  path = g_string_new (NULL);
  g_string_printf (path, "/sys/class/gpio/gpio%d%s",gpio_pin,"/direction");

  fd = fopen (path->str, "w");

  if (!fd)
    return -1;
  else
    fprintf (fd,"%s","out");

  fclose (fd);
  return 0;
}

/*
 * set_GPIO
 */
int set_GPIO (gint gpio_pin, gint value){

  FILE *fd;
  GString *path;

  path = g_string_new (NULL);
  g_string_printf (path, "/sys/class/gpio/gpio%d%s",gpio_pin,"/value");

  fd = fopen (path->str, "w");

  if (!fd)
    return -1;
  else
    fprintf (fd,"%d", value);

  fclose (fd);
  return 0;
}

/*
 * demonize
 */
void demonize (void)
{
  pid_t pid;
  pid_t sid;

  pid = fork();
  if (pid < 0)
    exit (EXIT_FAILURE);

  if (pid > 0)
    exit (EXIT_SUCCESS);

  umask(0);

  sid = setsid();
  if (sid < 0)
    exit (EXIT_FAILURE);

  if ((chdir("/")) < 0)
    exit (EXIT_FAILURE);

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
}

/*
 * Main function
 */
int main (int argc, char** argv)
{

  GDBusConnection *connection = NULL;
  GOptionContext *context = NULL;
  GError *error = NULL;

#ifndef HAVE_GLIB_2_36
    g_type_init ();
#endif
 
  /* open syslog */
  openlog("CUPS GPIO Notifier", LOG_NOWAIT|LOG_PID, LOG_USER);
  syslog(LOG_NOTICE, "Starting 'Cups GPIO Notifier' daemon...\n");

  /* signal handler SIGINT */
  if (signal (SIGINT, &unix_signal_handler) == SIG_ERR)
  {
    syslog(LOG_ERR, "can't catch SIGINT signal\n");
    exit (EXIT_FAILURE);
  }

  /* parses commandline options */
  context = g_option_context_new ("- Cups GPIO Notifier");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    syslog(LOG_NOTICE, "option parsing failed: %s\n", error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }

  /* demonize */
  demonize ();

  /* export selected GPIO */
  if (export_GPIO (gpio_num) < 0)
  {
    syslog(LOG_NOTICE, "export_GPIO() - error\n");
    exit (EXIT_FAILURE);
  }

  if (set_output_GPIO (gpio_num) < 0)
  {
    syslog(LOG_NOTICE, "set_output_GPIO - error\n");
    exit (EXIT_FAILURE);
  }

  if (set_GPIO (gpio_num, 1) < 0)
  {
    syslog(LOG_NOTICE, "set_GPIO - error\n");
    exit (EXIT_FAILURE);
  }

  /* get the system bus connection */
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

  if (connection == NULL)
  {
    syslog(LOG_ERR, "error connecting to D-Bus system bus: %s\n", error->message);
    g_error_free (error);
    exit (EXIT_FAILURE);
  }

  /* subscribe dbus signal */
  g_dbus_connection_signal_subscribe (connection,                  //connection
                                      NULL,                        //sender name
                                      "com.redhat.PrinterSpooler", //interface name
                                      "JobQueuedLocal",            //signal name
                                      NULL,                        //object path
                                      NULL,                        //arguments
                                      G_DBUS_SIGNAL_FLAGS_NONE,    //flags
                                      dbus_signal_callback,        //callback function
                                      NULL,                        //user data,
                                      NULL);                       //function to free user data

  /* start main loop */
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  /* close app */
  syslog(LOG_NOTICE, "Exiting 'CUPS GPIO Notifier' daemon...\n");
  unexport_GPIO (gpio_num);
  g_main_loop_unref (loop);

  return 0;
}

