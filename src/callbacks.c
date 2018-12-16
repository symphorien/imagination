/*
 *  Copyright (c) 2009-2018 Giuseppe Torelli <colossus73@gmail.com>
 *  Copyright (c) 2009 Tadej Borovšak 	<tadeboro@gmail.com>
 *  Copyright (c) 2011 Robert Chéramy   <robert@cheramy.net>
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

#include "callbacks.h"
#include "export.h"
#include <math.h>
#include <sys/stat.h>

/* Internal structure, used for creating empty slide */
typedef struct _ImgEmptySlide ImgEmptySlide;
struct _ImgEmptySlide
{
	/* Values */
	gdouble c_start[3];  /* Start color */
	gdouble c_stop[3];   /* Stop color */
	gdouble pl_start[2]; /* Linear start point */
	gdouble pl_stop[2];  /* Linear stop point */
	gdouble pr_start[2]; /* Radial start point */
	gdouble pr_stop[2];  /* Radial stop point */
	gint    drag;        /* Are we draging point:
							  0 - no
							  1 - start point
							  2 - stop point */
	gint    gradient;    /* Gradient type:
						      0 - solid color
							  1 - linear
							  2 - radial
							  3 - gradient fading animation */

	/* Widgets */
	GtkWidget *color2;
	GtkWidget *preview;
	GtkWidget *radio[4];
	
	/* Cairo */
	cairo_t		*cr;
	gdouble		alpha;
	
	/* GSource */
	gint		source;
};

static void img_file_chooser_add_preview(img_window_struct *);
static void img_update_preview_file_chooser(GtkFileChooser *,img_window_struct *);
static gboolean img_transition_timeout(img_window_struct *);
static gboolean img_still_timeout(img_window_struct *);
static void img_swap_toolbar_images( img_window_struct *, gboolean);
static void img_clean_after_preview(img_window_struct *);
static void img_about_dialog_activate_link(GtkAboutDialog * , const gchar *, gpointer );
static GdkPixbuf *img_rotate_pixbuf( GdkPixbuf *, GtkProgressBar *, ImgAngle );
static void img_rotate_selected_slides( img_window_struct *, gboolean );

static void
img_image_area_change_zoom( gdouble            step,
							gboolean           reset,
							img_window_struct *img );

static void
img_overview_change_zoom( gdouble            step,
						  gboolean           reset,
						  img_window_struct *img );

static gboolean
img_fade_gradient_decrease_alpha(ImgEmptySlide *slide);

static void
img_gradient_toggled( GtkToggleButton *button,
					  ImgEmptySlide   *slide );

static void
img_gradient_color_set( GtkColorButton *button,
						ImgEmptySlide  *slide );

static gboolean
img_gradient_expose( GtkWidget      *widget,
					 GdkEventExpose *expose,
					 ImgEmptySlide  *slide );

static gboolean
img_gradient_press( GtkWidget      *widget,
					GdkEventButton *button,
					ImgEmptySlide  *slide );

static gboolean
img_gradient_release( GtkWidget      *widget,
					  GdkEventButton *button,
					  ImgEmptySlide  *slide );

static gboolean
img_gradient_move( GtkWidget      *widget,
				   GdkEventMotion *motion,
				   ImgEmptySlide  *slide );


void img_set_window_title(img_window_struct *img, gchar *text)
{
	gchar *title = NULL;

	if (text == NULL)
	{
		title = g_strconcat("Imagination ", strcmp(REVISION, "-1") == 0 ? VERSION : VERSION "-" REVISION, NULL);
		gtk_window_set_title (GTK_WINDOW (img->imagination_window), title);
		g_free(title);
	}
	else
	{
		title = g_strconcat(text, " - Imagination ", strcmp(REVISION, "-1") == 0 ? VERSION : VERSION "-" REVISION, NULL);
		gtk_window_set_title (GTK_WINDOW (img->imagination_window), title);
		g_free(title);
	}
}

void img_new_slideshow(GtkMenuItem *item,img_window_struct *img_struct)
{
    if (img_struct->project_is_modified)
        if (GTK_RESPONSE_OK != img_ask_user_confirmation(img_struct, _("You didn't save your slideshow yet. Are you sure you want to close it?")))
            return;

    img_new_slideshow_settings_dialog(img_struct, FALSE);
}

void img_project_properties(GtkMenuItem *item, img_window_struct *img_struct)
{
	img_new_slideshow_settings_dialog(img_struct, TRUE);
}

void img_add_slides_thumbnails(GtkMenuItem *item, img_window_struct *img)
{
	GSList	*slides = NULL, *bak;
	GdkPixbuf *thumb;
	GtkTreeIter iter;
	slide_struct *slide_info;
	gint slides_cnt = 0, actual_slides = 0;

	slides = img_import_slides_file_chooser(img);

	if (slides == NULL)
		return;

	actual_slides = img->slides_nr;
	img->slides_nr += g_slist_length(slides);
	gtk_widget_show(img->progress_bar);

	/* Remove model from thumbnail iconview for efficiency */
	g_object_ref( G_OBJECT( img->thumbnail_model ) );
	gtk_icon_view_set_model( GTK_ICON_VIEW( img->thumbnail_iconview ), NULL );
	gtk_icon_view_set_model( GTK_ICON_VIEW( img->over_icon ), NULL );

	bak = slides;
	while (slides)
	{
		if( img_scale_image( slides->data, img->video_ratio, 88, 0,
							 img->distort_images, img->background_color,
							 &thumb, NULL ) )
		{
			slide_info = img_create_new_slide();
			if (slide_info)
			{
				img_set_slide_file_info( slide_info, slides->data );
				gtk_list_store_append (img->thumbnail_model,&iter);
				gtk_list_store_set (img->thumbnail_model, &iter, 0, thumb,
																 1, slide_info,
																 2, NULL,
																 3, FALSE,
																 -1);
				g_object_unref (thumb);
				slides_cnt++;
			}
			g_free(slides->data);
		}
		img_increase_progressbar(img, slides_cnt);
		slides = slides->next;
	}
	gtk_widget_hide(img->progress_bar);
	g_slist_free(bak);
	img_set_total_slideshow_duration(img);
	img_set_statusbar_message(img,0);
	img->project_is_modified = TRUE;

	gtk_icon_view_set_model( GTK_ICON_VIEW( img->thumbnail_iconview ),
							 GTK_TREE_MODEL( img->thumbnail_model ) );
	gtk_icon_view_set_model( GTK_ICON_VIEW( img->over_icon ),
							 GTK_TREE_MODEL( img->thumbnail_model ) );
	g_object_unref( G_OBJECT( img->thumbnail_model ) );
	
	/* Select the first slide */
	if (actual_slides == 0)
		img_goto_first_slide(NULL, img);

	/* Select the first loaded slide if a previous set of slides was loaded */
	else
		img_select_nth_slide(img, actual_slides);
}

void img_increase_progressbar(img_window_struct *img, gint nr)
{
	gchar *message;
	gdouble percent;

	percent = (gdouble)nr / img->slides_nr;
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (img->progress_bar), percent);
	message = g_strdup_printf( _("Please wait, importing image %d out of %d"),
							   nr, img->slides_nr );
	gtk_statusbar_push(GTK_STATUSBAR(img->statusbar), img->context_id, message);
	g_free(message);

	while (gtk_events_pending())
		gtk_main_iteration();
}

void img_remove_audio_files (GtkWidget *widget, img_window_struct *img)
{
	GtkTreeSelection *sel;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *rr_list = NULL;
	GList *node;
	gchar *time;
	gint secs;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(img->music_file_treeview));
	gtk_tree_selection_selected_foreach(sel, (GtkTreeSelectionForeachFunc) img_remove_foreach_func, &rr_list);

	for (node = rr_list; node != NULL; node = node->next)
	{
		path = gtk_tree_row_reference_get_path((GtkTreeRowReference *) node->data);
		if (path)
	    {
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(img->music_file_liststore), &iter, path))
			{
                gtk_tree_model_get(GTK_TREE_MODEL(img->music_file_liststore), &iter, 3, &secs, -1);
				gtk_list_store_remove(img->music_file_liststore, &iter);
			}
			gtk_tree_path_free(path);
		}
		img->total_music_secs -= secs;
	}
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(img->music_file_liststore), &iter) == FALSE)
	{
        img_play_stop_selected_file(NULL, img);
		gtk_widget_set_sensitive (img->remove_audio_button, FALSE);
		gtk_widget_set_sensitive (img->play_audio_button, FALSE);
		gtk_label_set_text(GTK_LABEL(img->music_time_data), "");
	}
	else
	{
		time = img_convert_seconds_to_time(img->total_music_secs);
		gtk_label_set_text(GTK_LABEL(img->music_time_data), time);
		g_free(time);
	}
	g_list_foreach(rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free(rr_list);
}

void img_remove_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GList **rowref_list)
{
	GtkTreeRowReference *rowref;

	rowref = gtk_tree_row_reference_new(model, path);
	*rowref_list = g_list_append(*rowref_list, rowref);
}

void img_select_audio_files_to_add ( GtkMenuItem* button, img_window_struct *img)
{
	GtkFileFilter *audio_filter, *all_files_filter;
	GtkWidget *fs;
	GSList *files = NULL;
	gint response;
	gchar *time = NULL;

	fs = gtk_file_chooser_dialog_new( _("Import audio files, use CTRL key "
										"for multiple select"),
									  GTK_WINDOW (img->imagination_window),
									  GTK_FILE_CHOOSER_ACTION_OPEN,
									  GTK_STOCK_CANCEL,
									  GTK_RESPONSE_CANCEL,
									  GTK_STOCK_OPEN,
									  GTK_RESPONSE_ACCEPT,
									  NULL );

	/* only audio files filter */
	audio_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (audio_filter, _("All audio files") );
	gtk_file_filter_add_pattern (audio_filter, "*.wav");
	gtk_file_filter_add_pattern (audio_filter, "*.mp3");
	gtk_file_filter_add_pattern (audio_filter, "*.ogg");
	gtk_file_filter_add_pattern (audio_filter, "*.flac");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fs), audio_filter);

	/* All files filter */
	all_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name(all_files_filter, _("All files"));
	gtk_file_filter_add_pattern(all_files_filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fs), all_files_filter);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (fs), TRUE);

	response = gtk_dialog_run (GTK_DIALOG (fs));
	if (response == GTK_RESPONSE_ACCEPT)
	{
		files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (fs));
		g_slist_foreach( files, (GFunc) img_add_audio_files, img);
	}
	if (files != NULL)
	{
		g_slist_foreach(files, (GFunc) g_free, NULL);
		g_slist_free (files);
	}	

	/* Update incompatibilities display */
	img_update_inc_audio_display( img );

	time = img_convert_seconds_to_time(img->total_music_secs);
	gtk_label_set_text(GTK_LABEL(img->music_time_data), time);
	g_free(time);

	gtk_widget_destroy (fs);
}

void img_add_audio_files (gchar *filename, img_window_struct *img)
{
	GtkTreeIter iter;
	gchar *path, *file, *time;
	gint secs;

	path = g_path_get_dirname(filename);
	file = g_path_get_basename(filename);
	time = img_get_audio_length(img, filename, &secs);

	if (time != NULL)
	{
		gtk_list_store_append(img->music_file_liststore, &iter);
		gtk_list_store_set (img->music_file_liststore, &iter, 0, path, 1, file, 2, time, 3, secs, -1);
		g_free(time);
	}
	g_free(path);
	g_free(file);
}

GSList *img_import_slides_file_chooser(img_window_struct *img)
{
	GtkFileFilter *all_images_filter, *all_files_filter;
	GSList *slides = NULL;
	int response;

	img->import_slide_chooser =
		gtk_file_chooser_dialog_new( _("Import images, use SHIFT key for "
									   "multiple select"),
									 GTK_WINDOW (img->imagination_window),
									 GTK_FILE_CHOOSER_ACTION_OPEN,
									 GTK_STOCK_CANCEL,
									 GTK_RESPONSE_CANCEL,
									 GTK_STOCK_OPEN,
									 GTK_RESPONSE_ACCEPT,
									 NULL);
	img_file_chooser_add_preview(img);

	/* Image files filter */
	all_images_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name(all_images_filter,_("All image files"));
	gtk_file_filter_add_pixbuf_formats( all_images_filter );
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(img->import_slide_chooser),all_images_filter);

	/* All files filter */
	all_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name(all_files_filter,_("All files"));
	gtk_file_filter_add_pattern(all_files_filter,"*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(img->import_slide_chooser),all_files_filter);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(img->import_slide_chooser),TRUE);
	if (img->current_dir)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(img->import_slide_chooser),img->current_dir);
	response = gtk_dialog_run (GTK_DIALOG(img->import_slide_chooser));
	if (response == GTK_RESPONSE_ACCEPT)
	{
		slides = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(img->import_slide_chooser));
		if (img->current_dir)
			g_free(img->current_dir);
		img->current_dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(img->import_slide_chooser));
	}
	gtk_widget_destroy (img->import_slide_chooser);
	return slides;
}

void img_free_allocated_memory(img_window_struct *img_struct)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	slide_struct *entry;

	/* Free the memory allocated the single slides one by one */
	if (img_struct->slides_nr)
	{
		model = GTK_TREE_MODEL( img_struct->thumbnail_model );

		gtk_tree_model_get_iter_first(model,&iter);
		do
		{
			gtk_tree_model_get(model, &iter,1,&entry,-1);
			img_free_slide_struct( entry );
			img_struct->slides_nr--;
		}
		while (gtk_tree_model_iter_next (model,&iter));
		g_signal_handlers_block_by_func((gpointer)img_struct->thumbnail_iconview, (gpointer)img_iconview_selection_changed, img_struct);
		g_signal_handlers_block_by_func((gpointer)img_struct->over_icon, (gpointer)img_iconview_selection_changed, img_struct);
		gtk_list_store_clear(GTK_LIST_STORE(img_struct->thumbnail_model));
		g_signal_handlers_unblock_by_func((gpointer)img_struct->thumbnail_iconview, (gpointer)img_iconview_selection_changed, img_struct);
		g_signal_handlers_unblock_by_func((gpointer)img_struct->over_icon, (gpointer)img_iconview_selection_changed, img_struct);
	}

	/* Unlink the possible created rotated pictures and free the GSlist */
	/* NOTE: This is now done by img_free_slide_struct function */

	/* Delete the audio files in the liststore */
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(img_struct->music_file_liststore), &iter))
	{
		gtk_list_store_clear(img_struct->music_file_liststore);
		img_struct->total_music_secs = 0;
		gtk_label_set_text(GTK_LABEL(img_struct->music_time_data), "");
	}
	
	/* Free gchar pointers */
	if (img_struct->current_dir)
	{
		g_free(img_struct->current_dir);
		img_struct->current_dir = NULL;
	}
	
	if (img_struct->project_current_dir)
    {
        g_free(img_struct->project_current_dir);
        img_struct->project_current_dir = NULL;
    }

	if (img_struct->project_filename)
	{
		g_free(img_struct->project_filename);
		img_struct->project_filename = NULL;
	}
	if (img_struct->first_selected_path)
	{
		gtk_tree_path_free(img_struct->first_selected_path);
		img_struct->first_selected_path = NULL;
	}
}

