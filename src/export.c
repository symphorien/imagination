/*
** Copyright (c) 2009-2020 Giuseppe Torelli <colossus73@gmail.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "export.h"
#include "support.h"
#include "callbacks.h"
#include "audio.h"
#include <fcntl.h>
#include <glib/gstdio.h>

static gboolean
img_start_export( img_window_struct *img );

static gboolean
img_run_encoder( img_window_struct *img );

static gboolean
img_export_transition( img_window_struct *img );

static gboolean
img_export_transition( img_window_struct *img );

static gboolean
img_export_still( img_window_struct *img );

static void
img_export_pause_unpause( GtkToggleButton   *button,
						  img_window_struct *img );

static gboolean
img_export_frame_to_ppm( cairo_surface_t *surface,
						 gint             file_desc );

#define img_fail_export(img, msg, ...) { \
    gchar *string; \
    string = g_strdup_printf( _("Failed while exporting slide %d :" msg), img->export_slide + 1, __VA_ARGS__); \
    img_message(img, TRUE, string); \
    gtk_label_set_label( GTK_LABEL( img->export_label ), string ); \
    g_free( string ); \
    img_stop_export(img); \
}

/*
 * img_start_export:
 * @img: global img_wndow_struct structure
 *
 * This function performs the last export step - spawns ffmpeg and initiates the
 * export progress indicators.
 *
 * Return value: Always returns FALSE, since we want it to be removed from main
 * context.
 */
static gboolean
img_start_export( img_window_struct *img )
{
	GtkTreeIter   iter;
	slide_struct *entry;
	GtkTreeModel *model;
	GtkWidget    *dialog;
	GtkWidget	 *image;
	GtkWidget    *vbox, *hbox;
	GtkWidget    *label;
	GtkWidget    *progress;
	AtkObject *atk;
	gchar        *string;
	cairo_t      *cr;

	/* Set export info */
	img->export_is_running = 3;

	/* Spawn ffmepg and abort if needed */
	if( ! img_run_encoder(img) )
	{
		img_stop_export(img);
		return( FALSE );
	}

	/* Create progress window with cancel and pause buttons, calculate
	 * the total number of frames to display. */
	dialog = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	img->export_dialog = dialog;
	gtk_window_set_title( GTK_WINDOW( img->export_dialog ),
						  _("Exporting the slideshow") );
	g_signal_connect (G_OBJECT(img->export_dialog), "delete_event", G_CALLBACK (on_close_export_dialog), img);
	gtk_container_set_border_width( GTK_CONTAINER( dialog ), 10 );
	gtk_window_set_default_size( GTK_WINDOW( dialog ), 400, -1 );
	gtk_window_set_type_hint( GTK_WINDOW( dialog ), GDK_WINDOW_TYPE_HINT_DIALOG );
	gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( dialog ),
								  GTK_WINDOW( img->imagination_window ) );

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add( GTK_CONTAINER( dialog ), vbox );

	label = gtk_label_new( _("Preparing for export ...") );
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	atk = gtk_widget_get_accessible(label);
	atk_object_set_description(atk, _("Status of export"));
	img->export_label = label;
	gtk_box_pack_start( GTK_BOX( vbox ), label, FALSE, FALSE, 0 );

	progress = gtk_progress_bar_new();
	img->export_pbar1 = progress;
	string = g_strdup_printf( "%.2f", .0 );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( progress ), string );
	gtk_box_pack_start( GTK_BOX( vbox ), progress, FALSE, FALSE, 0 );

	label = gtk_label_new( _("Overall progress:") );
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	gtk_box_pack_start( GTK_BOX( vbox ), label, FALSE, FALSE, 0 );

	progress = gtk_progress_bar_new();
	img->export_pbar2 = progress;
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( progress ), string );
	gtk_box_pack_start( GTK_BOX( vbox ), progress, FALSE, FALSE, 0 );
	g_free( string );
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );
	label = gtk_label_new( _("Elapsed time:") );
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0.5);
	gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );

	img->elapsed_time_label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(img->elapsed_time_label), 0);
	gtk_label_set_yalign(GTK_LABEL(img->elapsed_time_label), 0.5);
	gtk_box_pack_start( GTK_BOX( hbox ), img->elapsed_time_label, FALSE, FALSE, 0 );

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
	gtk_box_pack_start( GTK_BOX( vbox ), hbox, FALSE, FALSE, 0 );

	image = gtk_image_new_from_icon_name("list-remove", GTK_ICON_SIZE_BUTTON );
	img->export_cancel_button = gtk_button_new_with_label(_("Cancel"));
	gtk_button_set_always_show_image (GTK_BUTTON (img->export_cancel_button), TRUE);
	gtk_button_set_image (GTK_BUTTON (img->export_cancel_button), image);
	
	g_signal_connect_swapped( G_OBJECT( img->export_cancel_button ), "clicked",
							  G_CALLBACK( img_close_export_dialog ), img );
	gtk_box_pack_end( GTK_BOX( hbox ), img->export_cancel_button, FALSE, FALSE, 0 );

	image = gtk_image_new_from_icon_name( "media-playback-pause", GTK_ICON_SIZE_BUTTON );
	img->export_pause_button = gtk_toggle_button_new_with_label(_("Pause"));
	gtk_button_set_always_show_image (GTK_BUTTON (img->export_pause_button), TRUE);
	gtk_button_set_image (GTK_BUTTON (img->export_pause_button), image);

	g_signal_connect( G_OBJECT( img->export_pause_button ), "toggled",
					  G_CALLBACK( img_export_pause_unpause ), img );
	gtk_box_pack_end( GTK_BOX( hbox ), img->export_pause_button, FALSE, FALSE, 0 );

	gtk_widget_show_all( dialog );

	/* Create first slide */
	img->image1 = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
											  img->video_size[0],
											  img->video_size[1] );
	cr = cairo_create( img->image1 );
	cairo_set_source_rgb( cr, img->background_color[0],
							  img->background_color[1],
							  img->background_color[2] );
	cairo_paint( cr );
	cairo_destroy( cr );

	/* Load first image from model */
	model = GTK_TREE_MODEL( img->thumbnail_model );
	gtk_tree_model_get_iter_first( model, &iter );
	gtk_tree_model_get( model, &iter, 1, &entry, -1 );

	gboolean success = FALSE;

	if( ! entry->o_filename )
	{
		success = img_scale_gradient( entry->gradient, entry->g_start_point,
							entry->g_stop_point, entry->g_start_color,
							entry->g_stop_color, img->video_size[0],
							img->video_size[1], NULL, &img->image2 );
	}
	else
	{
		success = img_scale_image( entry->p_filename, img->video_ratio,
						 0, 0, img->distort_images,
						 img->background_color, NULL, &img->image2 );
	}

	if (!success) {
	    img->image2 = NULL;
	    img_stop_export(img);
	    return (FALSE);
	}

	/* Add export idle function and set initial values */
	img->export_is_running = 4;
	img->current_slide = entry;
	img->total_nr_frames = img->total_secs * img->export_fps;
	img->displayed_frame = 0;
	img->next_slide_off = 0;
	img_calc_next_slide_time_offset( img, img->export_fps );

	/* Create surfaces to be passed to transition renderer */
	img->image_from = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
												  img->video_size[0],
												  img->video_size[1] );
	img->image_to = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
												img->video_size[0],
												img->video_size[1] );
	img->exported_image = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
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

	/* Set stop points */
	img->cur_point = NULL;
	img->point1 = NULL;
	img->point2 = (ImgStopPoint *)( img->current_slide->no_points ?
									img->current_slide->points->data :
									NULL );

	/* Set first slide */
	gtk_tree_model_get_iter_first( GTK_TREE_MODEL( img->thumbnail_model ),
								   &img->cur_ss_iter );

	img->export_slide = 1;
	img->export_idle_func = (GSourceFunc)img_export_transition;
	img->source_id = g_idle_add( (GSourceFunc)img_export_transition, img );

	img->elapsed_timer = g_timer_new();

	string = g_strdup_printf( _("Slide %d export progress:"), 1 );
	/* I did this for the translators. ^^ */
	gtk_label_set_label( GTK_LABEL( img->export_label ), string );
	g_free( string );

	/* Update display */
	gtk_widget_queue_draw( img->image_area );

	return( FALSE );
}

