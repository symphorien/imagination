/*
 *  Copyright (c) 2009-2020 Giuseppe Torelli <colossus73@gmail.com>
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

#ifndef __IMAGINATION_AUDIO_H
#define __IMAGINATION_AUDIO_H

#include <gtk/gtk.h>

#include "support.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

gchar *img_get_audio_length(img_window_struct *, gchar *, gint *);
void img_play_stop_selected_file(GtkButton *, img_window_struct *);
#endif