gint img_ask_user_confirmation(img_window_struct *img_struct, gchar *msg)
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new(GTK_WINDOW(img_struct->imagination_window),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_OK_CANCEL, "%s.", msg);
	gtk_window_set_title(GTK_WINDOW(dialog),"Imagination");
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
	return response;
}

gboolean img_key_pressed(GtkWidget *widget, GdkEventKey *event, img_window_struct *img)
{
	if ( img->window_is_fullscreen )
	{
		if (event->keyval == GDK_KEY_F11 || event->keyval == GDK_KEY_Escape)
			img_exit_fullscreen(img);
		else if (event->keyval == GDK_space)
		{
			img->music_preview = TRUE;
			img_start_stop_preview(NULL, img);
		}
	}

	return FALSE;
}

void img_exit_fullscreen(img_window_struct *img)
{
	gtk_widget_show(img->thumb_scrolledwindow);
	gtk_widget_show(img->notebook);
	gtk_widget_show(img->statusbar);
	gtk_widget_show(img->menubar);
	gtk_widget_show(img->toolbar);
	gtk_alignment_set(GTK_ALIGNMENT(img->viewport_align), 0.5, 0.5, 0, 0);

	gtk_window_unfullscreen(GTK_WINDOW(img->imagination_window));
	gtk_widget_add_accelerator (img->fullscreen, "activate", img->accel_group, GDK_F11, GDK_MODE_DISABLED, GTK_ACCEL_VISIBLE);
	img->window_is_fullscreen = FALSE;

	/* Restore the cursor */
	GdkCursor *cursor	= gdk_cursor_new(GDK_ARROW);
	GdkWindow *win 		= gtk_widget_get_window(img->imagination_window);
	gdk_window_set_cursor(win, cursor);
}

gboolean img_quit_application(GtkWidget *widget, GdkEvent *event, img_window_struct *img_struct)
{
	gint response;

	if (img_struct->project_is_modified)
	{
		response = img_ask_user_confirmation( img_struct, _("You didn't save your slideshow yet. Are you sure you want to close it?"));
		if (response != GTK_RESPONSE_OK)
			return TRUE;
	}
	if( img_save_window_settings( img_struct ) )
		return( TRUE );
	img_free_allocated_memory(img_struct);

	/* Unloads the plugins */
	g_slist_foreach(img_struct->plugin_list,(GFunc)g_module_close,NULL);
	g_slist_free(img_struct->plugin_list);

	return FALSE;
}

static void img_file_chooser_add_preview(img_window_struct *img_struct)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER(vbox), 10);

	img_struct->preview_image = gtk_image_new ();

	img_struct->dim_label  = gtk_label_new (NULL);
	img_struct->size_label = gtk_label_new (NULL);

	gtk_box_pack_start (GTK_BOX (vbox), img_struct->preview_image, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), img_struct->dim_label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), img_struct->size_label, FALSE, TRUE, 0);
	gtk_widget_show_all (vbox);

	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER(img_struct->import_slide_chooser), vbox);
	gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER(img_struct->import_slide_chooser), FALSE);

	g_signal_connect (img_struct->import_slide_chooser, "update-preview",G_CALLBACK (img_update_preview_file_chooser), img_struct);
}

static void	img_update_preview_file_chooser(GtkFileChooser *file_chooser,img_window_struct *img_struct)
{
	gchar *filename,*size;
	gboolean has_preview = FALSE;
	gint width,height;
	GdkPixbuf *pixbuf;
	GdkPixbufFormat *pixbuf_format;

	filename = gtk_file_chooser_get_filename(file_chooser);
	if (filename == NULL)
	{
		gtk_file_chooser_set_preview_widget_active (file_chooser, has_preview);
		return;
	}
	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename, 93, 70, TRUE, NULL);
	has_preview = (pixbuf != NULL);
	if (has_preview)
	{
		pixbuf_format = gdk_pixbuf_get_file_info(filename,&width,&height);
		gtk_image_set_from_pixbuf (GTK_IMAGE(img_struct->preview_image), pixbuf);
		g_object_unref (pixbuf);

		size = g_strdup_printf(ngettext("%d x %d pixels", "%d x %d pixels", height),width,height);
		gtk_label_set_text(GTK_LABEL(img_struct->dim_label),size);
		g_free(size);
	}
	g_free(filename);
	gtk_file_chooser_set_preview_widget_active (file_chooser, has_preview);
}

void img_delete_selected_slides(GtkMenuItem *item,img_window_struct *img)
{
	GList *selected, *bak;
	GtkTreeIter iter;
	GtkTreeModel *model;
	slide_struct *entry;

	model =	GTK_TREE_MODEL( img->thumbnail_model );
	
	selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon));
	if (selected == NULL || img->preview_is_running)
		return;
	
	/* Free the slide struct for each slide and remove it from the iconview */
	bak = selected;
	g_signal_handlers_block_by_func( (gpointer)img->thumbnail_iconview,
									 (gpointer)img_iconview_selection_changed,
									 img );
	g_signal_handlers_block_by_func( (gpointer)img->over_icon,
									 (gpointer)img_iconview_selection_changed,
									 img );
	while (selected)
	{
		gtk_tree_model_get_iter(model, &iter,selected->data);
		gtk_tree_model_get(model, &iter,1,&entry,-1);
		img_free_slide_struct( entry );
		gtk_list_store_remove(GTK_LIST_STORE(img->thumbnail_model),&iter);
		img->slides_nr--;
		selected = selected->next;
	}
	g_signal_handlers_unblock_by_func( (gpointer)img->thumbnail_iconview,
									   (gpointer)img_iconview_selection_changed,
									   img );
	g_signal_handlers_unblock_by_func( (gpointer)img->over_icon,
									   (gpointer)img_iconview_selection_changed,
									   img );
	g_list_foreach (bak, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(bak);

	img_set_statusbar_message(img,0);
	cairo_surface_destroy( img->current_image );
	img->current_image = NULL;
	gtk_widget_queue_draw( img->image_area );
	img->project_is_modified = TRUE;
	img_iconview_selection_changed(GTK_ICON_VIEW(img->active_icon),img);
}

void
img_rotate_slides_left( GtkWidget         *widget,
						img_window_struct *img )
{
	img_rotate_selected_slides( img, TRUE );
}

void
img_rotate_slides_right( GtkWidget         *widget,
						 img_window_struct *img )
{
	img_rotate_selected_slides( img, FALSE );
}

static void
img_rotate_selected_slides( img_window_struct *img,
							gboolean           clockwise )
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	GList        *selected,
				 *bak;
	GdkPixbuf    *thumb;
	slide_struct *info_slide;

	/* Obtain the selected slideshow filename */
	model = GTK_TREE_MODEL( img->thumbnail_model );
	selected = gtk_icon_view_get_selected_items(
					GTK_ICON_VIEW( img->active_icon ) );

	if( selected == NULL)
		return;

	gtk_widget_show(img->progress_bar);

	bak = selected;
	while (selected)
	{
		ImgAngle angle;

		gtk_tree_model_get_iter( model, &iter, selected->data );
		gtk_tree_model_get( model, &iter, 1, &info_slide, -1 );

		/* Avoid seg-fault when dealing with text/gradient slides */
		if (info_slide->o_filename != NULL)
		{
			angle = ( info_slide->angle + ( clockwise ? 1 : -1 ) ) % 4;
			img_rotate_slide( info_slide, angle, GTK_PROGRESS_BAR( img->progress_bar ) );

			/* Display the rotated image in thumbnails iconview */
			img_scale_image( info_slide->r_filename, img->video_ratio, 88, 0,
							 img->distort_images, img->background_color,
							 &thumb, NULL );
			gtk_list_store_set( img->thumbnail_model, &iter, 0, thumb, -1 );
		}
		selected = selected->next;
	}
	gtk_widget_hide(img->progress_bar);
	g_list_foreach (bak, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(bak);

	/* If no slide is selected currently, simply return */
	if( ! img->current_slide )
		return;

	if (info_slide->o_filename != NULL)
	{
		cairo_surface_destroy( img->current_image );
		img_scale_image( img->current_slide->r_filename, img->video_ratio,
							 0, img->video_size[1], img->distort_images,
							 img->background_color, NULL, &img->current_image );
		
		gtk_widget_queue_draw( img->image_area );
	}
}

/* Rotate clockwise */
static GdkPixbuf *img_rotate_pixbuf( GdkPixbuf      *original,
									 GtkProgressBar *progress,
									 ImgAngle        angle )
{
	GdkPixbuf     *new;
	gint           w, h, r1, r2, channels, bps;
	GdkColorspace  colorspace;
	gboolean       alpha;
	guchar        *pixels1, *pixels2;
	gint           i, j;

	/* Get data from source */
	g_object_get( G_OBJECT( original ), "width", &w,
										"height", &h,
										"rowstride", &r1,
										"n_channels", &channels,
										"bits_per_sample", &bps,
										"colorspace", &colorspace,
										"has_alpha", &alpha,
										"pixels", &pixels1,
										NULL );

	switch( angle )
	{
		case ANGLE_0:
			g_object_ref( G_OBJECT( original ) );
			new = original;
			break;

		case ANGLE_90:
			/* Create new rotated image */
			new = gdk_pixbuf_new( colorspace, alpha, bps, h, w );
			g_object_get( G_OBJECT( new ), "rowstride", &r2,
										   "pixels", &pixels2,
										   NULL );
			
			/* Copy data, applying transormation along the way */
			for( j = 0; j < h; j++ )
			{
				for( i = 0; i < w; i++ )
				{
					int source = i * channels + r1 * j;
					int dest   = j * channels + r2 * ( w - i - 1 );
					int n;
					
					for( n = 0; n < channels; n++ )
						pixels2[dest + n] = pixels1[source + n];
				}
				
				if( j % 100 )
					continue;
				
				/* Update progress bar */
				gtk_progress_bar_set_fraction( progress, (gdouble)( j + 1 ) / h );
				while( gtk_events_pending() )
					gtk_main_iteration();
			}
			break;

		case ANGLE_180:
			/* Create new rotated image */
			new = gdk_pixbuf_new( colorspace, alpha, bps, w, h );
			g_object_get( G_OBJECT( new ), "rowstride", &r2,
										   "pixels", &pixels2,
										   NULL );

			/* Copy data, applying transormation along the way */
			for( j = 0; j < h; j++ )
			{
				for( i = 0; i < w; i++ )
				{
					int source = i * channels + r1 * j;
					int dest   = ( w - i - 1 ) * channels + r2 * ( h - j - 1 );
					int n;

					for( n = 0; n < channels; n++ )
						pixels2[dest + n] = pixels1[source + n];
				}

				if( j % 100 )
					continue;

				/* Update progress bar */
				gtk_progress_bar_set_fraction( progress, (gdouble)( j + 1 ) / h );
				while( gtk_events_pending() )
					gtk_main_iteration();
			}
			break;

		case ANGLE_270:
			/* Create new rotated image */
			new = gdk_pixbuf_new( colorspace, alpha, bps, h, w );
			g_object_get( G_OBJECT( new ), "rowstride", &r2,
										   "pixels", &pixels2,
										   NULL );

			/* Copy data, applying transormation along the way */
			for( j = 0; j < h; j++ )
			{
				for( i = 0; i < w; i++ )
				{
					int source = i * channels + r1 * j;
					int dest   = ( h - j - 1 ) * channels + r2 * i;
					int n;

					for( n = 0; n < channels; n++ )
						pixels2[dest + n] = pixels1[source + n];
				}

				if( j % 100 )
					continue;

				/* Update progress bar */
				gtk_progress_bar_set_fraction( progress, (gdouble)( j + 1 ) / h );
				while( gtk_events_pending() )
					gtk_main_iteration();
			}
			break;
	}

	return( new );
}

void img_show_about_dialog (GtkMenuItem *item,img_window_struct *img_struct)
{
	static GtkWidget *about = NULL;
	static gchar version[] = VERSION "-" REVISION;
    const char *authors[] = {"\nDevelopers:\nGiuseppe Torelli <colossus73@gmail.com>\nTadej Borovšak <tadeboro@gmail.com>\n",NULL};
    
	if (about == NULL)
	{
		about = gtk_about_dialog_new ();
		gtk_about_dialog_set_url_hook(img_about_dialog_activate_link, NULL, NULL);
		gtk_window_set_position (GTK_WINDOW (about),GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_transient_for (GTK_WINDOW (about),GTK_WINDOW (img_struct->imagination_window));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (about),TRUE);
		g_object_set (about,
			"name", "Imagination",
			"version", strcmp(REVISION, "-1") == 0 ? VERSION : version,
			"copyright","Copyright \xC2\xA9 2009-2018 Giuseppe Torelli",
			"comments","A simple and lightweight slideshow maker",
			"authors",authors,
			"documenters",NULL,
			"translator_credits",_("translator-credits"),
			"logo_icon_name","imagination",
			"website","http://imagination.sf.net",
			"license","Copyright \xC2\xA9 2009-2018 Giuseppe Torelli - Colossus <colossus73@gmail.com>\n\n"
		    			"This is free software; you can redistribute it and/or\n"
    					"modify it under the terms of the GNU Library General Public License as\n"
    					"published by the Free Software Foundation; either version 2 of the\n"
    					"License,or (at your option) any later version.\n"
    					"\n"
    					"This software is distributed in the hope that it will be useful,\n"
    					"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    					"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
    					"Library General Public License for more details.\n"
    					"\n"
    					"You should have received a copy of the GNU Library General Public\n"
    					"License along with the Gnome Library; see the file COPYING.LIB.  If not,\n"
    					"write to the Free Software Foundation,Inc.,59 Temple Place - Suite 330,\n"
    					"Boston,MA 02111-1307,USA.\n",
		      NULL);
	}
	gtk_dialog_run ( GTK_DIALOG(about));
	gtk_widget_hide (about);
}