gboolean
on_close_export_dialog(GtkWidget * UNUSED(widget), GdkEvent * UNUSED(event), img_window_struct *img)
{
    img_close_export_dialog(img);
    return TRUE;
}

/* If the export wasn't aborted by user display the close button
 * and wait for the user to click it to close the dialog. This
 * way the elapsed time is still shown until the dialog is closed */
void img_post_export(img_window_struct *img)
{
	gchar *dummy;

	/* Free all the allocated resources */
	img_stop_export(img);
	
	gtk_widget_hide(img->export_pause_button);
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar1 ), 1);
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar2 ), 1);
	gtk_button_set_label(GTK_BUTTON(img->export_cancel_button), _("Close"));
	g_signal_connect_swapped (img->export_cancel_button, "clicked", G_CALLBACK (gtk_widget_destroy), img->export_dialog);
	
	dummy = img_convert_seconds_to_time( (gint) img->elapsed_time);
	img_message(img,TRUE, _("Elapsed time: %s\n\n"), dummy);
	g_free(dummy);
}

void img_close_export_dialog(img_window_struct *img)
{
	img_stop_export(img);
	gtk_widget_destroy(img->export_dialog);
}

/*
 * img_stop_export:
 * @img: global img_window_struct structure
 *
 * This function should be called whenever we want to terminate export. It'll
 * take care of removing any idle functions, terminating any spawned processes
 * or threads, etc.
 *
 * If this function needs to be connected to some kind of widget as a callback,
 * this should be done using g_signal_connect_swapped, since we're not
 * interested in widget that caused the termination.
 *
 * Return value: always FALSE, since this function can be also called as an idle
 * callback and needs to remove itelf from main context.
 */
gboolean
img_stop_export( img_window_struct *img )
{
	/* Do any additional tasks */
	if( img->export_is_running > 3 )
	{
		kill( img->encoder_pid, SIGINT );
		g_source_remove( img->source_id );

		close(img->file_desc);
		g_spawn_close_pid( img->encoder_pid );

		/* Free ffmpeg cmd line */
		g_free( img->export_cmd_line );
	
		/* Destroy images that were used */
		if (img->image1) cairo_surface_destroy( img->image1 );
		if (img->image2) cairo_surface_destroy( img->image2 );
		cairo_surface_destroy( img->image_from );
		cairo_surface_destroy( img->image_to );
		cairo_surface_destroy( img->exported_image );

		/* Stops the timer */
		g_timer_destroy(img->elapsed_timer);
	}

	if( img->export_is_running > 1 )
	{
		/* Kill sox thread */
		if( img->exported_audio_no )
		{
			int i;

			if( g_atomic_int_get( &img->sox_flags ) != 2 )
			{
				gint   fd;
				guchar buf[1024];

				g_atomic_int_set( &img->sox_flags, 1 );

				/* Flush any buffered audio data from pipe */
				fd = open( img->fifo, O_RDONLY );
				while( read( fd, buf, sizeof( buf ) ) )
					;
				close( fd );
			}

			/* Wait for thread to finish */
			g_thread_join( img->sox );
			img->sox = NULL;

			for( i = 0; i < img->exported_audio_no; i++ )
				g_free( img->exported_audio[i] );
			img->exported_audio = NULL;
			img->exported_audio_no = 0;
		}
	}

	/* If we created FIFO, we need to destroy it now */
	if( img->fifo )
	{
		g_unlink( img->fifo );
		g_free( img->fifo );
		img->fifo = NULL;
	}

	/* Indicate that export is not running any more */
	img->export_is_running = 0;

	/* Switch mode if needed */
	if( img->auto_switch )
	{
		img->auto_switch = FALSE;
		img_switch_mode( img, 1 );
	}
	else
	{
		/* Redraw preview area */
		gtk_widget_queue_draw( img->image_area );
	}

	return( FALSE );
}

