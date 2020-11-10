/* Quintuple Agent passphrase query module for X
 * Copyright (C) 1999 Robert Bihlmeyer <robbe@orcus.priv.at>
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

#define _GNU_SOURCE

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <X11/X.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else 
#include "getopt.h"
#endif /* HAVE_GETOPT_H */

#include "i18n.h"
#include "gtksecentry.h"
#include "memory.h"
#include "util.h"

int debug = 0, enhanced = 0;

static GtkWidget *entry, *insure, *timeout;

/* usage - show usage */
void usage()
{
  printf(_("Usage: secret-query [OPTION]... [QUESTION]\n\
Ask securely for a secret and print it to stdout.\n\
Put QUESTION in the window, if given.\n\
\n\
  -e, --enhanced        ask for timeout and insurance, too\n\
  -g, --no-global-grab  grab keyboard only while window is focused\n\
  -d, --debug           turn on debugging output\n\
      --help            display this help and exit\n\
      --version         output version information and exit\n"));
}

/* ok - print the passphrase to stdout and exit */
void ok(GtkWidget *w, gpointer data)
{
  if (enhanced) {
    printf("Options: %s\nTimeout: %d\n\n",
	   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(insure)) ? "insure"
								   : "",
	   gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(timeout)));
  }
  printf("%s\n", gtk_secure_entry_get_text(GTK_SECURE_ENTRY(entry)));
  gtk_exit(0);
}

/* unselect - work around a bug in GTK+ that permits word-selection to
   work on the invisible passphrase */
void unselect(GtkWidget *w, GdkEventButton *e)
{
  static gint lastpos;

  if (e->button == 1) {
    if (e->type == GDK_BUTTON_PRESS) {
      lastpos = gtk_editable_get_position(GTK_EDITABLE(entry));
    } else if (e->type == GDK_2BUTTON_PRESS || e->type == GDK_3BUTTON_PRESS) {
      gtk_secure_entry_set_position(GTK_SECURE_ENTRY(w), lastpos);
      gtk_secure_entry_select_region(GTK_SECURE_ENTRY(w), 0, 0);
    }
  }
}

/* constrain_size - constrain size of the window
 the window should not shrink beyond the requisition, and should not grow
 vertically
*/
void constrain_size(GtkWidget *win, GtkRequisition *req, gpointer data)
{
  static gint width, height;
  GdkGeometry geo;

  if (req->width == width && req->height == height)
    return; 
  width = req->width;
  height = req->height;
  geo.min_width = width;
  geo.max_width = 10000;	/* this limit is arbitrary,
				   but INT_MAX breaks other things */
  geo.min_height = geo.max_height = height;
  gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &geo,
  				GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
}

/* grab_keyboard - grab the keyboard for maximum security */
void grab_keyboard(GtkWidget *win, GdkEvent *event, gpointer data)
{
  if (gdk_keyboard_grab(win->window, FALSE, gdk_event_get_time(event))) {
    g_error(_("could not grab keyboard"));
  }
}

/* ungrab_keyboard - remove grab */
void ungrab_keyboard(GtkWidget *win, GdkEvent *event, gpointer data)
{
  gdk_keyboard_ungrab(gdk_event_get_time(event));
}