static void img_about_dialog_activate_link(GtkAboutDialog * dialog, const gchar *link, gpointer data)
{
	/* Replace xdg-open with GTK+ equivalent */
	gtk_show_uri( NULL, link, GDK_CURRENT_TIME, NULL );
}

void img_go_fullscreen(GtkMenuItem *item, img_window_struct *img)
{
	/* Hide the cursor */
	GdkCursor *cursor	= gdk_cursor_new(GDK_BLANK_CURSOR);
	GdkWindow *win 		= gtk_widget_get_window(img->imagination_window);
	gdk_window_set_cursor(win, cursor);

	gtk_widget_remove_accelerator (img->fullscreen, img->accel_group, GDK_F11, GDK_MODE_DISABLED);

	gtk_widget_hide (img->thumb_scrolledwindow);
	gtk_widget_hide (img->notebook);
	gtk_widget_hide (img->statusbar);
	gtk_widget_hide (img->menubar);
	gtk_widget_hide (img->toolbar);
	
	gtk_alignment_set(GTK_ALIGNMENT(img->viewport_align), 0, 0, 1, 1);
	gtk_window_fullscreen(GTK_WINDOW(img->imagination_window));
	img->window_is_fullscreen =TRUE;

	gtk_widget_queue_draw(img->image_area);
}

void img_start_stop_preview(GtkWidget *item, img_window_struct *img)
{
	GtkTreeIter iter, prev;
	GtkTreePath *path = NULL;
	slide_struct *entry;
	GtkTreeModel *model;
	GList *list = NULL;

	/* If no images are present, abort */
	if( img->slides_nr == 0 )
		return;

	if(img->export_is_running)
		return;

	/* Save the current selected slide to restore it when the preview is finished */
	if (img->preview_is_running == FALSE)
	{
		gtk_tree_path_free(img->first_selected_path);
		gtk_icon_view_get_cursor(GTK_ICON_VIEW( img->thumbnail_iconview ), &img->first_selected_path, NULL);
	}
	if (img->preview_is_running)
	{
		img->loop_preview = img->music_preview = img->no_music = FALSE;
		/* If there is a background music during
		* the preview we want to stop it dont'we? */
		if (img->fullscreen_music_preview && img->play_child_pid)
		{
			kill (img->play_child_pid, SIGINT);
			/* Let's stop the next scheduled
			* audio file to be played too */
			if (img->audio_source_id > 0)
			{
				g_source_remove(img->audio_source_id);
				img->audio_source_id = 0;
			}
			img->play_child_pid = 0;
		}
		/* Remove timeout function from main loop */
		g_source_remove(img->source_id);

		/* Clean resources used by preview and prepare application for
		 * next preview. */
		img_clean_after_preview(img);

		/* Unselect the last selected item during the preview */
		GList *list;
		list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->thumbnail_iconview));

		if (list)
		{
			gtk_icon_view_unselect_path (GTK_ICON_VIEW(img->thumbnail_iconview), (GtkTreePath*)list->data);
			g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
			g_list_free (list);
		}

		/*  Reselect the first selected slide before the preview if any */
		if (img->first_selected_path)
		{
			gtk_icon_view_select_path (GTK_ICON_VIEW(img->thumbnail_iconview), img->first_selected_path);
			gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(img->thumbnail_iconview), img->first_selected_path, FALSE, 0.0, 0.0);
		}
	}
	else
	{
		if (GTK_WIDGET(item) == img->fullscreen_music_preview)
			img->music_preview = TRUE;

		else if (GTK_WIDGET(item) == img->fullscreen_loop_preview)
			img->loop_preview = TRUE;
		else
			img->no_music = TRUE;

		/* Start the preview */
		if( img->mode == 1 )
		{
			img->auto_switch = TRUE;
			img_switch_mode( img, 0 );
		}

		model = GTK_TREE_MODEL( img->thumbnail_model );
		list = gtk_icon_view_get_selected_items(
					GTK_ICON_VIEW( img->thumbnail_iconview ) );
		if( list )
			gtk_icon_view_get_cursor( GTK_ICON_VIEW(img->thumbnail_iconview),
									  &path, NULL);
		if( list )
		{
			/* Start preview from this slide */
			if( path )
				gtk_tree_model_get_iter( model, &iter, path );
			g_list_foreach( list, (GFunc)gtk_tree_path_free, NULL );
			g_list_free( list );
		}
		else
		{
			/* Start preview from the beginning */
			if( ! gtk_tree_model_get_iter_first( model, &iter ) )
				return;
		}
		img->cur_ss_iter = iter;

		/* Replace button and menu images */
		img_swap_toolbar_images( img, FALSE );

		/* Load the first image in the pixbuf */
		gtk_tree_model_get( model, &iter, 1, &entry, -1);

		if( ! entry->o_filename )
			img_scale_gradient( entry->gradient, entry->g_start_point,
								entry->g_stop_point, entry->g_start_color,
								entry->g_stop_color, img->video_size[0],
								img->video_size[1], NULL, &img->image2 );
		else
			img_scale_image( entry->r_filename, img->video_ratio,
								0, img->video_size[1], img->distort_images,
								img->background_color, NULL, &img->image2 );
	
		/* Load first stop point */
		img->point2 = (ImgStopPoint *)( entry->no_points ?
										entry->points->data :
										NULL );

		img->work_slide = entry;
		img->cur_point = NULL;

		/* If we started our preview from beginning, create empty pixbuf and
		 * fill it with background color otherwise load image that is before
		 * currently selected slide. */
		if( path != NULL && gtk_tree_path_prev( path ) )
		{
			gtk_tree_model_get_iter( model, &prev, path );
			gtk_tree_model_get( model, &prev, 1, &entry, -1 );
			
			if (img->music_preview)
				img_preview_with_music(img, 255);

			if( ! entry->o_filename )
			{
				img_scale_gradient( entry->gradient, entry->g_start_point,
									entry->g_stop_point, entry->g_start_color,
									entry->g_stop_color, img->video_size[0],
									img->video_size[1], NULL, &img->image1 );
			}
			else
				img_scale_image( entry->r_filename, img->video_ratio,
									0, img->video_size[1], img->distort_images,
									img->background_color, NULL, &img->image1 );
			
			/* Load last stop point */
			img->point1 = (ImgStopPoint *)( entry->no_points ?
											g_list_last( entry->points )->data :
											NULL );
		}
		else
		{
			cairo_t *cr;

			if (img->music_preview)
				img_preview_with_music(img, 0);

			img->image1 = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
													  img->video_size[0],
													  img->video_size[1] );
			cr = cairo_create( img->image1 );
			cairo_set_source_rgb( cr, img->background_color[0],
									  img->background_color[1],
									  img->background_color[2] );
			cairo_paint( cr );
			cairo_destroy( cr );
		}
		if( path )
			gtk_tree_path_free( path );

		/* Add transition timeout function */
		img->preview_is_running = TRUE;
		img->total_nr_frames = img->total_secs * img->preview_fps;
		img->displayed_frame = 0;
		img->next_slide_off = 0;
		img_calc_next_slide_time_offset( img, img->preview_fps );

		/* Create surfaces to be passed to transition renderer */
		img->image_from = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
													  img->video_size[0],
													  img->video_size[1] );
		img->image_to = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
													img->video_size[0],
													img->video_size[1] );
		if (entry->gradient == 3)
		{
			cairo_t	*cr;
			cr = cairo_create(img->image_from);
			cairo_set_source_rgb(cr,	entry->g_start_color[0],
										entry->g_start_color[1],
										entry->g_start_color[2] );
			cairo_paint( cr );
			
			cr = cairo_create(img->image_to);
			cairo_set_source_rgb(cr,	entry->g_stop_color[0],
										entry->g_stop_color[1],
										entry->g_stop_color[2] );
			cairo_paint( cr );
		}													
		img->exported_image = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
														  img->video_size[0],
														  img->video_size[1] );
//img_run_encoder(img);
		img->source_id = g_timeout_add( 1000 / img->preview_fps,
										(GSourceFunc)img_transition_timeout,
										img );
	}
	return;
}

void img_goto_first_slide(GtkWidget *button, img_window_struct *img)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	gchar *slide = NULL;

	model = GTK_TREE_MODEL( img->thumbnail_model );
	if ( ! gtk_tree_model_get_iter_first(model,&iter))
		return;

	slide = g_strdup_printf("%d", 1);
	gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry), slide);
	g_free(slide);
	gtk_icon_view_unselect_all(GTK_ICON_VIEW (img->active_icon));
	path = gtk_tree_path_new_from_indices(0,-1);
	gtk_icon_view_set_cursor (GTK_ICON_VIEW (img->active_icon), path, NULL, FALSE);
	gtk_icon_view_select_path (GTK_ICON_VIEW (img->active_icon), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (img->active_icon), path, FALSE, 0, 0);
	gtk_tree_path_free (path);
}

void img_goto_prev_slide(GtkWidget *button, img_window_struct *img)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GList *icons_selected = NULL;
	gchar *slide = NULL;
	gint slide_nr;

	icons_selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon) );
	if( ! icons_selected )
		return;

	model = GTK_TREE_MODEL( img->thumbnail_model );
	slide_nr = gtk_tree_path_get_indices(icons_selected->data)[0];

	if (slide_nr == 0)
		return;

	slide = g_strdup_printf("%d", slide_nr);
	gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry), slide);
	g_free(slide);
	gtk_icon_view_unselect_all(GTK_ICON_VIEW (img->active_icon));
	path = gtk_tree_path_new_from_indices(--slide_nr,-1);

	gtk_icon_view_set_cursor (GTK_ICON_VIEW (img->active_icon), path, NULL, FALSE);
	gtk_icon_view_select_path (GTK_ICON_VIEW (img->active_icon), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (img->active_icon), path, FALSE, 0, 0);
	gtk_tree_path_free (path);

	g_list_foreach (icons_selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (icons_selected);
}

void img_goto_next_slide(GtkWidget *button, img_window_struct *img)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GList *icons_selected = NULL;
	gchar *slide = NULL;
	gint slide_nr;

	icons_selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon) );
	if( ! icons_selected )
		return;

	/* Now get previous iter :) */
	model = GTK_TREE_MODEL( img->thumbnail_model );
	slide_nr = gtk_tree_path_get_indices(icons_selected->data)[0];

	if (slide_nr == (img->slides_nr-1) )
		return;

	gtk_icon_view_unselect_all(GTK_ICON_VIEW (img->active_icon));
	path = gtk_tree_path_new_from_indices(++slide_nr, -1);

	slide = g_strdup_printf("%d", slide_nr + 1);
	gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry), slide);
	g_free(slide);
	gtk_icon_view_set_cursor (GTK_ICON_VIEW (img->active_icon), path, NULL, FALSE);
	gtk_icon_view_select_path (GTK_ICON_VIEW (img->active_icon), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (img->active_icon), path, FALSE, 0, 0);
	gtk_tree_path_free (path);

	g_list_foreach (icons_selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (icons_selected);
}


void img_goto_last_slide(GtkWidget *button, img_window_struct *img)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	gchar *slide = NULL;

	model = GTK_TREE_MODEL( img->thumbnail_model );
	if ( ! gtk_tree_model_get_iter_first(model,&iter))
		return;

	slide = g_strdup_printf("%d", img->slides_nr);
	gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry), slide);
	g_free(slide);
	gtk_icon_view_unselect_all(GTK_ICON_VIEW (img->active_icon));
	path = gtk_tree_path_new_from_indices(img->slides_nr - 1, -1);
	gtk_icon_view_set_cursor (GTK_ICON_VIEW (img->active_icon), path, NULL, FALSE);
	gtk_icon_view_select_path (GTK_ICON_VIEW (img->active_icon), path);
	gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (img->active_icon), path, FALSE, 0, 0);
	gtk_tree_path_free (path);
}

void img_on_drag_data_received (GtkWidget *widget,GdkDragContext *context,int x,int y,GtkSelectionData *data,unsigned int info,unsigned int time, img_window_struct *img)
{
	gchar **pictures = NULL;
	gchar *filename;
	GtkWidget *dialog;
	GdkPixbuf *thumb;
	GtkTreeIter iter;
	gint len = 0, slides_cnt = 0, actual_slides;
	slide_struct *slide_info;

	pictures = gtk_selection_data_get_uris(data);
	if (pictures == NULL)
	{
		dialog = gtk_message_dialog_new(GTK_WINDOW(img->imagination_window),GTK_DIALOG_MODAL,GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,_("Sorry, I could not perform the operation!"));
		gtk_window_set_title(GTK_WINDOW(dialog),"Imagination");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (GTK_WIDGET (dialog));
		gtk_drag_finish(context,FALSE,FALSE,time);
		return;
	}
	actual_slides = img->slides_nr;
	gtk_drag_finish (context,TRUE,FALSE,time);
	while(pictures[len])
	{
		filename = g_filename_from_uri (pictures[len],NULL,NULL);
		if( img_scale_image( filename, img->video_ratio, 88, 0,
							 img->distort_images, img->background_color,
							 &thumb, NULL ) )
		{
			slide_info = img_create_new_slide();
			if (slide_info)
			{
				img_set_slide_file_info( slide_info, filename );
				gtk_list_store_append (img->thumbnail_model,&iter);
				gtk_list_store_set (img->thumbnail_model, &iter, 0, thumb, 1, slide_info, -1);
				g_object_unref (thumb);
				slides_cnt++;
			}
		}
		g_free(filename);
		len++;
	}
	if (slides_cnt > 0)
	{
		img->slides_nr += slides_cnt;
		img->project_is_modified = TRUE;
		img_set_total_slideshow_duration(img);
		img_set_statusbar_message(img, 0);
	}
	g_strfreev (pictures);

	/* Select the first slide */
	if (actual_slides == 0)
		img_goto_first_slide(NULL, img);

	/* Select the first loaded slide if a previous set of slides was loaded */
	else
		img_select_nth_slide(img, actual_slides);	
}