/*
 * img_prepare_pixbufs:
 * @img: global img_window_struct
 * @preview: do we load image for preview
 *
 * This function is used when previewing or exporting slideshow. It goes
 * through the model and prepares everything for next transition.
 *
 *
 * This function also sets img->point[12] that are used for transitions.
 *
 * Return value: TRUE if images have been succefully prepared, FALSE otherwise.
 */
gboolean
img_prepare_pixbufs( img_window_struct *img)
{
	GtkTreeModel    *model;
	GtkTreePath     *path;
	gchar			*selected_slide_nr;
	static gboolean  last_transition = TRUE;
	gboolean success;

	model = GTK_TREE_MODEL( img->thumbnail_model );

	/* Get last stop point of current slide */
	img->point1 = (ImgStopPoint *)( img->current_slide->no_points ?
									g_list_last( img->current_slide->points )->data :
									NULL );

	/* save the cur iter in the iconview to unselect the slide before selecting the next one */
	img->prev_ss_iter = img->cur_ss_iter;

	if( last_transition && gtk_tree_model_iter_next( model, &img->cur_ss_iter ) )
	{
		img->cur_nr_of_selected_slide++;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &img->prev_ss_iter); 
		if (path)
		{	
			gtk_icon_view_unselect_path (GTK_ICON_VIEW(img->thumbnail_iconview), path);
			gtk_tree_path_free(path);
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &img->cur_ss_iter); 
		gtk_icon_view_select_path (GTK_ICON_VIEW(img->thumbnail_iconview), path);
		gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(img->thumbnail_iconview), path, FALSE, 0.0, 0.0);
		gtk_tree_path_free(path);
		
		selected_slide_nr = g_strdup_printf("%d",img->cur_nr_of_selected_slide);
		gtk_entry_set_text(GTK_ENTRY(img->slide_number_entry),selected_slide_nr);
		g_free(selected_slide_nr);

		/* We have next iter, so prepare for next round */
		cairo_surface_destroy( img->image1 );
		img->image1 = img->image2;
		gtk_tree_model_get( model, &img->cur_ss_iter, 1, &img->current_slide, -1 );

		if( ! img->current_slide->o_filename )
		{
			img_scale_gradient( img->current_slide->gradient,
								img->current_slide->g_start_point,
								img->current_slide->g_stop_point,
								img->current_slide->g_start_color,
								img->current_slide->g_stop_color,
								img->video_size[0],
								img->video_size[1], NULL, &img->image2 );
		}
		else
		{
			success = img_scale_image( img->current_slide->p_filename, img->video_ratio,
								0, img->video_size[1], img->distort_images,
								img->background_color, NULL, &img->image2 );

			if (!success)
			{
				img->image2 = NULL;
				img_fail_export(img, "while loading file %s", img->current_slide->p_filename);
				return (FALSE);
			}
		}
		/* Get first stop point */
		img->point2 = (ImgStopPoint *)( img->current_slide->no_points ?
										img->current_slide->points->data :
										NULL );

		return( TRUE );
	}
	else if (last_transition)
	{
		if( img->bye_bye_transition )
		{
			cairo_t *cr;

			/* We displayed last image, but bye-bye transition hasn't
			 * been displayed. */
		
			last_transition = FALSE;
			cairo_surface_destroy( img->image1 );
			img->image1 = img->image2;

			img->image2 = cairo_image_surface_create( CAIRO_FORMAT_RGB24,
													  img->video_size[0],
													  img->video_size[1] );
			cr = cairo_create( img->image2 );
			cairo_set_source_rgb( cr, img->background_color[0],
									  img->background_color[1],
									  img->background_color[2] );
			cairo_paint( cr );
			cairo_destroy( cr );

			img->current_slide = &img->final_transition;
			img->point2 = NULL;
			return( TRUE );
		}
		
	}
	/* Unselect the last selected item during the preview */
	GList *list;
	list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->thumbnail_iconview));

	if (list)
	{
		gtk_icon_view_unselect_path (GTK_ICON_VIEW(img->thumbnail_iconview), (GtkTreePath*)list->data);
		GList *node0;
		for(node0 = list;node0 != NULL;node0 = node0->next) {
			gtk_tree_path_free(node0->data);
		}
		g_list_free (list);
	}

	/*  Reselect the first selected slide before the preview if any */
	if (img->first_selected_path)
	{
		gtk_icon_view_select_path (GTK_ICON_VIEW(img->thumbnail_iconview), img->first_selected_path);
		gtk_icon_view_scroll_to_path(GTK_ICON_VIEW(img->thumbnail_iconview), img->first_selected_path, FALSE, 0.0, 0.0);
	}

	/* We're done now */
	last_transition = TRUE;
	return( FALSE );
}

/*
 * img_run_encoder:
 * @img:
 *
 * Tries to start encoder.
 *
 * Return value: TRUE if encoder has been started succesfully, else FALSE.
 */
static gboolean
img_run_encoder( img_window_struct *img )
{
	GtkWidget  *message;
	GError     *error = NULL;
	gchar     **argv;
	gint		argc;
	gboolean    ret;

	g_shell_parse_argv( img->export_cmd_line, &argc, &argv, NULL);
	img_message(img, FALSE, "Running %s\n", img->export_cmd_line);

	ret = g_spawn_async_with_pipes( NULL, argv, NULL,
									G_SPAWN_SEARCH_PATH,
									NULL, NULL, &img->encoder_pid,
									&img->file_desc,
                                    NULL,               /* print to standard_output */
                                    NULL,               /* print to standard_error */
                                    &error );
	if( ! ret )
	{
		message = gtk_message_dialog_new( GTK_WINDOW( img->imagination_window ),
										  GTK_DIALOG_MODAL |
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
										  _("Failed to launch the encoder!" ) );
		gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( message ),
												  "%s.", error->message );
		gtk_dialog_run( GTK_DIALOG( message ) );
		gtk_widget_destroy( message );
		g_error_free( error );
	}

	g_strfreev( argv );

	return( ret );
}

