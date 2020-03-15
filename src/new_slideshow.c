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
	GtkWidget *main_frame;
	GtkWidget *ex_vbox;
	GtkWidget *ex_hbox;
	GtkWidget *distort_button;
	GtkWidget *bg_button;
	GtkWidget *bg_label;
	GdkRGBA   color;
	GtkWidget *label;
	gint       response;
    
	dialog1 = gtk_dialog_new_with_buttons( _("Create a new slideshow"),
										GTK_WINDOW(img->imagination_window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										"_Cancel", GTK_RESPONSE_CANCEL,
										"_Ok", GTK_RESPONSE_ACCEPT, NULL);

	gtk_window_set_default_size(GTK_WINDOW(dialog1),520,-1);

	dialog_vbox1 = gtk_dialog_get_content_area( GTK_DIALOG( dialog1 ) );
	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 5);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

	main_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox1), main_frame, TRUE, TRUE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (main_frame), GTK_SHADOW_IN);

	label = gtk_label_new (_("<b>Slideshow Settings</b>"));
	gtk_frame_set_label_widget (GTK_FRAME (main_frame), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

    ex_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(main_frame), ex_vbox);
    gtk_widget_set_halign(GTK_WIDGET(ex_vbox), GTK_ALIGN_FILL);
    gtk_widget_set_margin_top(GTK_WIDGET(ex_vbox), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(ex_vbox), 15);
    gtk_widget_set_margin_start(GTK_WIDGET(ex_vbox), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(ex_vbox), 10);

	distort_button = gtk_check_button_new_with_label( _("Rescale images to fit desired aspect ratio") );
	gtk_box_pack_start( GTK_BOX( ex_vbox ), distort_button, FALSE, FALSE, 0 );

	ex_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start( GTK_BOX( ex_vbox ), ex_hbox, FALSE, FALSE, 0 );

	img->bye_bye_transition_checkbox = gtk_check_button_new_with_label( _("End slideshow with blank slide") );
	gtk_box_pack_start( GTK_BOX( ex_hbox ), img->bye_bye_transition_checkbox, FALSE, FALSE, 0 );
	
	bg_label = gtk_label_new( _("Select blank slide color:") );
	gtk_box_pack_start( GTK_BOX( ex_hbox ), bg_label, TRUE, FALSE, 0 );

	color.red   = img->background_color[0] * 0xffff;
	color.green = img->background_color[1] * 0xffff;
	color.blue  = img->background_color[2] * 0xffff;
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

		GdkRGBA new;

		/* Get distorsion settings */
		img->distort_images = 
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( distort_button ) );

		/* Get bye bye transition settings */
		img->bye_bye_transition = 
			gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( img->bye_bye_transition_checkbox) );

		/* Set the max value of slide subtitles hrange scale
		* according to the new video size */
		//gtk_adjustment_set_upper( img->sub_posX_adj, (gdouble)img->video_size[0]);
		//gtk_adjustment_set_upper( img->sub_posY_adj, (gdouble)img->video_size[1]);
	
		/* Get color settings */
		gtk_color_chooser_get_rgba( GTK_COLOR_CHOOSER( bg_button ), &new );
		img->background_color[0] = (gdouble)new.red   / 0xffff;
		img->background_color[1] = (gdouble)new.green / 0xffff;
		img->background_color[2] = (gdouble)new.blue  / 0xffff;
	}

	gtk_widget_destroy(dialog1);
}


/*
static void
img_update_thumbs( img_window_struct *img )
{
	gboolean      next;
	GtkTreeIter   iter;
	GtkListStore *store = img->thumbnail_model;
	GtkTreeModel *model = GTK_TREE_MODEL( store );

	for( next = gtk_tree_model_get_iter_first( model, &iter );
		 next;
		 next = gtk_tree_model_iter_next( model, &iter ) )
	{
		slide_struct *slide;
		GdkPixbuf    *pix;

		gtk_tree_model_get( model, &iter, 1, &slide, -1 );
		if( img_scale_image( slide->p_filename, img->video_ratio, 88, 0,
							 img->distort_images, img->background_color,
							 &pix, NULL ) )
		{
			gtk_list_store_set( store, &iter, 0, pix, -1 );
			g_object_unref( G_OBJECT( pix ) );
		}
	}
}

static void
img_update_current_slide( img_window_struct *img )
{
	if( ! img->current_slide )
		return;

	cairo_surface_destroy( img->current_image );
	img_scale_image( img->current_slide->p_filename, img->video_ratio,
					 0, img->video_size[1], img->distort_images,
					 img->background_color, NULL, &img->current_image );
	gtk_widget_queue_draw( img->image_area );
}
*/