/*
 * img_on_expose_event:
 * @widget: preview GtkDrawingArea
 * @event: expose event info
 * @img: global img_window_struct structure
 *
 * This function is responsible for all of the drawing on preview area, thus it
 * should handle "edit mode" (when user is constructing slide show), "preview
 * mode" (when user is previewing his work) and "export mode" (when export is in
 * progress).
 *
 * This might be seen as an overkill for single function, but since all of the
 * actual rendering is done by helper functions, this function just merely
 * paints the results on screen.
 *
 * Return value: This function returns TRUE if the area has been painted, FALSE
 * otherwise (this way the default expose function is only called when no slide
 * is selected).
 */
gboolean
img_on_expose_event( GtkWidget         *widget,
					 GdkEventExpose    *event,
					 img_window_struct *img )
{
	cairo_t *cr;

	/* If we're previewing or exporting, only paint frame that is being
	 * currently produced. */
	if( img->preview_is_running || img->export_is_running > 2)
	{
		gdouble factor;

		cr = gdk_cairo_create( widget->window );
		
		/* Do the drawing */
		factor = (gdouble)img->image_area->allocation.width /
						  img->video_size[0];
		cairo_scale( cr, factor, factor );
		cairo_set_source_surface( cr, img->exported_image, 0, 0 );
		cairo_paint( cr );

		cairo_destroy( cr );
	}
	else
	{
		if( ! img->current_image )
			/* Use default handler */
			return( FALSE );

		cr = gdk_cairo_create( widget->window );
		
		/* Do the drawing */
		img_draw_image_on_surface( cr, img->image_area->allocation.width, img->image_area->allocation.height,
								   img->current_image, &img->current_point, img );

		/* Render subtitle if present */
		if( img->current_slide->subtitle )
			img_render_subtitle( img,
								 cr,
								 img->video_size[0],
								 img->video_size[1],
								 img->image_area_zoom,
								 img->current_slide->posX,
								 img->current_slide->posY,
								 img->current_slide->subtitle_angle,
								 img->current_point.zoom,
								 img->current_point.offx,
								 img->current_point.offy,
								 img->current_slide->subtitle,
								 img->current_slide->pattern_filename,
								 img->current_slide->font_desc,
								 img->current_slide->font_color,
                                 img->current_slide->font_brdr_color,
                                 img->current_slide->font_bg_color,
                                 img->current_slide->border_color,
                                 img->current_slide->top_border,
                                 img->current_slide->bottom_border,
                                 img->current_slide->border_width,
								 img->current_slide->anim,
								 FALSE,
								 FALSE,
								 1.0 );

		cairo_destroy( cr );
	}

	return( TRUE );
}

/*
 * img_draw_image_on_surface:
 * @cr: cairo context
 * @width: width of the surface that @cr draws on
 * @surface: cairo surface to be drawn on @cr
 * @point: stop point holding zoom and offsets
 * @img: global img_window_struct
 *
 * This function takes care of scaling and moving of @surface to fit properly on
 * cairo context passed in.
 */
void
img_draw_image_on_surface( cairo_t           *cr,
						   gint               width,
						   gint               height,
						   cairo_surface_t   *surface,
						   ImgStopPoint      *point,
						   img_window_struct *img )
{
	gdouble  offxr, offyr;  /* Relative offsets */
	gdouble  factor_c_x;    /* X Scaling factor for cairo context */
	gdouble  factor_c_y;    /* Y Scaling factor for cairo context */
	gdouble  factor_o_x;    /* X Scaling factor for offset mods */
	gdouble  factor_o_y;    /* Y Scaling factor for offset mods */
	gint     cw;            /* Width of the surface */
	gint     ch;            /* Height of the surface */

	cw = cairo_image_surface_get_width( surface );
	ch = cairo_image_surface_get_height( surface );

	factor_c_x = (gdouble)width / cw * point->zoom;
	factor_c_y = (gdouble)height / ch * point->zoom;
	factor_o_x = (gdouble)img->video_size[0] / cw * point->zoom;
	factor_o_y = (gdouble)img->video_size[1] / ch * point->zoom;
	
	offxr = point->offx / factor_o_x;
	offyr = point->offy / factor_o_y;

	/* Make sure that matrix modifications are only visible from this function
	 * and they don't interfere with text drawing. */
	cairo_save( cr );
	cairo_scale( cr, factor_c_x, factor_c_y );
	cairo_set_source_surface( cr, surface, offxr, offyr );
	cairo_paint( cr );
	cairo_restore( cr );
}

static gboolean img_transition_timeout(img_window_struct *img)
{
	/* If we output all transition slides (or if there is no slides to output in
	 * transition part), connect still preview phase. */
	if( img->slide_cur_frame == img->slide_trans_frames )
	{
		img->source_id = g_timeout_add( 1000 / img->preview_fps,
										(GSourceFunc)img_still_timeout, img );

		return FALSE;
	}

	/* Render single frame */
	if (img->work_slide->gradient == 3)
	{
		img->gradient_slide = TRUE;
		memcpy(img->g_stop_color, img->work_slide->g_stop_color,  3 * sizeof(gdouble));
	}
	img_render_transition_frame( img );

	/* Schedule our image redraw */
	gtk_widget_queue_draw( img->image_area );

	/* Increment counters */
	img->slide_cur_frame++;
	img->displayed_frame++;

	return TRUE;
}

static gboolean img_still_timeout(img_window_struct *img)
{
	/* If there is next slide, connect transition preview, else finish
	 * preview. */
	if( img->slide_cur_frame == img->slide_nr_frames )
	{
		if( img_prepare_pixbufs( img) )
		{
			img_calc_next_slide_time_offset( img, img->preview_fps );
			img->source_id = g_timeout_add( 1000 / img->preview_fps,
										    (GSourceFunc)img_transition_timeout,
											img );
		
		}
		else
		{
			/* Clean resources used in preview and prepare application for
			 * next preview. */
			img_clean_after_preview( img );

			/* If user chose continous preview start it again */
			if (img->loop_preview)
				img_start_stop_preview(NULL, img);
		}

		/* Indicate that we must start fresh with new slide */
		img->cur_point = NULL;

		return FALSE;
	}

	/* This is a dirty hack to prevent Imagination
	keep painting the source image with the second
	* color set in the empty slide fade gradient */
	if (strcmp(gtk_entry_get_text(GTK_ENTRY(img->slide_number_entry)) , "2") == 0)
		img->gradient_slide = FALSE;

	/* Render frame */
	img_render_still_frame( img, img->preview_fps );

	/* Increment counters */
	img->still_counter++;
	img->slide_cur_frame++;
	img->displayed_frame++;

	/* Redraw */
	gtk_widget_queue_draw( img->image_area );

	return( TRUE );
}

static void img_swap_toolbar_images( img_window_struct *img,gboolean flag )
{
	GtkWidget *tmp_image;

	if( flag )
	{
		tmp_image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY,GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_widget_show(tmp_image);
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(img->preview_button), tmp_image);
		gtk_widget_set_tooltip_text(img->preview_button,_("Starts the preview without music"));
	}
	else
	{
		tmp_image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_STOP,GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_widget_show(tmp_image);
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(img->preview_button), tmp_image);
		gtk_widget_set_tooltip_text(img->preview_button,_("Stops the preview"));
	}
}

static void img_clean_after_preview(img_window_struct *img)
{
	/* Switch to right mode */
	if( img->auto_switch )
	{
		img_switch_mode( img, 1 );
		img->auto_switch = FALSE;
	}

	/* Stop the music if the user chose preview with music */
	if (img->fullscreen_music_preview && img->play_child_pid)
		kill (img->play_child_pid, SIGINT);

	/* Swap toolbar and menu icons */
	img_swap_toolbar_images( img, TRUE );

	/* Indicate that preview is not running */
	img->preview_is_running = FALSE;
	
	/* Destroy images that were used */
	cairo_surface_destroy( img->image1 );
	cairo_surface_destroy( img->image2 );
	cairo_surface_destroy( img->image_from );
	cairo_surface_destroy( img->image_to );
	cairo_surface_destroy( img->exported_image );

	gtk_widget_queue_draw( img->image_area );

	return;
}

void img_choose_slideshow_filename(GtkWidget *widget, img_window_struct *img)
{
	GtkWidget *fc;
	GtkFileChooserAction action = 0;
	gint	 response;
	gchar *filename = NULL;
    GtkFileFilter *project_filter, *all_files_filter;

	/* Determine the mode of the chooser. */
	if (widget == img->open_menu || widget == img->open_button || widget == img->import_project_menu)
		action = GTK_FILE_CHOOSER_ACTION_OPEN;
	else if (widget == img->save_as_menu || widget == img->save_menu || widget == img->save_button)
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
    
    /* close old slideshow if we import */
    if (widget == img->open_menu || widget == img->open_button)
    {
        if (img->project_is_modified)
            if (GTK_RESPONSE_OK != img_ask_user_confirmation(img, _("You didn't save your slideshow yet. Are you sure you want to close it?")))
                return;
    }

	/* If user wants to save empty slideshow, simply abort */
	if( img->slides_nr == 0 && action == GTK_FILE_CHOOSER_ACTION_SAVE  )
		return;

	if (img->project_filename == NULL || widget == img->save_as_menu || action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		fc = gtk_file_chooser_dialog_new (action == GTK_FILE_CHOOSER_ACTION_OPEN ? _("Load an Imagination slideshow project") : 
					_("Save an Imagination slideshow project"),
					GTK_WINDOW (img->imagination_window),
					action,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					action == GTK_FILE_CHOOSER_ACTION_OPEN ?  GTK_STOCK_OPEN : GTK_STOCK_SAVE,
					GTK_RESPONSE_ACCEPT,NULL);

        /* Filter .img files */
        project_filter = gtk_file_filter_new ();
        gtk_file_filter_set_name(project_filter, _("Imagination projects (*.img)"));
        gtk_file_filter_add_pattern(project_filter, "*.img");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), project_filter);

        /* All files filter */
        all_files_filter = gtk_file_filter_new ();
        gtk_file_filter_set_name(all_files_filter, _("All files"));
        gtk_file_filter_add_pattern(all_files_filter, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), all_files_filter);

        /* if we are saving, propose a default filename */
        if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
        {
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER (fc), "unknown.img");
			/* Add the checkbok to save filenames only */
			GtkWidget *box = gtk_hbox_new (TRUE, 0);
			GtkWidget *relative_names = gtk_check_button_new_with_mnemonic(_("Don't include folder names in the filenames"));
			gtk_widget_set_tooltip_text(relative_names, _("If you check this button, be sure to include ALL the project files in the SAME folder before attempting to open it again."));
			gtk_box_pack_start (GTK_BOX (box), relative_names, FALSE, FALSE, 0);
			gtk_widget_show(relative_names);
			g_signal_connect (relative_names, "toggled", G_CALLBACK (img_save_relative_filenames), img);
			gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(fc), box );
		}
		if (img->project_current_dir)
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc),img->project_current_dir);

		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (fc),TRUE);
		response = gtk_dialog_run (GTK_DIALOG (fc));
		if (response == GTK_RESPONSE_ACCEPT)
		{
			filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
			if( ! filename )
			{
				gtk_widget_destroy(fc);
				return;
			}
			/*if (img->project_current_dir)
                g_free(img->project_current_dir);
            img->project_current_dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(fc));*/

		}
		else if (response == GTK_RESPONSE_CANCEL || GTK_RESPONSE_DELETE_EVENT)
		{
			gtk_widget_destroy(fc);
			return;
		}
		gtk_widget_destroy(fc);
	}

	if( ! filename )
		filename = g_strdup( img->project_filename );

	if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		/* Close the current slideshow before opening a new one */
		img_close_slideshow(widget, img);
		img_load_slideshow( img, filename );
	}
	else
		img_save_slideshow( img, filename, img->relative_filenames );

	g_free( filename );
}

void img_close_slideshow(GtkWidget *widget, img_window_struct *img)
{
    /* When called from close_menu, ask for confirmation */
    if (img->project_is_modified && widget == img->close_menu)
    {
        if (GTK_RESPONSE_OK != img_ask_user_confirmation(img, _("You didn't save your slideshow yet. Are you sure you want to close it?")))
            return;
    }
	img->project_is_modified = FALSE;
	img_free_allocated_memory(img);
	img_set_window_title(img,NULL);
	img_set_statusbar_message(img,0);
	if( img->current_image )
		cairo_surface_destroy( img->current_image );
	img->current_image = NULL;
	gtk_widget_queue_draw( img->image_area );
	gtk_label_set_text(GTK_LABEL (img->total_time_data),"");

	/* Reset slideshow properties */
	img->distort_images = TRUE;
	img->background_color[0] = 0;
	img->background_color[1] = 0;
	img->background_color[2] = 0;
	img->final_transition.speed = NORMAL;
	img->final_transition.render = NULL;

	/* Disable the video tab */
    img_disable_videotab (img);

    gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry), "");
}

void img_move_audio_up( GtkButton *button, img_window_struct *img )
{
	GtkTreeSelection *sel;
	GtkTreeModel     *model;
	GtkTreeIter       iter1, iter2;

	/* We need path, since there is no gtk_tree_model_iter_prev function!! */
	GtkTreePath      *path;

	/* First we need to get selected iter. This function won't work if
	 * selection's mode is set to GTK_SELECTION_MULTIPLE!! */
	sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( img->music_file_treeview ) );
	if( ! gtk_tree_selection_get_selected( sel, &model, &iter1 ) )
		return;

	/* Now get previous iter and swap two items if previous iter exists. */
	path = gtk_tree_model_get_path( model, &iter1 );
	if( gtk_tree_path_prev( path ) )
	{
		gtk_tree_model_get_iter( model, &iter2, path );
		gtk_list_store_swap( GTK_LIST_STORE( model ), &iter1, &iter2 );
	}
	gtk_tree_path_free( path );
}