/*
 * img_calc_next_slide_time_offset:
 * @img: global img_window_struct structure
 * @rate: frame rate to be used for calculations
 *
 * This function will calculate:
 *   - time offset of next slide (img->next_slide_off)
 *   - number of frames for current slide (img->slide_nr_frames)
 *   - number of slides needed for transition (img->slide_trans_frames)
 *   - number of slides needed for still part (img->slide_still_franes)
 *   - reset current slide counter to 0 (img->slide_cur_frame)
 *   - number of frames for subtitle animation (img->no_text_frames)
 *   - reset current subtitle counter to 0 (img->cur_text_frame)
 *
 * Return value: new time offset. The same value is stored in
 * img->next_slide_off.
 */
gdouble
img_calc_next_slide_time_offset( img_window_struct *img,
								 gdouble            rate )
{
	if( img->current_slide->render )
	{
		img->next_slide_off += img->current_slide->duration +
							   img->current_slide->speed;
		img->slide_trans_frames = img->current_slide->speed * rate;
	}
	else
	{
		img->next_slide_off += img->current_slide->duration;
		img->slide_trans_frames = 0;
	}

	img->slide_nr_frames = img->next_slide_off * rate - img->displayed_frame;
	img->slide_cur_frame = 0;
	img->slide_still_frames = img->slide_nr_frames - img->slide_trans_frames;

	/* Calculate subtitle frames */
	if( img->current_slide->subtitle )
	{
		img->cur_text_frame = 0;
		img->no_text_frames = img->current_slide->anim_duration * rate;
	}

	return( img->next_slide_off );
}

/*
 * img_export_transition:
 * @img:
 *
 * This is idle callback function that creates transition frames. When
 * transition is complete, it detaches itself from main context and connects
 * still export function.
 *
 * Return value: TRUE if transition isn't exported completely, FALSE otherwise.
 */
static gboolean
img_export_transition( img_window_struct *img )
{
	gchar   string[10];
	gchar	*dummy;
	gdouble export_progress;

	/* If we rendered all transition frames, connect still export */
	if( img->slide_cur_frame == img->slide_trans_frames )
	{
		img->export_idle_func = (GSourceFunc)img_export_still;
		img->source_id = g_idle_add( (GSourceFunc)img_export_still, img );

		return( FALSE );
	}

	/* Draw one frame of transition animation */
	img_render_transition_frame( img );

	/* Increment global frame counters and update progress bars */
	img->slide_cur_frame++;
	img->displayed_frame++;

	export_progress = CLAMP( (gdouble)img->slide_cur_frame /
									  img->slide_nr_frames, 0, 1 );
	snprintf( string, 10, "%.2f%%", export_progress * 100 );
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar1 ),
								   export_progress );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( img->export_pbar1 ), string );
	export_progress = CLAMP( (gdouble)img->displayed_frame /
									  img->total_nr_frames, 0, 1 );
	snprintf( string, 10, "%.2f%%", export_progress * 100 );
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar2 ),
								   export_progress );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( img->export_pbar2 ), string );

	/* Update the elapsed time */
	img->elapsed_time = g_timer_elapsed(img->elapsed_timer, NULL);  
	dummy = img_convert_seconds_to_time( (gint) img->elapsed_time);
	gtk_label_set_text(GTK_LABEL(img->elapsed_time_label), dummy);
	g_free(dummy);

	/* Draw every 10th frame of animation on screen 
	if( img->displayed_frame % 10 == 0 )
		gtk_widget_queue_draw( img->image_area );*/

	return( TRUE );
}

/*
 * img_export_still:
 * @img:
 *
 * Idle callback that outputs still image frames. When enough frames has been
 * outputed, it connects transition export.
 *
 * Return value: TRUE if more still frames need to be exported, else FALSE.
 */
static gboolean
img_export_still( img_window_struct *img )
{
	gdouble export_progress;
	gchar	*dummy;

	/* If there is next slide, connect transition preview, else finish
	 * preview. */
	if( img->slide_cur_frame == img->slide_nr_frames )
	{
		if( img_prepare_pixbufs( img) )
		{
			gchar *string;

			img_calc_next_slide_time_offset( img, img->export_fps );
			img->export_slide++;

			/* Make dialog more informative */
			if( img->current_slide->duration == 0 )
				string = g_strdup_printf( _("Final transition export progress:") );
			else
				string = g_strdup_printf( _("Slide %d export progress:"),
										  img->export_slide );
			gtk_label_set_label( GTK_LABEL( img->export_label ), string );
			g_free( string );

			img->export_idle_func = (GSourceFunc)img_export_transition;
			img->source_id = g_idle_add( (GSourceFunc)img_export_transition, img );

			img->cur_point = NULL;
		}
		else {
			img_post_export(img);
		}

		return( FALSE );
	}
	gchar   string[10];

	/* Draw frames until we have enough of them to fill slide duration gap. */
	
	 img_render_still_frame( img, img->export_fps );

	/* Increment global frame counter and update progress bar */
	img->still_counter++;
	img->slide_cur_frame++;
	img->displayed_frame++;

	/* CLAMPS are needed here because of the loosy conversion when switching
	 * from floating point to integer arithmetics. */
	export_progress = CLAMP( (gdouble)img->slide_cur_frame /
									  img->slide_nr_frames, 0, 1 );
	snprintf( string, 10, "%.2f%%", export_progress * 100 );
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar1 ),
								   export_progress );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( img->export_pbar1 ), string );

	export_progress = CLAMP( (gdouble)img->displayed_frame /
									  img->total_nr_frames, 0, 1 );
	snprintf( string, 10, "%.2f%%", export_progress * 100 );
	gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( img->export_pbar2 ),
								   export_progress );
	gtk_progress_bar_set_text( GTK_PROGRESS_BAR( img->export_pbar2 ), string );

	/* Update the elapsed time */
	img->elapsed_time = g_timer_elapsed(img->elapsed_timer, NULL);  
	dummy = img_convert_seconds_to_time( (gint) img->elapsed_time);
	gtk_label_set_text(GTK_LABEL(img->elapsed_time_label), dummy);
	g_free(dummy);
	
	/* Draw every 10th frame of animation on screen 
	if( img->displayed_frame % 10 == 0 )
		gtk_widget_queue_draw( img->image_area );*/

	return( TRUE );
}

