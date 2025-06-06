/**
 * global settings variables
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "globals.h"

#include <QtCore/QDir>

#include <thread>
//#include <limits>


// epsilon for calculations
//t_real g_eps = std::numeric_limits<t_real>::epsilon();
t_real g_eps = 1e-6;

// precision for showing numbers in the gui
t_int g_prec_gui = 4;

// radius for data smoothing
t_int g_smooth_rad = 10;


// number of calculation threads
t_int g_num_threads = std::max<unsigned int>(std::thread::hardware_concurrency() / 2, 1);


bool g_use_recent_dir = true;
bool g_reload_last = true;

// show UTF8 icons
bool g_show_icons = true;


// distance calculation function index
int g_dist_func = 0;

// minimum height difference [m] before being counted as climb
t_real g_asc_eps = 5.;


// assumed time interval if non is given (for import)
t_real g_assume_dt = 2.;


// xml map options
t_real g_map_scale = 1.;
t_real g_map_overdraw = 0.1;
bool g_map_show_buildings = false;
bool g_map_show_labels = false;


// directory for temporary files
QString g_temp_dir = QDir::tempPath() + QDir::separator() + "tracks";