void img_move_audio_down( GtkButton *button, img_window_struct *img )
{
	GtkTreeSelection *sel;
	GtkTreeModel     *model;
	GtkTreeIter       iter1, iter2;

	/* First we need to get selected iter. This function won't work if
	 * selection's mode is set to GTK_SELECTION_MULTIPLE!! */
	sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( img->music_file_treeview ) );
	if( ! gtk_tree_selection_get_selected( sel, &model, &iter1 ) )
		return;

	/* Get next iter and swap rows if iter exists. */
	iter2 = iter1;
	if( gtk_tree_model_iter_next( model, &iter2 ) )
		gtk_list_store_swap( GTK_LIST_STORE( model ), &iter1, &iter2 );
}

/*
 * img_ken_burns_zoom_changed:
 * @range: GtkRange that will provide proper value for us
 * @img: global img_window_strct structure
 *
 * This function modifies current zoom value and queues redraw of preview area.
 *
 * To keep image center in focus, we also do some calculation on offsets.
 *
 * IMPORTANT: This function is part of the Ken Burns effect and doesn't change
 * preview area size!! If you're looking for function that does that,
 * img_image_area_change_zoom is the thing to look at.
 */
void
img_ken_burns_zoom_changed( GtkRange          *range,
							img_window_struct *img )
{
	/* Store old zoom for calcutaions */
	gdouble old_zoom = img->current_point.zoom;

	img->current_point.zoom = gtk_range_get_value( range );

	/* If zoom is 1, reset parameters to avoid drift. */
	if( img->current_point.zoom < 1.00005 )
	{
		img->maxoffx = 0;
		img->maxoffy = 0;
		img->current_point.offx = 0;
		img->current_point.offy = 0;
	}
	else
	{
		gdouble fracx, fracy;
		gint    tmpoffx, tmpoffy;
		gdouble aw  = img->video_size[0];
		gdouble ah  = img->video_size[1];
		gdouble aw2 = aw / 2;
		gdouble ah2 = ah / 2;

		fracx = (gdouble)( aw2 - img->current_point.offx ) / ( aw2 * old_zoom );
		fracy = (gdouble)( ah2 - img->current_point.offy ) / ( ah2 * old_zoom );
		img->maxoffx = aw * ( 1 - img->current_point.zoom );
		img->maxoffy = ah * ( 1 - img->current_point.zoom );
		tmpoffx = aw2 * ( 1 - fracx * img->current_point.zoom );
		tmpoffy = ah2 * ( 1 - fracy * img->current_point.zoom );

		img->current_point.offx = CLAMP( tmpoffx, img->maxoffx, 0 );
		img->current_point.offy = CLAMP( tmpoffy, img->maxoffy, 0 );
	}

	gtk_widget_queue_draw( img->image_area );
}

/*
 * img_image_area_button_press:
 * @widget: image area
 * @event: event description
 * @img: global img_window_struct structure
 *
 * This function stores initial coordinates of button press that we'll be
 * needing for drag emulation.
 *
 * Return value: TRUE, indicating that we handled this event.
 */
gboolean
img_image_area_button_press( GtkWidget         *widget,
							 GdkEventButton    *event,
							 img_window_struct *img )
{
	if( event->button != 1 )
		return( FALSE );

	img->x = event->x;
	img->y = event->y;
	img->bak_offx = img->current_point.offx;
	img->bak_offy = img->current_point.offy;

	return( TRUE );
}

gboolean
img_image_area_button_release(	GtkWidget			*widget,
								GdkEventButton		*event,
								img_window_struct 	*img)
{
	img_update_stop_point(NULL, img);
	return (TRUE);
}

/*
 * img_image_area_motion:
 * @widget: image area
 * @event: event description
 * @img: global img_window_struct structure
 *
 * This function calculates offsets from stored button press coordinates and
 * queue redraw of the preview area.
 *
 * Return value: TRUE if mouse button 1 has been pressed during drag, else
 * FALSE.
 */
gboolean
img_image_area_motion( GtkWidget         *widget,
					   GdkEventMotion    *event,
					   img_window_struct *img )
{
	gdouble deltax,
			deltay;

	deltax = ( event->x - img->x ) / img->image_area_zoom;
	deltay = ( event->y - img->y ) / img->image_area_zoom;

	img->current_point.offx = CLAMP( deltax + img->bak_offx, img->maxoffx, 0 );
	img->current_point.offy = CLAMP( deltay + img->bak_offy, img->maxoffy, 0 );

	gtk_widget_queue_draw( img->image_area );
	
	return( TRUE );
}

void img_text_pos_changed( GtkRange *range, img_window_struct *img)
{
	gdouble value;

	if (range == GTK_RANGE(img->sub_posX))
	{
		value = gtk_range_get_value( range );
		img->current_slide->posX = (gint)value;
	}
	else if (range == GTK_RANGE(img->sub_posY))
	{
		value = gtk_range_get_value( range );
		img->current_slide->posY = (gint)value;
	}
	else
	{
		value = gtk_range_get_value( range );
		img->current_slide->subtitle_angle = (gint)value;
	}

	gtk_widget_queue_draw(img->image_area);
}

/* Zoom callback functions */
void
img_zoom_in( GtkWidget         *item,
			 img_window_struct *img )
{
	if( img->mode == 0 )
		img_image_area_change_zoom( 0.1, FALSE, img );
	else
		img_overview_change_zoom( 0.1, FALSE, img );
}

void
img_zoom_out( GtkWidget         *item,
			  img_window_struct *img )
{
	if( img->mode == 0 )
		img_image_area_change_zoom( - 0.1, FALSE, img );
	else
		img_overview_change_zoom( - 0.1, FALSE, img );
}

void
img_zoom_reset( GtkWidget         *item,
				img_window_struct *img )
{
	if( img->mode == 0 )
		img_image_area_change_zoom( 0, TRUE, img );
	else
		img_overview_change_zoom( 0, TRUE, img );
}

void
img_zoom_fit( GtkWidget         *item,
              img_window_struct *img )
{
    gdouble step, level1, level2;
    
    if( img->mode == 0 )
    {
        /* we want to fit the frame into prev_root. Frame = video + 4 px */
        level1 = (float)img->prev_root->allocation.width / (img->video_size[0] + 4);
        level2 = (float)img->prev_root->allocation.height / (img->video_size[1] + 4);
        if (level1 < level2)
            /* step is relative to zoom level 1 */
            step = level1 - 1;
        else
            step = level2 - 1;

        img_image_area_change_zoom( 0, TRUE, img );
        img_image_area_change_zoom( step, FALSE, img );
    }
    else
        img_overview_change_zoom( 0, TRUE, img );
}

/*
 * img_image_area_change_zoom:
 * @step: amount of zoom to be changed
 * @reset: do we want to reset zoom level
 * @img: global img_widget_struct structure
 *
 * This function will increase/decrease/reset zoom level. If @step is less than
 * zero, preview area will zoom out, if @step is bigger that zero, image area
 * will zoom in.
 *
 * If @reset is TRUE, @step value is ignored and zoom reset to 1.
 *
 * Zoom level of image area is in interval [0.1, 5], but this can be easily
 * changed by adjusting bounds array values. If the zoom would be set outside of
 * this interval, it is clamped in between those two values.
 */
static void
img_image_area_change_zoom( gdouble            step,
							gboolean           reset,
							img_window_struct *img )
{
	static gdouble bounds[] = { 0.1, 5.0 };

	if( reset )
		img->image_area_zoom = 1;
	else
		img->image_area_zoom = CLAMP( img->image_area_zoom + step,
									  bounds[0], bounds[1] );

	/* Apply change */
	gtk_widget_set_size_request( img->image_area,
								 img->video_size[0] * img->image_area_zoom,
								 img->video_size[1] * img->image_area_zoom );
}

static void
img_overview_change_zoom( gdouble            step,
						  gboolean           reset,
						  img_window_struct *img )
{
	static gdouble  bounds[] = { 0.1, 3.0 };
	GtkTreeModel   *model;

	if( reset )
		img->overview_zoom = 1;
	else
		img->overview_zoom = CLAMP( img->overview_zoom + step,
									bounds[0], bounds[1] );

	/* Apply change */
	g_object_get( G_OBJECT( img->over_icon ), "model", &model, NULL );
	g_object_set( G_OBJECT( img->over_icon ), "model", NULL, NULL );
	g_object_set( img->over_cell, "zoom", img->overview_zoom, NULL );
	g_object_set( G_OBJECT( img->over_icon ), "model", model, NULL );
	g_object_unref( G_OBJECT( model ) );
}

void
img_add_stop_point( GtkButton         *button,
					img_window_struct *img )
{
	ImgStopPoint *point;
	GList        *tmp;

	if (img->current_slide == NULL)
		return;

	/* Create new point */
	point = g_slice_new( ImgStopPoint );
	*point = img->current_point;
	point->time = gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON( img->ken_duration ) );

	/* Append it to the list */
	tmp = img->current_slide->points;
	tmp = g_list_append( tmp, point );
	img->current_slide->points = tmp;
	img->current_slide->cur_point = img->current_slide->no_points;
	img->current_slide->no_points++;

	/* Update display */
	img_update_stop_display( img, FALSE );
	img_ken_burns_update_sensitivity( img, TRUE,
									  img->current_slide->no_points );

	/* Sync timings */
	img_sync_timings( img->current_slide, img );
}

void
img_update_stop_point( GtkSpinButton  *button,
					   img_window_struct *img )
{
	ImgStopPoint *point;

	if( img->current_slide == NULL || img->current_slide->points == NULL)
		return;

	/* Get selected point */
	point = g_list_nth_data( img->current_slide->points,
							 img->current_slide->cur_point );

	/* Update data */
	*point = img->current_point;
	point->time = gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON( img->ken_duration ) );
	point->zoom = gtk_range_get_value( GTK_RANGE( img->ken_zoom ) );
	
	/* Update display */
	img_update_stop_display( img, FALSE );

	/* Sync timings */
	img_sync_timings( img->current_slide, img );
}

void
img_delete_stop_point( GtkButton         *button,
					   img_window_struct *img )
{
	GList *node;

	if( img->current_slide == NULL )
		return;

	/* Get selected node and free it */
	node = g_list_nth( img->current_slide->points,
					   img->current_slide->cur_point );
	g_slice_free( ImgStopPoint, node->data );
	img->current_slide->points = 
			g_list_delete_link( img->current_slide->points, node );

	/* Update counters */
	img->current_slide->no_points--;
	img->current_slide->cur_point = MIN( img->current_slide->cur_point,
										 img->current_slide->no_points - 1 );

	/* Update display */
	img_update_stop_display( img, TRUE );
	img_ken_burns_update_sensitivity( img, TRUE,
									  img->current_slide->no_points );

	/* Sync timings */
	img_sync_timings( img->current_slide, img );
}

void
img_update_stop_display( img_window_struct *img,
						 gboolean           update_pos )
{
	gchar        *string;
	gdouble      full;

	/* Disable/enable slide duration */
	gtk_widget_set_sensitive( img->duration,
							  img->current_slide->no_points == 0 );

	/* Set slide duration */
	full = img_calc_slide_duration_points( img->current_slide->points,
										   img->current_slide->no_points );

	if( ! full )
		full = img->current_slide->duration;
	
	g_signal_handlers_block_by_func( img->duration,
									 img_spinbutton_value_changed, img );
	
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( img->duration ), full );
	
	g_signal_handlers_unblock_by_func( img->duration,
									 img_spinbutton_value_changed, img );
									 
	/* Set point count */
	string = g_strdup_printf( "%d", img->current_slide->no_points );
	gtk_label_set_text( GTK_LABEL( img->total_stop_points_label), string );
	g_free( string );

	/* If no point is set yet, use default values */
	if( img->current_slide->no_points )
	{
		ImgStopPoint *point;

		/* Set current point */
		string = g_strdup_printf( "%d", img->current_slide->cur_point + 1 );
		gtk_entry_set_text( GTK_ENTRY( img->ken_entry ), string );
		g_free( string );
		
		/* Set duration of this point */
		point = (ImgStopPoint *)g_list_nth_data( img->current_slide->points,
												 img->current_slide->cur_point );
		
		g_signal_handlers_block_by_func( img->ken_duration,
									 img_update_stop_point, img );
	
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( img->ken_duration ),
								   point->time );
		g_signal_handlers_unblock_by_func( img->ken_duration,
									 img_update_stop_point, img );
	
		/* Set zoom value */
		gtk_range_set_value( GTK_RANGE( img->ken_zoom ), point->zoom );

		/* Do we need to refresh current stop point on screen */
		if( update_pos )
			img->current_point = *point;
	}
	else
	{
		ImgStopPoint point = { 1, 0, 0, 1.0 };

		gtk_entry_set_text( GTK_ENTRY( img->ken_entry ), "" );
		gtk_spin_button_set_value( GTK_SPIN_BUTTON( img->ken_duration ), 1 );
		gtk_range_set_value( GTK_RANGE( img->ken_zoom ), 1.0 );
		if( update_pos )
			img->current_point = point;
	}

	/* Force update on preview area */
	gtk_widget_queue_draw( img->image_area );
}

