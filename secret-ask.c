/* Quintuple Agent ask-for-confirmation module
 * Copyright (C) 2000 Robert Bihlmeyer <robbe@orcus.priv.at>
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "i18n.h"

void yes(GtkWidget *w, gpointer data)
{
  gtk_exit(2);
}

void no(GtkWidget *w, gpointer data)
{
  gtk_exit(3);
}

void usage()
{
  printf(_("Usage: secret-ask bool QUESTION\n\
Pop up a window asking QUESTION, soliciting either Yes or No as answer.\n\
\n\
If a positive answer is selected the exit status is 2.\n\
If a negative answer is selected the exit status is 3.\n\
On errors, or if the decision is avioded the exit status is 1.\n"));
}

int main(int argc, char **argv)
{
  gtk_init(&argc, &argv);
  if (argc <= 2 || strcmp(argv[1], "bool") != 0) {
    usage();
    gtk_exit(1);
  }
  {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkAccelGroup *acc = gtk_accel_group_new();
    gtk_signal_connect(GTK_OBJECT(win), "delete_event", gtk_main_quit, NULL);
    gtk_signal_connect(GTK_OBJECT(win), "destroy", gtk_main_quit, NULL);
    gtk_accel_group_attach(acc, GTK_OBJECT(win));
    {
      GtkWidget *box = gtk_vbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(win), box);
      {
	GtkWidget *w = gtk_label_new(argv[2]);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 5);
	gtk_widget_show(w);
      }
      {
	GtkWidget *bbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(box), bbox, FALSE, FALSE, 0);
	{
	  GtkWidget *w = gtk_button_new_with_label(_("Yes"));
	  gtk_container_add(GTK_CONTAINER(bbox), w);
	  gtk_signal_connect(GTK_OBJECT(w), "clicked", yes, NULL);
	  GTK_WIDGET_SET_FLAGS(w, GTK_CAN_DEFAULT);
	  gtk_widget_grab_default(w);
	  gtk_widget_show(w);
	}
	{
	  GtkWidget *w = gtk_button_new_with_label(_("No"));
	  gtk_container_add(GTK_CONTAINER(bbox), w);
	  gtk_accel_group_add(acc, GDK_Escape, 0, 0, GTK_OBJECT(w), "clicked");
	  gtk_signal_connect(GTK_OBJECT(w), "clicked", no, NULL);
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
