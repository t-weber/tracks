/**
 * global settings variables
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_GLOBALS_H__
#define __TRACKS_GLOBALS_H__


#include "common/types.h"
#include "lib/trackdb.h"
#include "lib/map.h"

#include <QtCore/QString>


// track types
using t_tracks = MultipleTracks<t_real, t_size>;
using t_track = typename t_tracks::t_track;
using t_track_pt = typename t_track::t_trackpt;


// map types
using t_map = Map<t_real_map, t_size_map>;


// epsilon and precision values
extern t_real g_eps;
extern t_int g_prec_gui;

// radius for data smoothing
extern t_int g_smooth_rad;


// use recently used directory or a special document folder
extern bool g_use_recent_dir;
extern bool g_reload_last;

// show UTF8 icons
extern bool g_show_icons;


// distance calculation function index
extern int g_dist_func;

// minimum height difference [m] before being counted as climb
extern t_real g_asc_eps;


// assumed time interval if non is given (for import)
extern t_real g_assume_dt;


// xml map options
extern t_real g_map_scale;
extern t_real g_map_overdraw;
extern bool g_map_show_buildings;
extern bool g_map_show_labels;


// directory for temporary files
extern QString g_temp_dir;


#endif