void
img_update_subtitles_widgets( img_window_struct *img )
{
	gchar       	*string;
	GdkColor     	color;
	gdouble     	*f_colors;
	GtkWidget		*tmp_image;
	GdkPixbuf		*pattern_pix;
	GtkIconTheme	*icon_theme;

	/* Block all handlers */
	g_signal_handlers_block_by_func( img->slide_text_buffer,
									 img_queue_subtitle_update, img );
	g_signal_handlers_block_by_func( img->sub_font,
									 img_text_font_set, img );
	g_signal_handlers_block_by_func( img->sub_color,
									 img_font_color_changed, img );
    g_signal_handlers_block_by_func( img->sub_brdr_color,
                                     img_font_brdr_color_changed, img );
	g_signal_handlers_block_by_func( img->sub_bgcolor,
                                     img_font_bg_color_changed, img );
	g_signal_handlers_block_by_func( img->sub_border_color,
                                     img_sub_border_color_changed, img );
    g_signal_handlers_block_by_func( img->sub_border_width,
                                     img_sub_border_width_changed, img );                                    
	g_signal_handlers_block_by_func( img->sub_anim,
									 img_text_anim_set, img );
	g_signal_handlers_block_by_func( img->sub_anim_duration,
									 img_combo_box_anim_speed_changed, img );
	g_signal_handlers_block_by_func( img->sub_posX, img_text_pos_changed, img );
	g_signal_handlers_block_by_func( img->sub_posY, img_text_pos_changed, img );
	g_signal_handlers_block_by_func( img->sub_angle,
									   img_text_pos_changed, img );
	g_signal_handlers_block_by_func( img->border_top,
									   img_subtitle_top_border_toggled, img );
	g_signal_handlers_block_by_func( img->border_bottom,
									   img_subtitle_bottom_border_toggled, img );
	/* Update text field */
	string = ( img->current_slide->subtitle ?
			   img->current_slide->subtitle :
			   "" );
	g_object_set( G_OBJECT( img->slide_text_buffer ), "text", string, NULL );

	/* Update text pattern */
	if (img->current_slide->pattern_filename)
		pattern_pix = gdk_pixbuf_new_from_file_at_scale( img->current_slide->pattern_filename, 32, 32, TRUE, NULL);
	else
	{
		icon_theme = gtk_icon_theme_get_default();
		pattern_pix = gtk_icon_theme_load_icon(icon_theme,"image", 20, 0, NULL);
	}
	tmp_image = gtk_image_new_from_pixbuf(pattern_pix);
	gtk_widget_show(tmp_image);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(img->pattern_image), tmp_image);
	g_object_unref(pattern_pix);

	/* Update font button */
	string = pango_font_description_to_string(img->current_slide->font_desc);	
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(img->sub_font), string);
	g_free(string);

	/* Update color button */
	f_colors = img->current_slide->font_color;
	color.red   = (gint)( f_colors[0] * 0xffff );
	color.green = (gint)( f_colors[1] * 0xffff );
	color.blue  = (gint)( f_colors[2] * 0xffff );
	gtk_color_button_set_color( GTK_COLOR_BUTTON( img->sub_color ), &color ); 
	gtk_color_button_set_alpha( GTK_COLOR_BUTTON( img->sub_color ),
								(gint)(f_colors[3] * 0xffff ) );

    /* Update border color button */
    f_colors = img->current_slide->font_brdr_color;
    color.red   = (gint)( f_colors[0] * 0xffff );
    color.green = (gint)( f_colors[1] * 0xffff );
    color.blue  = (gint)( f_colors[2] * 0xffff );
    gtk_color_button_set_color( GTK_COLOR_BUTTON( img->sub_brdr_color ), &color ); 
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON( img->sub_brdr_color ),
                                (gint)(f_colors[3] * 0xffff ) );
                                
    /* Update background color button */
    f_colors = img->current_slide->font_bg_color;
    color.red   = (gint)( f_colors[0] * 0xffff );
    color.green = (gint)( f_colors[1] * 0xffff );
    color.blue  = (gint)( f_colors[2] * 0xffff );
    gtk_color_button_set_color( GTK_COLOR_BUTTON( img->sub_bgcolor ), &color ); 
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON( img->sub_bgcolor ),
                                (gint)(f_colors[3] * 0xffff ) );
                                
     /* Update border color button */
    f_colors = img->current_slide->border_color;
    color.red   = (gint)( f_colors[0] * 0xffff );
    color.green = (gint)( f_colors[1] * 0xffff );
    color.blue  = (gint)( f_colors[2] * 0xffff );
    gtk_color_button_set_color( GTK_COLOR_BUTTON( img->sub_border_color ), &color ); 
    gtk_color_button_set_alpha( GTK_COLOR_BUTTON( img->sub_border_color ),
                                (gint)(f_colors[3] * 0xffff ) );

	/* Update toggle buttons top/bottom borders */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(img->border_top), img->current_slide->top_border);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(img->border_bottom), img->current_slide->bottom_border);

	/* Update spinbutton border with*/
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( img->sub_border_width ), img->current_slide->border_width );
	
	/* Update animation */
	gtk_combo_box_set_active( GTK_COMBO_BOX( img->sub_anim ),
							  img->current_slide->anim_id );

	/* Update duration */
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( img->sub_anim_duration ),
							  img->current_slide->anim_duration );

	/* Update position */
	gtk_range_set_value( GTK_RANGE(img->sub_posX), (gdouble) img->current_slide->posX);
	gtk_range_set_value( GTK_RANGE(img->sub_posY), (gdouble) img->current_slide->posY);
	gtk_range_set_value( GTK_RANGE(img->sub_angle), (gdouble) img->current_slide->subtitle_angle);

	/* Unblock all handlers */
	g_signal_handlers_unblock_by_func( img->slide_text_buffer,
									   img_queue_subtitle_update, img );
	g_signal_handlers_unblock_by_func( img->sub_font,
									   img_text_font_set, img );
	g_signal_handlers_unblock_by_func( img->sub_color,
									   img_font_color_changed, img );
    g_signal_handlers_unblock_by_func( img->sub_brdr_color,
                                       img_font_brdr_color_changed, img );
	g_signal_handlers_unblock_by_func( img->sub_bgcolor,
                                       img_font_bg_color_changed, img );
	g_signal_handlers_unblock_by_func( img->sub_border_color,
                                       img_sub_border_color_changed, img );                                       
	g_signal_handlers_unblock_by_func( img->sub_border_width,
                                     img_sub_border_width_changed, img );  
	g_signal_handlers_unblock_by_func( img->sub_anim,
									   img_text_anim_set, img );
	g_signal_handlers_unblock_by_func( img->sub_anim_duration,
									   img_combo_box_anim_speed_changed, img );
	g_signal_handlers_unblock_by_func( img->sub_posX,
									   img_text_pos_changed, img );
	g_signal_handlers_unblock_by_func( img->sub_posY,
									   img_text_pos_changed, img );
	g_signal_handlers_unblock_by_func( img->sub_angle,
									   img_text_pos_changed, img );
	g_signal_handlers_unblock_by_func( img->border_top,
									   img_subtitle_top_border_toggled, img );
	g_signal_handlers_unblock_by_func( img->border_bottom,
									   img_subtitle_bottom_border_toggled, img );								   								   
}

void
img_goto_prev_point( GtkButton         *button,
					 img_window_struct *img )
{
	if( img->current_slide && img->current_slide->no_points )
	{
		img->current_slide->cur_point =
				CLAMP( img->current_slide->cur_point - 1,
					   0, img->current_slide->no_points - 1 );

		img_update_stop_display( img, TRUE );
	}
}

void
img_goto_next_point( GtkButton         *button,
					 img_window_struct *img )
{
	if( img->current_slide && img->current_slide->no_points )
	{
		img->current_slide->cur_point =
				CLAMP( img->current_slide->cur_point + 1,
					   0, img->current_slide->no_points - 1 );

		img_update_stop_display( img, TRUE );
	}
}

void
img_goto_point ( GtkEntry          *entry,
				 img_window_struct *img )
{
	const gchar *string;
	gint         number;

	string = gtk_entry_get_text( entry );
	number = (gint)strtol( string, NULL, 10 );

	if( img->current_slide && img->current_slide->no_points )
	{
		img->current_slide->cur_point =
				CLAMP( number - 1, 0, img->current_slide->no_points - 1 );

		img_update_stop_display( img, TRUE );
	}
}


void
img_calc_current_ken_point( ImgStopPoint *res,
							ImgStopPoint *from,
							ImgStopPoint *to,
							gdouble       progress,
							gint          mode )
{
	gdouble fracx, /* Factor for x offset */
			fracy, /* Factor for y offset */
			fracz; /* Factor for zoom */

	switch( mode )
	{
		case( 0 ): /* Linear mode */
			fracx = progress;
			fracy = progress;
			fracz = progress;
			break;

		case( 1 ): /* Acceleration mode */
			break;

		case( 2 ): /* Deceleration mode */
			break;
	}

	res->offx = from->offx * ( 1 - fracx ) + to->offx * fracx;
	res->offy = from->offy * ( 1 - fracy ) + to->offy * fracy;
	res->zoom = from->zoom * ( 1 - fracz ) + to->zoom * fracz;
}

void img_clipboard_cut_copy_operation(img_window_struct *img, ImgClipboardMode mode)
{
	GtkClipboard *img_clipboard;
	GList *selected = NULL;
	GtkTargetEntry targets[] = 
	{
		{ "application/imagination-info-list", 0, 0 }
	};

	selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon));
	if (selected == NULL)
		return;

	img_clipboard = gtk_clipboard_get (IMG_CLIPBOARD);

	/* Let's delete the GList if the user chooses Cut/Copy again instead of Paste */
	if (img->selected_paths)
	{
		g_list_foreach (img->selected_paths, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (img->selected_paths);
	}
	img->selected_paths = selected;
	img->clipboard_mode = mode;

	gtk_clipboard_set_with_data (	img_clipboard,
									targets, G_N_ELEMENTS (targets),
									(GtkClipboardGetFunc) 	img_clipboard_get,
									NULL, img);	
}

void img_clipboard_get (GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, img_window_struct *img)
{
	if (selection_data->target != IMG_INFO_LIST)
		return;

	gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) img->selected_paths, sizeof(GList) * g_list_length(img->selected_paths) );
}

void img_clipboard_clear (GtkClipboard *clipboard, img_window_struct *img)
{
	img_message (img, FALSE, "I'm here\n");
	//gtk_clipboard_clear(clipboard);
}

