/*
 *  Copyright (c) 2009-2020 Giuseppe Torelli <colossus73@gmail.com>
 *  Copyright (c) 2009 Tadej Borovšak 	<tadeboro@gmail.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License,or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not,write to the Free Software
 *  Foundation,Inc.,59 Temple Place - Suite 330,Boston,MA 02111-1307,USA.
 *
 */

#include "new_slideshow.h"

void img_new_slideshow_settings_dialog(img_window_struct *img)
{
	GtkWidget *dialog1;
	GtkWidget *dialog_vbox1;
	GtkWidget *vbox1;
	GtkWidget *grid;
	GtkWidget *width;
	GtkWidget *height;
	GtkWidget *ex_hbox;
	GtkWidget *distort_button;
	GtkWidget *bg_button;
	GdkRGBA   color;
	GtkWidget *label;
	gint       response;
    
	dialog1 = gtk_dialog_new_with_buttons( _("Create a new slideshow"),
										GTK_WINDOW(img->imagination_window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										"_Cancel", GTK_RESPONSE_CANCEL,
										"_Ok", GTK_RESPONSE_ACCEPT, NULL);
	dialog_vbox1 = gtk_dialog_get_content_area( GTK_DIALOG( dialog1 ) );
	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 5);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);
 
    gtk_widget_set_halign(GTK_WIDGET(vbox1), GTK_ALIGN_FILL);
    gtk_widget_set_margin_top(GTK_WIDGET(vbox1), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(vbox1), 5);
    gtk_widget_set_margin_start(GTK_WIDGET(vbox1), 5);
    gtk_widget_set_margin_end(GTK_WIDGET(vbox1), 5);

	grid = gtk_grid_new();
	gtk_box_pack_start( GTK_BOX( vbox1 ), grid, FALSE, FALSE, 0 );
	gtk_grid_set_row_spacing(GTK_GRID(grid), 7);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);

	label = gtk_label_new( _("Width:") );
	gtk_grid_attach( GTK_GRID(grid), label, 0, 0, 1, 1);
	
	width = gtk_spin_button_new_with_range (300, 9999, 10);
	gtk_grid_attach( GTK_GRID(grid), width, 1, 0, 1, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(width), img->video_size[0]);
	
	label = gtk_label_new("<b>8k\n4k:</b>");
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_grid_attach( GTK_GRID(grid), label, 2, 0, 1, 1);

	label = gtk_label_new("7680 x 4320\n4096 x 2160");
	gtk_grid_attach( GTK_GRID(grid), label, 3, 0, 1, 1);
    
	label = gtk_label_new( _("Height:") );
	gtk_grid_attach( GTK_GRID(grid), label, 0, 1, 1, 1);

	height = gtk_spin_button_new_with_range(300, 9999, 10);
	gtk_grid_attach( GTK_GRID(grid), height, 1, 1, 1, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(height), img->video_size[1]);
	
	label = gtk_label_new("<b>Full HD:\nHD:</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_grid_attach( GTK_GRID(grid), label, 2, 1, 1, 1);

	label = gtk_label_new("1920 x 1080\n1280 x 720");
	gtk_grid_attach( GTK_GRID(grid), label, 3, 1, 1, 1);
    
	ex_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start( GTK_BOX( vbox1 ), ex_hbox, FALSE, FALSE, 0 );
	distort_button = gtk_check_button_new_with_label( _("Distort images to fill the whole screen") );
	gtk_box_pack_start( GTK_BOX( vbox1 ), distort_button, FALSE, FALSE, 0 );

	ex_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start( GTK_BOX( vbox1 ), ex_hbox, FALSE, FALSE, 0 );

	img->bye_bye_transition_checkbox = gtk_check_button_new_with_label( _("End slideshow with blank slide") );
	gtk_box_pack_start( GTK_BOX( ex_hbox ), img->bye_bye_transition_checkbox, FALSE, FALSE, 0 );
	
	color.red   = img->background_color[0];
	color.green = img->background_color[1];
	color.blue  = img->background_color[2];
	color.alpha = 1.0;

	bg_button = gtk_color_button_new_with_rgba( &color );
	gtk_box_pack_start( GTK_BOX( ex_hbox ), bg_button, FALSE, FALSE, 0 );
	gtk_widget_show_all(dialog_vbox1);

	/* Set parameters */
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( distort_button ), img->distort_images );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( img->bye_bye_transition_checkbox ), img->bye_bye_transition );

	response = gtk_dialog_run(GTK_DIALOG(dialog1));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		img_close_slideshow(NULL, img);
		
		img->video_size[0] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(width));
		img->video_size[1] = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(height));
		img->video_ratio = (gdouble)img->video_size[0] / img->video_size[1];

		GdkRGBA new;

		/* Get distorsion settings */
		img->distort_images = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( distort_button ) );

		/* Get bye bye transition settings */
		img->bye_bye_transition = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( img->bye_bye_transition_checkbox) );

		/* Set the max value of slide subtitles hrange scale
		* according to the new video size */
		gtk_adjustment_set_upper(img->sub_posX_adj, (gdouble)img->video_size[0]);
		gtk_adjustment_set_upper(img->sub_posY_adj, (gdouble)img->video_size[1]);
	
		/* Get color settings */
		gtk_color_chooser_get_rgba( GTK_COLOR_CHOOSER(bg_button), &new);
		img->background_color[0] = (gdouble)new.red;
		img->background_color[1] = (gdouble)new.green;
		img->background_color[2] = (gdouble)new.blue;
	
		/* Adjust zoom level */
        img_zoom_fit(NULL, img);
	}
	gtk_widget_destroy(dialog1);
}