/*
 * img_export_pause_unpause:
 * @img:
 *
 * Temporarily disconnect export functions. This doesn't stop ffmpeg!!!
 */
static void
img_export_pause_unpause( GtkToggleButton   *button,
						  img_window_struct *img )
{
	if( gtk_toggle_button_get_active( button ) )
	{
		/* Pause export */
		g_source_remove( img->source_id );
		g_timer_stop(img->elapsed_timer);
	}
	else
	{
		img->source_id = g_idle_add(img->export_idle_func, img);
		g_timer_continue(img->elapsed_timer);
	}
}

void
img_render_transition_frame( img_window_struct *img )
{
	ImgStopPoint  point = { 0, 0, 0, 1.0 }; /* Default point */
	gdouble       progress;
	cairo_t      *cr;

	/* Do image composing here and place result in exported_image */
	
	/* Create first image
	 * this is a dirt hack to have Imagination use the image_from painted
	 * with the second color set in the empty slide fade gradient */
	if (img->current_slide->o_filename && img->gradient_slide)
	{
		cr = cairo_create( img->image_from );
		cairo_set_source_rgb(cr,	img->g_stop_color[0],
									img->g_stop_color[1],
									img->g_stop_color[2]);
		cairo_paint( cr );
	}
	else
	{
		cr = cairo_create( img->image_from );
		if (img->current_slide->gradient != 3)
			img_draw_image_on_surface( cr, img->video_size[0], img->image1,
								( img->point1 ? img->point1 : &point ), img );
	}
#if 0
	/* Render subtitle if present */
	if( img->current_slide->subtitle )
	{
		gdouble       progress;     /* Text animation progress */
		ImgStopPoint *p_draw_point; 

		progress = (gdouble)img->cur_text_frame / ( img->no_text_frames - 1 );
		progress = CLAMP( progress, 0, 1 );
		img->cur_text_frame++;

		p_draw_point = ( img->point1 ? img->point1 : &point );

		img_render_subtitle( img,
							 cr,
							 img->video_size[0],
							 img->video_size[1],
							 1.0,
							 img->current_slide->position,
							 p_draw_point->zoom,
							 p_draw_point->offx,
							 p_draw_point->offy,
							 img->current_slide->subtitle,
							 img->current_slide->font_desc,
							 img->current_slide->font_color,
							 img->current_slide->anim,
							 FALSE,
							 FALSE,
							 progress );
	}
#endif
	cairo_destroy( cr );

	/* Create second image */
	cr = cairo_create( img->image_to );
	if (img->current_slide->gradient != 3)
		img_draw_image_on_surface( cr, img->video_size[0], img->image2,
							   ( img->point2 ? img->point2 : &point ), img );
	/* FIXME: Add subtitles here */
	cairo_destroy( cr );

	/* Compose them together */
	progress = (gdouble)img->slide_cur_frame / ( img->slide_trans_frames - 1 );
	cr = cairo_create( img->exported_image );
	cairo_save( cr );
	img->current_slide->render( cr, img->image_from, img->image_to, progress );
	cairo_restore( cr );
	
	/* Export frame */
	if (img->export_is_running) {
		gboolean ok = img_export_frame_to_ppm( img->exported_image, img->file_desc );
		if (!ok) {
		    img_fail_export(img, "Error while exporting frame to ppm: errno=%d", errno);
		}
	}

	cairo_destroy( cr );
}