void
img_add_empty_slide( GtkMenuItem       *item,
					 img_window_struct *img )
{
	/* This structure retains values across invocations */
	ImgEmptySlide slide = { { 0, 0, 0 },         /* Start color */
								   { 1, 1, 1 },         /* Stop color */
								   { 0, 0 },            /* Start point (l) */
								   { -1, 0 },           /* Stop point (l) */
								   { 0, 0 },            /* Start point (r) */
								   { 0, 0 },            /* Stop point (r) */
								   0,                   /* Drag */
								   0,                   /* Gradient type */
								   NULL,                /* Color button */
								   NULL,                /* Preview area */
							{ NULL, NULL, NULL, NULL }, /* Radio buttons */
								   NULL,				/* Cairo context */
								   1.0,					/* Fade alpha value */
								   0
								 }; 

	GList *where_to_insert = NULL;
	GtkWidget *dialog,
			  *vbox,
			  *frame,
			  *table,
			  *radio1,
			  *radio2,
			  *radio3,
			  *radio4,
			  *color1,
			  *color2,
			  *preview,
			  *hbox;
	GdkColor   color;
	gint       i, w, h, pos;

	w = img->video_size[0] / 2;
	h = img->video_size[1] / 2;

	if (GTK_WIDGET(item) == img->edit_empty_slide)
	{
		/* Let's copy the existing empty slide
		 * values in the empty slide struct so
		 * the preview dialog area shows the settings
		 * user applied when creating the empty one */
		slide.gradient = img->current_slide->gradient;
		slide.c_start[0] = img->current_slide->g_start_color[0];
		slide.c_start[1] = img->current_slide->g_start_color[1];
		slide.c_start[2] = img->current_slide->g_start_color[2];
		
		slide.c_stop[0] = img->current_slide->g_stop_color[0];
		slide.c_stop[1] = img->current_slide->g_stop_color[1];
		slide.c_stop[2] = img->current_slide->g_stop_color[2];
		
		if (slide.gradient < 2) /* solid and linear */
		{
			slide.pl_start[0] = img->current_slide->g_start_point[0] * w;
			slide.pl_start[1] = img->current_slide->g_start_point[1] * h;
			slide.pl_stop[0]  = img->current_slide->g_stop_point[0] * w;
			slide.pl_stop[1]  = img->current_slide->g_stop_point[1] * h;
		}
		else /* radial */
		{
			slide.pr_start[0] = img->current_slide->g_start_point[0] * w;
			slide.pr_start[1] = img->current_slide->g_start_point[1] * h;
			slide.pr_stop[0]  = img->current_slide->g_stop_point[0] * w;
			slide.pr_stop[1]  = img->current_slide->g_stop_point[1] * h;
		}
	}
	dialog = gtk_dialog_new_with_buttons(
					GTK_WIDGET(item) == img->edit_empty_slide ? _("Edit empty slide") : _("Create empty slide"),
					GTK_WINDOW( img->imagination_window ),
					GTK_DIALOG_MODAL,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					NULL );

	gtk_button_box_set_layout (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area), GTK_BUTTONBOX_SPREAD);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	vbox = gtk_dialog_get_content_area( GTK_DIALOG( dialog ) );
	
	frame = gtk_frame_new( _("Empty slide options:") );
	gtk_box_pack_start( GTK_BOX( vbox ), frame, TRUE, TRUE, 5 );
	gtk_container_set_border_width( GTK_CONTAINER( frame ), 5 );

	hbox = gtk_hbox_new( FALSE, 6 );
	gtk_container_add( GTK_CONTAINER( frame ), hbox );

	table = gtk_table_new( 6, 2, TRUE );
	gtk_box_pack_start( GTK_BOX( hbox ), table, FALSE, FALSE, 10 );

	radio1 = gtk_radio_button_new_with_mnemonic( NULL, _("_Solid color") );
	gtk_table_attach( GTK_TABLE( table ), radio1, 0, 2, 0, 1,
					  GTK_FILL, GTK_FILL, 0, 0 );
	slide.radio[0] = radio1;

	radio2 = gtk_radio_button_new_with_mnemonic_from_widget(
				GTK_RADIO_BUTTON( radio1 ), _("_Linear gradient") );
	gtk_table_attach( GTK_TABLE( table ), radio2, 0, 2, 1, 2,
					  GTK_FILL, GTK_FILL, 0, 0 );
	slide.radio[1] = radio2;

	radio3 = gtk_radio_button_new_with_mnemonic_from_widget(
				GTK_RADIO_BUTTON( radio1 ), _("_Radial gradient") );
	gtk_table_attach( GTK_TABLE( table ), radio3, 0, 2, 2, 3,
					  GTK_FILL, GTK_FILL, 0, 0 );
	slide.radio[2] = radio3;
	
	radio4 = gtk_radio_button_new_with_mnemonic_from_widget(
				GTK_RADIO_BUTTON( radio1 ), _("_Fade gradient") );
	gtk_table_attach( GTK_TABLE( table ), radio4, 0, 2, 3, 4,
					  GTK_FILL, GTK_FILL, 0, 0 );
	slide.radio[3] = radio4;

	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON( slide.radio[slide.gradient] ), TRUE );

	for( i = 0; i < 4; i++ )
		g_signal_connect( G_OBJECT( slide.radio[i] ), "toggled",
						  G_CALLBACK( img_gradient_toggled ), &slide );

	color.red   = (gint)( slide.c_start[0] * 0xffff );
	color.green = (gint)( slide.c_start[1] * 0xffff );
	color.blue  = (gint)( slide.c_start[2] * 0xffff );
	color1 = gtk_color_button_new_with_color( &color );
	gtk_table_attach( GTK_TABLE( table ), color1, 0, 1, 4, 5,
					  GTK_FILL, GTK_FILL, 0, 0 );
	g_signal_connect( G_OBJECT( color1 ), "color-set",
					  G_CALLBACK( img_gradient_color_set ), &slide );

	color.red   = (gint)( slide.c_stop[0] * 0xffff );
	color.green = (gint)( slide.c_stop[1] * 0xffff );
	color.blue  = (gint)( slide.c_stop[2] * 0xffff );
	color2 = gtk_color_button_new_with_color( &color );
	gtk_table_attach( GTK_TABLE( table ), color2, 1, 2, 4, 5,
					  GTK_FILL, GTK_FILL, 0, 0 );
	gtk_widget_set_sensitive( color2, (gboolean)slide.gradient );
	g_signal_connect( G_OBJECT( color2 ), "color-set",
					  G_CALLBACK( img_gradient_color_set ), &slide );

	preview = gtk_drawing_area_new();
	gtk_widget_set_size_request( preview, w, h );
	gtk_widget_add_events( preview, GDK_BUTTON1_MOTION_MASK |
									GDK_BUTTON_PRESS_MASK |
									GDK_BUTTON_RELEASE_MASK );
	gtk_box_pack_start( GTK_BOX( hbox ), preview, TRUE, TRUE, 5 );

	g_signal_connect( G_OBJECT( preview ), "expose-event",
					  G_CALLBACK( img_gradient_expose ), &slide );
	g_signal_connect( G_OBJECT( preview ), "button-press-event",
					  G_CALLBACK( img_gradient_press ), &slide );
	g_signal_connect( G_OBJECT( preview ), "button-release-event",
					  G_CALLBACK( img_gradient_release ), &slide );
	g_signal_connect( G_OBJECT( preview ), "motion-notify-event",
					  G_CALLBACK( img_gradient_move ), &slide );

	/* Show all */
	gtk_widget_show_all( dialog );

	/* Fill internal structure */
	slide.color2 = color2;
	slide.preview = preview;
	if( slide.pl_stop[0] < 0 )
	{
		slide.pl_stop[0] = (gdouble)w;
		slide.pl_stop[1] = (gdouble)h;
		slide.pr_start[0] = w * 0.5;
		slide.pr_start[1] = h * 0.5;
	}

	if( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT )
	{
		GtkTreeIter   iter;
		slide_struct *slide_info;
		GdkPixbuf    *thumb, *pix;
		GtkTreeModel *model;
		gchar		 *path;

		if (GTK_WIDGET(item) == img->edit_empty_slide)
			slide_info = img->current_slide;
		else
			slide_info = img_create_new_slide();

		if (slide.gradient == 3)
			path = "10:0";
		else
			path = "0";

		/* Set the transition type to Misc Cross Fade or to None */
		g_signal_handlers_block_by_func(img->transition_type, (gpointer)img_combo_box_transition_type_changed, img);
		{
			GtkTreeIter   iter;
			GtkTreeModel *model;

			model = gtk_combo_box_get_model( GTK_COMBO_BOX( img->transition_type ) );
			gtk_tree_model_get_iter_from_string( model, &iter, path );
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(img->transition_type), &iter );
		}
		g_signal_handlers_unblock_by_func(img->transition_type, (gpointer)img_combo_box_transition_type_changed, img);

		if( slide_info )
		{
			gdouble p_start[2],
					p_stop[2];

			/* Convert gradient points into relative offsets (this enables us to
			 * scale gradient on any surface size) */
			if( slide.gradient == 0 || slide.gradient == 1 || slide.gradient == 3 ) /* Solid, linear and fade */
			{
				p_start[0] = slide.pl_start[0] / w;
				p_start[1] = slide.pl_start[1] / h;
				p_stop[0] = slide.pl_stop[0] / w;
				p_stop[1] = slide.pl_stop[1] / h;
			}
			else /* Radial gradient */
			{
				p_start[0] = slide.pr_start[0] / w;
				p_start[1] = slide.pr_start[1] / h;
				p_stop[0]  = slide.pr_stop[0]  / w;
				p_stop[1]  = slide.pr_stop[1]  / h;
			}

			/* Update slide info */
			img_set_slide_gradient_info( slide_info, slide.gradient,
										 slide.c_start, slide.c_stop,
										 p_start, p_stop );

			/* Create thumbnail */
			img_scale_gradient( slide.gradient, p_start, p_stop,
								slide.c_start, slide.c_stop,
								88, 49,
								&thumb, NULL );

			/* If a slide is selected, add the empty slide after it */
			where_to_insert	= gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon));
			if (where_to_insert)
			{
				pos = gtk_tree_path_get_indices(where_to_insert->data)[0]+1;
				/* Add empty slide */
				if (GTK_WIDGET(item) != img->edit_empty_slide)
				{
					pix = img_set_fade_gradient(img, slide.gradient, slide_info);
					gtk_list_store_insert_with_values(img->thumbnail_model, &iter,
													pos,
													0, thumb,
													1, slide_info,
													2, pix,
													3, FALSE,
													-1 );
				}
				else
				{
					/* Edit empty slide */
					pix = img_set_fade_gradient(img, slide.gradient, slide_info);
					model =	GTK_TREE_MODEL( img->thumbnail_model );
					gtk_tree_model_get_iter(model, &iter, where_to_insert->data);
					gtk_list_store_set(img->thumbnail_model, &iter,
													0, thumb,
													1, slide_info,
													2, pix,
													3, slide_info->subtitle ? TRUE : FALSE,
													-1 );
					/* Update gradient in image area */
					img_scale_gradient( slide.gradient,
								slide_info->g_start_point,
								slide_info->g_stop_point,
								slide_info->g_start_color,
								slide_info->g_stop_color,
								img->video_size[0],
								img->video_size[1],
								NULL,
								&img->current_image );
				}
				g_list_foreach (where_to_insert, (GFunc)gtk_tree_path_free, NULL);
				g_list_free (where_to_insert);
			}
			else
			{
				/* Add empty slide */
				if (GTK_WIDGET(item) != img->edit_empty_slide)
				{
					pix = img_set_fade_gradient(img, slide.gradient, slide_info);
					gtk_list_store_append( img->thumbnail_model, &iter );
					gtk_list_store_set(img->thumbnail_model, &iter, 0, thumb, 1, slide_info, 2, pix, 3, FALSE, -1 );
				}
			}
				
			if (GTK_WIDGET(item) != img->edit_empty_slide)
			{
				img->slides_nr++;
				img_set_total_slideshow_duration( img );
				img_select_nth_slide( img, img->slides_nr );
			}
			g_object_unref( G_OBJECT( thumb ) );
		}
	img->project_is_modified = TRUE;
	gtk_widget_queue_draw(img->image_area);
	}
	gtk_widget_destroy( dialog );
}

static void
img_gradient_toggled( GtkToggleButton *button,
					  ImgEmptySlide   *slide )
{
	GtkWidget *widget = GTK_WIDGET( button );
	gint       i;

	if( ! gtk_toggle_button_get_active( button ) )
		return;

	for( i = 0; widget != slide->radio[i]; i++ )
		;

	slide->gradient = i;

	gtk_widget_set_sensitive( slide->color2, (gboolean)i );

	if (i == 3)
	{
		if (slide->source > 0)
			g_source_remove(slide->source);
		slide->source = g_timeout_add( 2000 * 0.01, (GSourceFunc)img_fade_gradient_decrease_alpha, slide );
	}
	gtk_widget_queue_draw( slide->preview );
}

static gboolean
img_fade_gradient_decrease_alpha(ImgEmptySlide *slide)
{
	if (slide->gradient != 3)
		return FALSE;

	slide->alpha -= 0.01;
	
	if (slide->alpha <= 0)
	{
		slide->alpha = 1.0;
		slide->source = 0;
		return FALSE;
	}
	gtk_widget_queue_draw(slide->preview);
	return TRUE;
}

static void
img_gradient_color_set( GtkColorButton *button,
						ImgEmptySlide  *slide )
{
	GdkColor  color;
	gdouble  *my_color;

	gtk_color_button_get_color( button, &color );

	if( (GtkWidget *)button == slide->color2 )
		my_color = slide->c_stop;
	else
		my_color = slide->c_start;

	my_color[0] = (gdouble)color.red   / 0xffff;
	my_color[1] = (gdouble)color.green / 0xffff;
	my_color[2] = (gdouble)color.blue  / 0xffff;

	gtk_widget_queue_draw( slide->preview );
}

static gboolean
img_gradient_expose( GtkWidget      *widget,
					 GdkEventExpose *expose,
					 ImgEmptySlide  *slide )
{
	cairo_t         *cr;
	cairo_pattern_t *pattern;
	gint             w, h;
	gdouble          radius, diffx, diffy;

	gdk_drawable_get_size( expose->window, &w, &h );
	cr = gdk_cairo_create( expose->window );

	switch( slide->gradient )
	{
		case 0:
			cairo_set_source_rgb( cr, slide->c_start[0],
									  slide->c_start[1],
									  slide->c_start[2] );
			cairo_paint( cr );
			break;

		case 1:
			pattern = cairo_pattern_create_linear( slide->pl_start[0],
												   slide->pl_start[1],
												   slide->pl_stop[0],
												   slide->pl_stop[1] );
			cairo_pattern_add_color_stop_rgb( pattern, 0,
											  slide->c_start[0],
											  slide->c_start[1],
											  slide->c_start[2] );
			cairo_pattern_add_color_stop_rgb( pattern, 1,
											  slide->c_stop[0],
											  slide->c_stop[1],
											  slide->c_stop[2] );
			cairo_set_source( cr, pattern );
			cairo_paint( cr );
			cairo_pattern_destroy( pattern );

			/* Paint indicators */
			cairo_rectangle( cr, slide->pl_start[0] - 7,
								 slide->pl_start[1] - 7,
								 8, 8 );
			cairo_rectangle( cr, slide->pl_stop[0] - 7,
								 slide->pl_stop[1] - 7,
								 8, 8 );
			cairo_set_source_rgb( cr, 0, 0, 0 );
			cairo_stroke_preserve( cr );
			cairo_set_source_rgb( cr, 1, 1, 1 );
			cairo_fill( cr );
			break;

		case 2:
			diffx = ABS( slide->pr_start[0] - slide->pr_stop[0] );
			diffy = ABS( slide->pr_start[1] - slide->pr_stop[1] );
			radius = sqrt( pow( diffx, 2 ) + pow( diffy, 2 ) );
			pattern = cairo_pattern_create_radial( slide->pr_start[0],
												   slide->pr_start[1],
												   0,
												   slide->pr_start[0],
												   slide->pr_start[1],
												   radius );
			cairo_pattern_add_color_stop_rgb( pattern, 0,
											  slide->c_start[0],
											  slide->c_start[1],
											  slide->c_start[2] );
			cairo_pattern_add_color_stop_rgb( pattern, 1,
											  slide->c_stop[0],
											  slide->c_stop[1],
											  slide->c_stop[2] );
			cairo_set_source( cr, pattern );
			cairo_paint( cr );
			cairo_pattern_destroy( pattern );

			/* Paint indicators */
			cairo_rectangle( cr, slide->pr_start[0] - 7,
								 slide->pr_start[1] - 7,
								 8, 8 );
			cairo_rectangle( cr, slide->pr_stop[0] - 7,
								 slide->pr_stop[1] - 7,
								 8, 8 );
			cairo_set_source_rgb( cr, 0, 0, 0 );
			cairo_stroke_preserve( cr );
			cairo_set_source_rgb( cr, 1, 1, 1 );
			cairo_fill( cr );
			break;

		case 3:
		cairo_set_source_rgb(cr,	slide->c_stop[0],
									slide->c_stop[1],
									slide->c_stop[2] );
		cairo_paint( cr );
	
		cairo_set_source_rgb(cr, 	slide->c_start[0],
									slide->c_start[1],
									slide->c_start[2] ); 
		cairo_paint_with_alpha(cr, slide->alpha);
		if (slide->source == 0)
			slide->source = g_timeout_add( 2000 * 0.01, (GSourceFunc)img_fade_gradient_decrease_alpha, slide );
		break;
	}
	cairo_destroy( cr );

	return( TRUE );
}

static gboolean
img_gradient_press( GtkWidget      *widget,
					GdkEventButton *button,
					ImgEmptySlide  *slide )
{
	if( button->button != 1 )
		return( FALSE );

	switch( slide->gradient )
	{
		case 1:
			if( button->x < ( slide->pl_start[0] + 8 ) &&
				button->x > ( slide->pl_start[0] - 8 ) &&
				button->y < ( slide->pl_start[1] + 8 ) &&
				button->y > ( slide->pl_start[1] - 8 ) )
			{
				slide->drag = 1;
			}
			else if( button->x < ( slide->pl_stop[0] + 8 ) &&
					 button->x > ( slide->pl_stop[0] - 8 ) &&
					 button->y < ( slide->pl_stop[1] + 8 ) &&
					 button->y > ( slide->pl_stop[1] - 8 ) )
			{
				slide->drag = 2;
			}
			break;

		case 2:
			if( button->x < ( slide->pr_start[0] + 8 ) &&
				button->x > ( slide->pr_start[0] - 8 ) &&
				button->y < ( slide->pr_start[1] + 8 ) &&
				button->y > ( slide->pr_start[1] - 8 ) )
			{
				slide->drag = 1;
			}
			else if( button->x < ( slide->pr_stop[0] + 8 ) &&
					 button->x > ( slide->pr_stop[0] - 8 ) &&
					 button->y < ( slide->pr_stop[1] + 8 ) &&
					 button->y > ( slide->pr_stop[1] - 8 ) )
			{
				slide->drag = 2;
			}
			break;
	}

	return( TRUE );
}