int main(int argc, char **argv)
{
  int opt, opt_help = 0, opt_version = 0;
  int global_grab = 1;
  struct option opts[] = {{ "debug",	      no_argument, NULL,	'd' },
			  { "enhanced",       no_argument, NULL,	'e' },
			  { "no-global-grab", no_argument, NULL,	'g' },
			  { "help",	      no_argument, &opt_help,	 1  },
			  { "version",	      no_argument, &opt_version, 1  },
			  { NULL, 0, NULL, 0 }};

  secmem_init(1);		/* 1 is too small, so default size is used */
  secmem_set_flags(SECMEM_WARN);
  drop_privs();

  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  gtk_init(&argc, &argv);
  while ((opt = getopt_long(argc, argv, "deg", opts, NULL)) != -1)
    switch (opt) {
    case 'd':
      debug = 1;
      break;
    case 'e':
      enhanced = 1;
      break;
    case 'g':
      global_grab = 0;
      break;
    case 0:
    case '?':
      break;
    default:
      g_error("impossible case in %s:%d", __FILE__, __LINE__);
    }
  if (opt_version) {
    printf("secret-query " VERSION " (" PACKAGE ")\n");
    exit(EXIT_SUCCESS);
  }
  if (opt_help) {
    usage();
    exit(EXIT_SUCCESS);
  }
  {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkAccelGroup *acc = gtk_accel_group_new();
    gtk_signal_connect(GTK_OBJECT(win), "delete_event", gtk_main_quit, NULL);
    gtk_signal_connect(GTK_OBJECT(win), "destroy", gtk_main_quit, NULL);
    gtk_signal_connect(GTK_OBJECT(win), "size-request", constrain_size, NULL);
    gtk_signal_connect(GTK_OBJECT(win),
		       global_grab ? "map-event" : "focus-in-event",
		       grab_keyboard, NULL);
    gtk_signal_connect(GTK_OBJECT(win),
		       global_grab ? "unmap-event" : "focus-out-event",
		       ungrab_keyboard, NULL);
    gtk_accel_group_attach(acc, GTK_OBJECT(win));
    {
      GtkWidget *box = gtk_vbox_new(FALSE, 5);
      gtk_container_add(GTK_CONTAINER(win), box);
      if (argc <= optind) {
	GtkWidget *w = gtk_label_new(_("Please give me your secret:"));
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_show(w);
      } else
	for (; optind < argc; optind++) {
	  GtkWidget *w = gtk_label_new(argv[optind]);
	  gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	  gtk_widget_show(w);
	}
      {
	entry = gtk_secure_entry_new();
	gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
	gtk_secure_entry_set_visibility(GTK_SECURE_ENTRY(entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(entry), "activate", ok, NULL);
	gtk_signal_connect_after(GTK_OBJECT(entry), "button_press_event",
				 unselect, NULL);
	gtk_widget_grab_focus(entry);
	gtk_widget_show(entry);
      }
      if (enhanced) {
	GtkWidget *sbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), sbox, FALSE, FALSE, 0);
	{
	  GtkWidget *w = gtk_label_new("Forget secret after");
	  gtk_box_pack_start(GTK_BOX(sbox), w, FALSE, FALSE, 0);
	  gtk_widget_show(w);
	}
	{
	  timeout =
	    gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0,
								  HUGE_VAL, 1,
								  60, 60)),
				2, 0);
	  gtk_box_pack_start(GTK_BOX(sbox), timeout, FALSE, FALSE, 0);
	  gtk_widget_show(timeout);
	}
	{
	  GtkWidget *w = gtk_label_new("seconds");
	  gtk_box_pack_start(GTK_BOX(sbox), w, FALSE, FALSE, 0); 
	  gtk_widget_show(w);
	}
	gtk_widget_show(sbox);
	insure =
	  gtk_check_button_new_with_label(_("ask before giving out secret"));
	gtk_box_pack_start(GTK_BOX(box), insure, FALSE, FALSE, 0);
	gtk_widget_show(insure);
      }
      {
	GtkWidget *bbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(box), bbox, FALSE, FALSE, 0);
	{
	  GtkWidget *w = gtk_button_new_with_label(_("OK"));
	  gtk_container_add(GTK_CONTAINER(bbox), w);
	  gtk_signal_connect(GTK_OBJECT(w), "clicked", ok, NULL);
	  GTK_WIDGET_SET_FLAGS(w, GTK_CAN_DEFAULT);
	  gtk_widget_grab_default(w);
	  gtk_widget_show(w);
	}
	{
	  GtkWidget *w = gtk_button_new_with_label(_("Cancel"));
	  gtk_container_add(GTK_CONTAINER(bbox), w);
	  gtk_accel_group_add(acc, GDK_Escape, 0, 0,
			      GTK_OBJECT(w), "clicked");
	  gtk_signal_connect(GTK_OBJECT(w), "clicked", gtk_main_quit, NULL);
	  GTK_WIDGET_SET_FLAGS(w, GTK_CAN_DEFAULT);
	  gtk_widget_show(w);
	}
	gtk_widget_show(bbox);
      }
      gtk_widget_show(box); 
    }
    gtk_widget_show(win);
  }
  gtk_main();
  return 1;
}