void
img_render_still_frame( img_window_struct *img,
						gdouble            rate )
{
	cairo_t      *cr;
	ImgStopPoint *p_draw_point;                  /* Pointer to current sp */
	ImgStopPoint  draw_point = { 0, 0, 0, 1.0 }; /* Calculated stop point */

	/* If no stop points are specified, we simply draw img->image2 with default
	 * stop point on each frame.
	 *
	 * If we have only one stop point, we draw img->image2 on each frame
	 * properly scaled, with no movement.
	 *
	 * If we have more than one point, we draw movement from point to point.
	 */
	switch( img->current_slide->no_points )
	{
		case( 0 ): /* No stop points */
			p_draw_point = &draw_point;
			break;

		case( 1 ): /* Single stop point */
			p_draw_point = (ImgStopPoint *)img->current_slide->points->data;
			break;

		default:   /* Many stop points */
			{
				ImgStopPoint *point1,
							 *point2;
				gdouble       progress;
				GList        *tmp;

				if( ! img->cur_point )
				{
					/* This is initialization */
					img->cur_point = img->current_slide->points;
					point1 = (ImgStopPoint *)img->cur_point->data;
					img->still_offset = point1->time;
					img->still_max = img->still_offset * rate;
					img->still_counter = 0;
					img->still_cmlt = 0;
				}
				else if( img->still_counter == img->still_max )
				{
					/* This is advancing to next point */
					img->cur_point = g_list_next( img->cur_point );
					point1 = (ImgStopPoint *)img->cur_point->data;
					img->still_offset += point1->time;
					img->still_cmlt += img->still_counter;
					img->still_max = img->still_offset * rate -
									 img->still_cmlt;
					img->still_counter = 0;
				}

				point1 = (ImgStopPoint *)img->cur_point->data;
				tmp = g_list_next( img->cur_point );
				if( tmp )
				{
					point2 = (ImgStopPoint *)tmp->data;
					progress = (gdouble)img->still_counter /
										( img->still_max - 1);
					img_calc_current_ken_point( &draw_point, point1, point2,
												progress, 0 );
					p_draw_point = &draw_point;
				}
				else
					p_draw_point = point1;
			}
			break;
	}

	/* Paint surface */
	cr = cairo_create( img->exported_image );
	if (img->current_slide->gradient == 3)
		img_draw_image_on_surface( cr, img->video_size[0], img->image_to,
							   p_draw_point, img );
	else
		img_draw_image_on_surface( cr, img->video_size[0], img->image2,
							   p_draw_point, img );

	/* Render subtitle if present */
	if( img->current_slide->subtitle )
	{
		gdouble progress; /* Text animation progress */

		progress = (gdouble)img->cur_text_frame / ( img->no_text_frames - 1 );
		progress = CLAMP( progress, 0, 1 );
		img->cur_text_frame++;

		img_render_subtitle( img,
							 cr,
							 1.0,
							 img->current_slide->posX,
							 img->current_slide->posY,
							 img->current_slide->subtitle_angle,
							 img->current_slide->alignment,
							 p_draw_point->zoom,
							 p_draw_point->offx,
							 p_draw_point->offy,
							 FALSE,
							 FALSE,
							 progress );
	}

	/* Export frame */
	if (img->export_is_running) {
		gboolean ok = img_export_frame_to_ppm( img->exported_image, img->file_desc );
		if (!ok) {
		    img_fail_export(img, "Error while exporting frame to ppm: errno=%d", errno);
		}
	}


	/* Destroy drawing context */
	cairo_destroy( cr );
}

/* Writes len bytes of buffer into file_desc in several tries if
 * necessary. Also retries if EAGAIN is encountered.
 * Resets errno, returns false when an error happened and is set in
 * errno.
 * */
gboolean safe_write(gint file_desc, const gchar* buffer, gsize len) {
    gsize n = 0;
    errno = 0;
    while (n < len && (errno == EAGAIN || !errno)) {
	n += write(file_desc, buffer+n, len-n);
    }
    if (errno) {
	return FALSE;
    }
    return TRUE;
}


static gboolean
img_export_frame_to_ppm( cairo_surface_t *surface,
						 gint             file_desc )
{
	gint            width, height, stride, row, col;
	guchar         *data, *pix;
	gchar          *header;

	guchar         *buffer, *tmp;
	gint            buf_size;
	gboolean	ok;

	/* Image info and pixel data */
	width  = cairo_image_surface_get_width( surface );
	height = cairo_image_surface_get_height( surface );
	stride = cairo_image_surface_get_stride( surface );
	pix    = cairo_image_surface_get_data( surface );

	/* Output PPM file header information:
	 *   - P6 is a magic number for PPM file
	 *   - width and height are image's dimensions
	 *   - 255 is number of colors
	 * */
	header = g_strdup_printf( "P6\n%d %d\n255\n", width, height );

	ok = safe_write( file_desc, header, sizeof( gchar ) * strlen( header ) );
	g_free( header );
	if (!ok) {
	    g_message("Could not write full ppm header: %s", strerror(errno));
	    return FALSE;
	}

	/* PRINCIPLES BEHING EXPORT LOOP
	 *
	 * Cairo surface data is composed of height * stride 32-bit numbers. The
	 * actual data for displaying image is inside height * width boundary,
	 * and each pixel is represented with 1 32-bit number.
	 *
	 * In CAIRO_FORMAT_ARGB32, first 8 bits contain alpha value, second 8
	 * bits red value, third green and fourth 8 bits blue value.
	 *
	 * In CAIRO_FORMAT_RGB24, groups of 8 bits contain values for red, green
	 * and blue color respectively. Last 8 bits are unused.
	 *
	 * Since guchar type contains 8 bits, it's usefull to think of cairo
	 * surface as a height * stride gropus of 4 guchars, where each guchar
	 * holds value for each color. And this is the principle behing my method
	 * of export.
	 * */

	/* Output PPM data */
	buf_size = sizeof( guchar ) * width * height * 3;
	buffer = g_slice_alloc( buf_size );
	tmp = buffer;
	data = pix;
	for( row = 0; row < height; row++ )
	{
		data = pix + row * stride;

		for( col = 0; col < width; col++ )
		{
			/* Output data. This is done differenty on little endian
			 * and big endian machines. */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			/* Little endian machine sees pixel data as being stored in
			 * BGRA format. This is why we skip the last 8 bit group and
			 * read the other three groups in reverse order. */
			tmp[0] = data[2];
			tmp[1] = data[1];
			tmp[2] = data[0];
#elif G_BYTE_ORDER == G_BIG_ENDIAN
			tmp[0] = data[1];
			tmp[1] = data[2];
			tmp[2] = data[3];
#endif
			data += 4;
			tmp  += 3;
		}
	}
	ok = safe_write( file_desc, (gchar *)buffer, buf_size );
	if (!ok) {
	    g_message("Could not write full ppm image: %s", strerror(errno));
	    return FALSE;
	}
	g_slice_free1( buf_size, buffer );
	return TRUE;
}