static gboolean
img_gradient_release( GtkWidget      *widget,
					  GdkEventButton *button,
					  ImgEmptySlide  *slide )
{
	slide->drag = 0;

	return( TRUE );
}

static gboolean
img_gradient_move( GtkWidget      *widget,
				   GdkEventMotion *motion,
				   ImgEmptySlide  *slide )
{
	gint w, h;

	if( ! slide->drag )
		return( FALSE );

	gdk_drawable_get_size( motion->window, &w, &h );
	switch( slide->gradient )
	{
		case 1:
			if( slide->drag == 1 )
			{
				slide->pl_start[0] = CLAMP( motion->x, 0, w );
				slide->pl_start[1] = CLAMP( motion->y, 0, h );
			}
			else
			{
				slide->pl_stop[0] = CLAMP( motion->x, 0, w );
				slide->pl_stop[1] = CLAMP( motion->y, 0, h );
			}
			break;

		case 2:
			if( slide->drag == 1 )
			{
				slide->pr_start[0] = CLAMP( motion->x, 0, w );
				slide->pr_start[1] = CLAMP( motion->y, 0, h );
			}
			else
			{
				slide->pr_stop[0] = CLAMP( motion->x, 0, w );
				slide->pr_stop[1] = CLAMP( motion->y, 0, h );
			}
			break;
	}
	gtk_widget_queue_draw( slide->preview );

	return( TRUE );
}

gboolean
img_save_window_settings( img_window_struct *img )
{
	GKeyFile *kf;
	gchar    *group = "Interface settings";
	gchar    *rc_file, *rc_path, *contents;
	int       w, h, g, f; /* Width, height, gutter, flags */
	gboolean  max;

	gtk_window_get_size( GTK_WINDOW( img->imagination_window ), &w, &h );
	g = gtk_paned_get_position( GTK_PANED( img->paned ) );
	f = gdk_window_get_state( gtk_widget_get_window( img->imagination_window ) );
	max = f & GDK_WINDOW_STATE_MAXIMIZED;

	/* If window is maximized, store sizes that are a bit smaller than full
	 * screen, else making window non-fullscreen will have no effect. */
	if( max )
	{
		w -= 100;
		h -= 100;
	}

	kf = g_key_file_new();
	g_key_file_set_integer( kf, group, "width",   w );
	g_key_file_set_integer( kf, group, "height",  h );
	g_key_file_set_integer( kf, group, "gutter",  g );
	g_key_file_set_integer( kf, group, "mode",    img->mode );
	g_key_file_set_double(  kf, group, "zoom_p",  img->image_area_zoom );
	g_key_file_set_double(  kf, group, "zoom_o",  img->overview_zoom );
	g_key_file_set_boolean( kf, group, "max",     max );
	g_key_file_set_integer( kf, group, "preview", img->preview_fps );

	rc_path = g_build_filename( g_get_home_dir(), ".config",
								"imagination", NULL );
	rc_file = g_build_filename( rc_path, "imaginationrc", NULL );
	contents = g_key_file_to_data( kf, NULL, NULL );
	g_key_file_free( kf );

	g_mkdir_with_parents( rc_path, S_IRWXU );
	g_file_set_contents( rc_file, contents, -1, NULL );

	g_free( contents );
	g_free( rc_file );
	g_free( rc_path );

	return( FALSE );
}

gboolean
img_load_window_settings( img_window_struct *img )
{
	GKeyFile *kf;
	gchar    *group = "Interface settings";
	gchar    *rc_file;
	int       w, h, g, m; /* Width, height, gutter, mode */
	gboolean  max;

	rc_file = g_build_filename( g_get_home_dir(), ".config",
								"imagination", "imaginationrc", NULL );
	if( ! g_file_test( rc_file, G_FILE_TEST_EXISTS ) )
		return( FALSE );

	kf = g_key_file_new();
	g_key_file_load_from_file( kf, rc_file, G_KEY_FILE_NONE, NULL );

	w                    = g_key_file_get_integer( kf, group, "width",   NULL );
	h                    = g_key_file_get_integer( kf, group, "height",  NULL );
	g                    = g_key_file_get_integer( kf, group, "gutter",  NULL );
	m                    = g_key_file_get_integer( kf, group, "mode",    NULL );
	img->image_area_zoom = g_key_file_get_double(  kf, group, "zoom_p",  NULL );
	img->overview_zoom   = g_key_file_get_double(  kf, group, "zoom_o",  NULL );
	max                  = g_key_file_get_boolean( kf, group, "max",     NULL );

	/* New addition to environment settings */
	img->preview_fps     = g_key_file_get_integer( kf, group, "preview", NULL );
	if( ! img->preview_fps )
		img->preview_fps = PREVIEW_FPS_DEFAULT;

	g_key_file_free( kf );

	/* Update mode */
	img->mode = - 1;
	
	img_switch_mode( img, m );
	if (m == 0)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (img->menu_preview_mode), TRUE);
	else
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (img->menu_overview_mode), TRUE);
	/* Update window size and gutter position */
	gtk_window_set_default_size( GTK_WINDOW( img->imagination_window ), w, h );
	gtk_paned_set_position( GTK_PANED( img->paned ), g );
	if( max )
		gtk_window_maximize( GTK_WINDOW( img->imagination_window ) );

	/* Update zoom display */
	gtk_widget_set_size_request( img->image_area,
								 img->video_size[0] * img->image_area_zoom,
								 img->video_size[1] * img->image_area_zoom );
	g_object_set( img->over_cell, "zoom", img->overview_zoom, NULL );

	return( TRUE );
}

void
img_set_window_default_settings( img_window_struct *img )
{
	img->image_area_zoom = 1.0;
	img->overview_zoom = 1.0;
	img->preview_fps = PREVIEW_FPS_DEFAULT;

	/* Update mode */
	img->mode = - 1;
	img_switch_mode( img, 0 );

	/* Update window size and gutter position */
	gtk_window_set_default_size( GTK_WINDOW( img->imagination_window ),
								 800, 600 );
	gtk_paned_set_position( GTK_PANED( img->paned ), 500 );
}

void
img_rotate_slide( slide_struct   *slide,
				  ImgAngle        angle,
				  GtkProgressBar *progress )
{
	gchar *filename;

	/* If this slide is gradient, do nothing */
	if( ! slide->o_filename )
		return;

	/* If the angle is ANGLE_0, then simply copy original filename into rotated
	 * filename. */
	if( angle )
	{
		GdkPixbuf *image,
				  *rotated;
		gint       handle;
		GError    *error = NULL;

		image = gdk_pixbuf_new_from_file( slide->o_filename, NULL );
		if( progress )
			rotated = img_rotate_pixbuf( image, progress, angle );
		else
			rotated = gdk_pixbuf_rotate_simple( image, angle * 90 );
		g_object_unref( image );
		
		handle = g_file_open_tmp( "img-XXXXXX.jpg", &filename, NULL );
		close( handle );
		if( ! gdk_pixbuf_save( rotated, filename, "jpeg", &error, NULL ) )
		{
			g_message( "%s.", error->message );
			g_error_free( error );
			g_free( filename );
			filename = g_strdup( slide->r_filename );
		}
		g_object_unref( rotated );
	}
	else
		filename = g_strdup( slide->o_filename );

	/* Delete any temporary image that is present from previous rotation */
	if( slide->angle )
		unlink( slide->r_filename );
	g_free( slide->r_filename );
	slide->r_filename = filename;
	slide->angle = angle;
}

void
img_notebook_switch_page (GtkNotebook       *notebook,
                          GtkNotebookPage   *page,
                          guint              page_num,
                          img_window_struct *img)
{
    /* When message page is viewed, set it back to black */
    if (page_num == img->message_page)
    {
        PangoAttrList *   pango_list = pango_attr_list_new();
        PangoAttribute *  pango_attr = pango_attr_weight_new (PANGO_WEIGHT_NORMAL);
        pango_attr_list_insert(pango_list, pango_attr);
        gtk_label_set_attributes(GTK_LABEL(img->message_label), pango_list);
        pango_attr_list_unref (pango_list);
    }
}

void img_align_text_horizontally_vertically(GtkMenuItem *item, img_window_struct *img)
{
	cairo_t	*cr;
	gboolean centerX = FALSE, centerY = FALSE;

	cr = gdk_cairo_create( img->image_area->window );
	
	if (GTK_WIDGET(item) == img->x_justify)
	{
		centerX = TRUE;
		centerY = FALSE;
	}
	else if (GTK_WIDGET(item) == img->y_justify)
	{
		centerX = FALSE;
		centerY = TRUE;
	}
	else
	{	
		img->current_slide->subtitle_angle = 0;
		gtk_range_set_value( GTK_RANGE(img->sub_angle), 0);
	}

	img_render_subtitle( img,
								 cr,
								 img->video_size[0],
								 img->video_size[1],
								 img->image_area_zoom,
								 img->current_slide->posX,
								 img->current_slide->posY,
								 img->current_slide->subtitle_angle,
								 img->current_point.zoom,
								 img->current_point.offx,
								 img->current_point.offy,
								 img->current_slide->subtitle,
								 img->current_slide->pattern_filename,
								 img->current_slide->font_desc,
								 img->current_slide->font_color,
                                 img->current_slide->font_brdr_color,
                                 img->current_slide->font_bg_color,
                                 img->current_slide->border_color,
                                 img->current_slide->top_border,
                                 img->current_slide->bottom_border,
                                 img->current_slide->border_width,
								 img->current_slide->anim,
								 centerX,
								 centerY,
								 1.0 );

	gtk_widget_queue_draw(img->image_area);
}

void
img_pattern_clicked(GtkMenuItem *item,
					img_window_struct *img)
{
	GtkWidget		*fc;
	GtkWidget		*tmp_image;
	gchar			*filename;
	gint			response;
	GtkFileFilter	*png_filter;
	GError			*error = NULL;
	GdkPixbuf		*pattern_pix;

	fc = gtk_file_chooser_dialog_new (_("Please choose a PNG file"),
							GTK_WINDOW (img->imagination_window),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							GTK_STOCK_CANCEL,
							GTK_RESPONSE_CANCEL,
							GTK_STOCK_OPEN,
							GTK_RESPONSE_ACCEPT,
							NULL);

	/*if (img->project_current_dir)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc),img->project_current_dir);*/

	/* Image files filter */
	png_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name(png_filter,_("PNG files"));
	gtk_file_filter_add_pattern( png_filter, "*.png" );
	gtk_file_filter_add_pattern( png_filter, "*.PNG" );
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc),png_filter);

	/* Set the delete pattern button */
	GtkWidget *box = gtk_hbox_new (TRUE, 0);
	GtkWidget *delete_pattern = gtk_button_new_with_mnemonic(_("Delete current pattern"));
	gtk_box_pack_start (GTK_BOX (box), delete_pattern, FALSE, FALSE, 0);
	gtk_widget_show(delete_pattern);
	g_signal_connect (delete_pattern, "clicked", G_CALLBACK (img_delete_subtitle_pattern), img);
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER(fc), box );

	response = gtk_dialog_run (GTK_DIALOG (fc));
	if (response == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		if( ! filename )
		{
			gtk_widget_destroy(fc);
			return;
		}
		if ( (img->current_slide)->pattern_filename)
			g_free( ( img->current_slide )->pattern_filename);

		(img->current_slide)->pattern_filename = g_strdup(filename);
		pattern_pix = gdk_pixbuf_new_from_file_at_scale( filename, 32, 32, TRUE, &error);
		g_free(filename);
		if (! pattern_pix)
		{
			img_message(img, TRUE, error->message);
			g_error_free(error);
			gtk_widget_destroy(fc);
			return;
		}
		/* Swap the current image button
		 * with the loaded one */
		tmp_image = gtk_image_new_from_pixbuf(pattern_pix);
		gtk_widget_show(tmp_image);
		gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(img->pattern_image), tmp_image);
	
		gtk_widget_set_sensitive(img->sub_color, FALSE);

		if (pattern_pix)
			g_object_unref(pattern_pix); 
	}
	if (GTK_IS_WIDGET(fc))
		gtk_widget_destroy(fc);
}

void img_subtitle_top_border_toggled (GtkToggleButton *button, img_window_struct *img)
{
	img_update_sub_properties( img, NULL, -1, -1, NULL, NULL, NULL, NULL, NULL, 
							gtk_toggle_button_get_active(button),
							img->current_slide->bottom_border, -1);
	
	gtk_widget_queue_draw(img->image_area);
}

void img_subtitle_bottom_border_toggled (GtkToggleButton *button, img_window_struct *img)
{
	img_update_sub_properties( img, NULL, -1, -1, NULL, NULL, NULL, NULL, NULL, 
								img->current_slide->top_border,
								gtk_toggle_button_get_active(button), -1);
	
	gtk_widget_queue_draw(img->image_area);
}

void img_spinbutton_value_changed (GtkSpinButton *spinbutton, img_window_struct *img)
{
	gdouble duration = 0;
	GList *selected, *bak;
	GtkTreeIter iter;
	GtkTreeModel *model;
	slide_struct *info_slide;

	model = GTK_TREE_MODEL( img->thumbnail_model );
	selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->active_icon));
	if (selected == NULL)
		return;

	duration = gtk_spin_button_get_value(spinbutton);
	bak = selected;
	while (selected)
	{
		gtk_tree_model_get_iter(model, &iter,selected->data);
		gtk_tree_model_get(model, &iter,1,&info_slide,-1);
		img_set_slide_still_info( info_slide, duration, img );
		selected = selected->next;
	}

	g_list_foreach (bak, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(bak);

	/* Sync timings */
	img_sync_timings( img->current_slide, img );
}
