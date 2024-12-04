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


// track types
using t_tracks = MultipleTracks<t_real, t_size>;
using t_track = typename t_tracks::t_track;
using t_track_pt = typename t_track::t_TrackPoint;


// epsilon and precision values
extern t_real g_eps;
extern t_int g_prec_gui;


// use recently used directory or a special document folder
extern bool g_use_recent_dir;
extern bool g_reload_last;


// distance calculation function index
extern int g_dist_func;


#endif
