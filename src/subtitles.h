/*
** Copyright (C) 2009 Tadej Borovšak <tadeboro@gmail.com>
** Copyright (C) 2010 Robert Chéramy <robert@cheramy.net>
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

#ifndef __SUBTITLES_H__
#define __SUBTITLES_H__

#include "imagination.h"


typedef struct _TextAnimation TextAnimation;
struct _TextAnimation
{
	gchar             *name; /* Name of animation */
	TextAnimationFunc  func; /* Actual renderer */
	gint               id;   /* Unique id (for save and load operations) */
};


gint
img_get_text_animation_list( TextAnimation **animations );

void
img_free_text_animation_list( gint           no_animations,
							  TextAnimation *animations );

void
img_render_subtitle( img_window_struct	  *img,
					 cairo_t              *cr,
					 gint                  width,
					 gint                  height,
					 gdouble               zoom,
					 gint					posx,
					 gint					posy,
					 gint					angle,
					 ImgRelPlacing         placing,
					 gdouble               factor,
					 gdouble               offx,
					 gdouble               offy,
					 gchar                *subtitle,
					 gchar                *pattern_filename,
					 PangoFontDescription *font_desc,
					 gdouble              *font_color,
					 gdouble              *font_brdr_color,
                     gdouble              *font_bg_color,
                     gdouble              *border_color,
                     gint	              border_width,
					 TextAnimationFunc     func,
					 gdouble               progress );

void
img_set_slide_text_info( slide_struct      *slide,
						 GtkListStore      *store,
						 GtkTreeIter       *iter,
						 const gchar       *subtitle,
						 gchar		       *pattern_filename,
						 gint	            anim_id,
						 gint               anim_duration,
						 gint               posx,
						 gint               posy,
						 gint               angle,
						 gint               placing,
						 const gchar       *font_desc,
						 gdouble           *font_color,
                         gdouble           *font_brdr_color,
                         gdouble           *font_bgcolor,
                         gdouble           *border_color,
                         gint	           border_width,
						 img_window_struct *img );

#endif