void img_exporter (GtkWidget *button, img_window_struct *img )
{
	const gchar *filename;
	GtkWidget    *dialog, *container_menu;
	GtkWidget    *vbox, *range_menu, *export_grid, *sample_rate, *bitrate;
	GtkWidget    *ex_vbox, *audio_frame, *video_frame, *label;
	GtkWidget    *width, *height, *frame_rate, *quality, *slideshow_title_entry;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkCellRenderer *cell;
	GtkTreeIter   iter;
	GList 		  *selected = NULL;
	gint		  slides_selected = 0;

	gchar *container[10];
	
	container[0] = "3GPP";
	container[1] = "FLV";
	container[2] = "MPEG-1 Video";
	container[3] = "MPEG-2 Video";
	container[4] = "MPEG-4 Video";
	container[5] = "MPEG-TS";
	container[6] = "Matroska MKV";
	container[7] = "Ogg";
	container[8] = "QuickTime MOV";
	container[9] = "WebM";

	/* Abort if preview is running */
	if( img->preview_is_running )
		return;

	/* Abort if export is running */
	if( img->export_is_running )
		return;

	/* Abort if no slide is present */
	model = GTK_TREE_MODEL( img->thumbnail_model );
	if( ! gtk_tree_model_get_iter_first( model, &iter ) )
		return;

	/* If there are selected slides count their
	 * number to set the combo box later */
	selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(img->thumbnail_iconview));
	if (selected)
	{
		slides_selected = g_list_length(selected);
		g_list_free(selected);
	}
	/* Create dialog */
	dialog = gtk_dialog_new_with_buttons( _("Export Slideshow"), GTK_WINDOW(img->imagination_window),
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  "_Cancel", GTK_RESPONSE_CANCEL,
										  "_Export", GTK_RESPONSE_ACCEPT,
										  NULL );

	//gtk_window_set_default_size(GTK_WINDOW(dialog), 550, -1);
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	video_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), video_frame, TRUE, TRUE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (video_frame), GTK_SHADOW_IN);

	label = gtk_label_new (_("<b>Video Settings</b>"));
	gtk_frame_set_label_widget (GTK_FRAME (video_frame), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

	ex_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(video_frame), ex_vbox);
    gtk_widget_set_halign(GTK_WIDGET(ex_vbox), GTK_ALIGN_FILL);
    gtk_widget_set_margin_top(GTK_WIDGET(ex_vbox), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(ex_vbox), 10);
    gtk_widget_set_margin_start(GTK_WIDGET(ex_vbox), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(ex_vbox), 10);

	export_grid = gtk_grid_new();
	gtk_grid_set_row_homogeneous(GTK_GRID(export_grid), TRUE);
	gtk_grid_set_row_spacing (GTK_GRID(export_grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID(export_grid), 10);
	gtk_box_pack_start (GTK_BOX (ex_vbox), export_grid, TRUE, FALSE, 0);

	label = gtk_label_new( _("Container:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,0,1,1);
	
	container_menu = gtk_combo_box_text_new();
	gtk_grid_attach( GTK_GRID(export_grid), container_menu, 1,0,1,1);

	label = gtk_label_new( _("Codec:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,1,1,1);
	
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	model = GTK_TREE_MODEL(store);
	img->vcodec_menu = gtk_combo_box_new_with_model(model);
	g_object_unref(G_OBJECT(model));
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (img->vcodec_menu), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (img->vcodec_menu), cell,
										"text", 0,
										NULL);
	g_object_set(cell, "ypad", (guint)0, NULL);

	gtk_grid_attach( GTK_GRID(export_grid), img->vcodec_menu, 1,1,1,1);
	g_signal_connect(G_OBJECT (container_menu),"changed",G_CALLBACK (img_container_changed),img);

	label = gtk_label_new( _("Range:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,2,1,1);

	range_menu = gtk_combo_box_text_new();
	gtk_grid_attach( GTK_GRID(export_grid), range_menu, 1,2,1,1);

	label = gtk_label_new( _("Width:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,3,1,1);

	width = gtk_spin_button_new_with_range (1280, 9999, 10);
	gtk_grid_attach( GTK_GRID(export_grid), width, 1,3,1,1);
	
	label = gtk_label_new( _("Height:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,4,1,1);

	height = gtk_spin_button_new_with_range(720, 9999, 10);
	gtk_grid_attach( GTK_GRID(export_grid), height, 1,4,1,1);
	
	label = gtk_label_new( _("Frame Rate:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,5,1,1);

	frame_rate = gtk_spin_button_new_with_range(20, 30, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(frame_rate), 25);
	gtk_grid_attach( GTK_GRID(export_grid), frame_rate, 1,5,1,1);
	
	label = gtk_label_new( _("Quality (CRF):") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,6,1,1);

	quality = gtk_spin_button_new_with_range(20, 36, 1);
	gtk_grid_attach( GTK_GRID(export_grid), quality, 1,6,1,1);

	label = gtk_label_new( _("Filename:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,7,1,1);
	
    slideshow_title_entry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(slideshow_title_entry), GTK_ENTRY_ICON_SECONDARY, "document-open"), 
	g_signal_connect (slideshow_title_entry, "icon-press", G_CALLBACK (img_show_file_chooser), img);
	gtk_grid_attach( GTK_GRID(export_grid), slideshow_title_entry, 1,7,1,1);
	
	/* Fill range combo box */
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(range_menu), NULL, _("All slides"));
	if (slides_selected > 1)
	{
		gchar *string = g_strdup_printf("Selected %d slides",slides_selected);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(range_menu), NULL, string);
		gtk_combo_box_set_active(GTK_COMBO_BOX(range_menu), 1);
		g_free(string);
	}
	else
	{	
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(range_menu), NULL, _("<no slides selected>"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(range_menu), 0);
	}
	
	/* Audio Settings */
	audio_frame = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), audio_frame, TRUE, TRUE, 10);
	gtk_frame_set_shadow_type (GTK_FRAME (audio_frame), GTK_SHADOW_IN);

	/* Disable the whole Audio frame if there
	 * is no music in the project */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(img->music_file_treeview));
	if(gtk_tree_model_get_iter_first(model, &iter) == FALSE)
		gtk_widget_set_sensitive(audio_frame, FALSE);

	label = gtk_label_new (_("<b>Audio Settings</b>"));
	gtk_frame_set_label_widget (GTK_FRAME (audio_frame), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

	ex_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(audio_frame), ex_vbox);
    gtk_widget_set_halign(GTK_WIDGET(ex_vbox), GTK_ALIGN_FILL);
    gtk_widget_set_margin_top(GTK_WIDGET(ex_vbox), 5);
    gtk_widget_set_margin_bottom(GTK_WIDGET(ex_vbox), 5);
    gtk_widget_set_margin_start(GTK_WIDGET(ex_vbox), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(ex_vbox), 10);

	export_grid = gtk_grid_new();
	gtk_grid_set_row_homogeneous(GTK_GRID(export_grid), TRUE);
	gtk_grid_set_row_spacing (GTK_GRID(export_grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID(export_grid), 10);
	gtk_box_pack_start (GTK_BOX (ex_vbox), export_grid, TRUE, FALSE, 0);

	label = gtk_label_new( _("Codec:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,0,1,1);
	
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	model = GTK_TREE_MODEL(store);
	img->acodec_menu = gtk_combo_box_new_with_model(model);
	g_object_unref(G_OBJECT(model));
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (img->acodec_menu), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (img->acodec_menu), cell,
										"text", 0,
										NULL);
	g_object_set(cell, "ypad", (guint)0, NULL);
	gtk_grid_attach( GTK_GRID(export_grid), img->acodec_menu, 1,0,1,1);

	/* Fill container combo box and all the
	 * others connected to it */
	gint i;
	for (i = 0; i <= 9; i++)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(container_menu), NULL, container[i]);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(container_menu), 4);
	
	label = gtk_label_new( _("Sample Rate:") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,1,1,1);

	sample_rate = gtk_spin_button_new_with_range(20000, 44100, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_rate), 44100);
	gtk_grid_attach( GTK_GRID(export_grid), sample_rate, 1,1,1,1);
	
	label = gtk_label_new( _("Bitrate (Kbps/CBR):") );
	gtk_label_set_xalign (GTK_LABEL(label), 0.0);
	gtk_grid_attach( GTK_GRID(export_grid), label, 0,2,1,1);

	bitrate = gtk_spin_button_new_with_range(96, 320, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(bitrate), 256);
	gtk_grid_attach( GTK_GRID(export_grid), bitrate, 1,2,1,1);

	gtk_widget_show_all (dialog );

    /* Run dialog and abort if needed */
	if ( gtk_dialog_run( GTK_DIALOG( dialog ) ) != GTK_RESPONSE_ACCEPT )
	{
		gtk_widget_destroy(dialog);
		return;
	}

    if (gtk_entry_get_text_length (GTK_ENTRY(slideshow_title_entry)) == 0)
    {
        gtk_widget_destroy( dialog );
        return;
    }

    filename = gtk_entry_get_text(GTK_ENTRY(slideshow_title_entry));
	img->export_is_running = 1;

	//~ cmd_line = g_strdup_printf("%s -f image2pipe -vcodec ppm "
				//~ "-i pipe: <#AUDIO#> "
				//~ "-r %s "                /* frame rate */
				//~ "-y "                   /* overwrite output */
                //~ "%s "                   /* ffmpeg option */
                //~ "-s %dx%d "             /* size */
                //~ "%s "                   /* aspect ratio */
                //~ "%s "                   /* Bitrate */
                //~ "%s "
                //~ "\"%s\"",               /*filename */
                 //~ img->encoder_name,
                //~ "",
                //~ "",
                //~ img->video_size[0], img->video_size[1],
                //~ aspect_ratio_cmd,
                //~ bitrate_cmd,
                //~ " -metadata comment=\"Made with Imagination " VERSION "\"",
                //~ filename);
	//~ img->export_cmd_line = cmd_line;

	/* Initiate stage 2 of export - audio processing */
	//g_idle_add( (GSourceFunc)img_prepare_audio, img );

	gtk_widget_destroy( dialog );
}

void img_container_changed (GtkComboBox *combo, img_window_struct *img)
{
	GtkListStore *store = NULL;
	gint x;
	
	x = gtk_combo_box_get_active(combo);
	
	store = GTK_LIST_STORE( gtk_combo_box_get_model(GTK_COMBO_BOX(img->vcodec_menu)));
    gtk_list_store_clear(store);
    
    store = GTK_LIST_STORE( gtk_combo_box_get_model(GTK_COMBO_BOX(img->acodec_menu)));
    gtk_list_store_clear(store);

	switch (x)
	{
		case 0:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG4);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H264);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H265);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AAC);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 1);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 0);
		break;
		
		case 1:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_FLV1);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 0);
		break;
		
		case 2:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG1VIDEO);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP2);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_PCM_S16LE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 2);
		break;

		case 3:
		case 5:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG2VIDEO);
		
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP2);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_PCM_S16LE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 2);
		break;

		case 4:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG4);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H264);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H265);
		
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AAC);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP2);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 1);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 0);
		break;
		
		case 6:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG4);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H264);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H265);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AAC);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_EAC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_FLAC);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP2);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_OPUS);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_PCM_S16LE);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_VORBIS);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 1);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 0);
		break;

		case 7:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_THEORA);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_OPUS);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_VORBIS);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 0);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 1);
		break;
		
		case 8:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_QTRLE);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MPEG4);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H264);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_H265);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_MJPEG);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_PRORES);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AAC);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_AC3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP2);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_MP3);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_PCM_S16LE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 2);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->acodec_menu), 0);
		break;
		
		case 9:
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_VP8);
		img_add_codec_to_container_combo(img->vcodec_menu, AV_CODEC_ID_VP9);

		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_OPUS);
		img_add_codec_to_container_combo(img->acodec_menu, AV_CODEC_ID_VORBIS);
		gtk_combo_box_set_active(GTK_COMBO_BOX(img->vcodec_menu), 0);
		break;
	}
}

void img_add_codec_to_container_combo(GtkWidget *combo, enum AVCodecID codec)
{
	GtkListStore *store;
	GtkTreeIter   iter;
	
	AVCodec *codec_info;
	codec_info = avcodec_find_encoder(codec);

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 0, codec_info->long_name, 1, codec, -1 );
}
