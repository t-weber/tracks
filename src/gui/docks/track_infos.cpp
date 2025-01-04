/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_infos.h"
#include "lib/calc.h"
#include "common/version.h"
#include "../helpers.h"
namespace fs = __map_fs;

#include <QtCore/QDir>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QMessageBox>
#include <QtSvg/QSvgRenderer>

#include <sstream>
#include <iomanip>
#include <cmath>
#include <optional>
#include <limits>
#include <algorithm>
#include <numbers>
namespace num = std::numbers;


#define TAB_TRACK  0
#define TAB_ALT    1
#define TAB_PACE   2
#define TAB_MAP    3


/**
 * setup tabs
 */
TrackInfos::TrackInfos(QWidget* parent) : QWidget{parent}
{
	m_tab = std::make_shared<QTabWidget>(this);
	QWidget *plot_panel = new QWidget(m_tab.get());
	QWidget *alt_panel = new QWidget(m_tab.get());
	QWidget *pace_panel = new QWidget(m_tab.get());
	QWidget *map_panel = new QWidget(m_tab.get());
	m_tab->addTab(plot_panel, "Track");
	m_tab->addTab(alt_panel, "Altitude");
	m_tab->addTab(pace_panel, "Pace");
	m_tab->addTab(map_panel, "Map");
#ifndef _TRACKS_USE_OSMIUM_
	m_tab->setTabEnabled(TAB_MAP, false);
	map_panel->setEnabled(false);
#endif

	// track plot panel
	m_track_plot = std::make_shared<QCustomPlot>(plot_panel);
	m_track_plot->setSelectionRectMode(QCP::srmZoom);
	m_track_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_track_plot->xAxis->setLabel("Longitude (Degrees)");
	m_track_plot->yAxis->setLabel("Latitude (Degrees)");
	m_track_plot->legend->setVisible(false);
	connect(m_track_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::TrackPlotMouseMove);
	connect(m_track_plot.get(), &QCustomPlot::mousePress, [this](QMouseEvent *evt)
	{
		PlotMouseClick(evt, m_track_context.get(), m_track_plot.get());
	});

	m_track_context = std::make_shared<QMenu>(plot_panel);
	QIcon iconSaveTrackPdf = QIcon::fromTheme("image-x-generic");
	QAction *actionSaveTrackPdf = new QAction(iconSaveTrackPdf, "Save Image...", m_track_context.get());
	m_track_context->addAction(actionSaveTrackPdf);
	connect(actionSaveTrackPdf, &QAction::triggered, [this]()
	{
		SavePlotPdf(m_track_plot.get(), "track");
	});

	m_same_range = std::make_shared<QCheckBox>(plot_panel);
	m_same_range->setText("Keep Aspect");
	m_same_range->setToolTip("Correct the angular range aspect ratio for longitudes and latitudes.");
	m_same_range->setChecked(true);
	connect(m_same_range.get(), &QAbstractButton::toggled, this, &TrackInfos::ResetTrackPlotRange);

	QPushButton *btn_replot = new QPushButton(plot_panel);
	btn_replot->setText("Reset Plot");
	btn_replot->setToolTip("Reset the range of the longitude and latitude to show all track data.");
	connect(btn_replot, &QAbstractButton::clicked, this, &TrackInfos::ResetTrackPlotRange);

	QGridLayout *plot_panel_layout = new QGridLayout(plot_panel);
	plot_panel_layout->setContentsMargins(4, 4, 4, 4);
	plot_panel_layout->setVerticalSpacing(4);
	plot_panel_layout->setHorizontalSpacing(4);
	plot_panel_layout->addWidget(m_track_plot.get(), 0, 0, 1, 4);
	plot_panel_layout->addWidget(m_same_range.get(), 1, 0, 1, 1);
	plot_panel_layout->addWidget(btn_replot, 1, 3, 1, 1);

	// altitude plot panel
	m_alt_plot = std::make_shared<QCustomPlot>(alt_panel);
	m_alt_plot->setSelectionRectMode(QCP::srmZoom);
	m_alt_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_alt_plot->yAxis->setLabel("Altitude (m)");
	m_alt_plot->legend->setVisible(false);
	connect(m_alt_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::AltPlotMouseMove);
	connect(m_alt_plot.get(), &QCustomPlot::mousePress, [this](QMouseEvent *evt)
	{
		PlotMouseClick(evt, m_alt_context.get(), m_alt_plot.get());
	});

	m_alt_context = std::make_shared<QMenu>(alt_panel);
	QIcon iconSaveAltPdf = QIcon::fromTheme("image-x-generic");
	QAction *actionSaveAltPdf = new QAction(iconSaveAltPdf, "Save Image...", m_alt_context.get());
	m_alt_context->addAction(actionSaveAltPdf);
	connect(actionSaveAltPdf, &QAction::triggered, [this]()
	{
		SavePlotPdf(m_alt_plot.get(), "altitude");
	});

	m_time_check = std::make_shared<QCheckBox>(alt_panel);
	m_time_check->setText("Plot Times");
	m_time_check->setToolTip("Show elapsed time instead of distance.");
	m_time_check->setChecked(false);
	connect(m_time_check.get(), &QCheckBox::toggled, this, &TrackInfos::PlotAlt);

	QPushButton *btn_replot_alt = new QPushButton(alt_panel);
	btn_replot_alt->setText("Reset Plot");
	btn_replot_alt->setToolTip("Reset the plotting range.");
	connect(btn_replot_alt, &QAbstractButton::clicked, this, &TrackInfos::ResetAltPlotRange);

	QGridLayout *alt_panel_layout = new QGridLayout(alt_panel);
	alt_panel_layout->setContentsMargins(4, 4, 4, 4);
	alt_panel_layout->setVerticalSpacing(4);
	alt_panel_layout->setHorizontalSpacing(4);
	alt_panel_layout->addWidget(m_alt_plot.get(), 0, 0, 1, 4);
	alt_panel_layout->addWidget(m_time_check.get(), 1, 0, 1, 1);
	alt_panel_layout->addWidget(btn_replot_alt, 1, 3, 1, 1);

	// pace plot panel
	m_pace_plot = std::make_shared<QCustomPlot>(pace_panel);
	m_pace_plot->setSelectionRectMode(QCP::srmZoom);
	m_pace_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_pace_plot->xAxis->setLabel("Distance (km)");
	m_pace_plot->legend->setVisible(true);
	connect(m_pace_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::PacePlotMouseMove);
	connect(m_pace_plot.get(), &QCustomPlot::mousePress, [this](QMouseEvent *evt)
	{
		PlotMouseClick(evt, m_pace_context.get(), m_pace_plot.get());
	});

	m_pace_context = std::make_shared<QMenu>(pace_panel);
	QIcon iconSavePacePdf = QIcon::fromTheme("image-x-generic");
	QAction *actionSavePacePdf = new QAction(iconSavePacePdf, "Save Image...", m_pace_context.get());
	m_pace_context->addAction(actionSavePacePdf);
	connect(actionSavePacePdf, &QAction::triggered, [this]()
	{
		SavePlotPdf(m_pace_plot.get(), "pace");
	});

	m_speed_check = std::make_shared<QCheckBox>(pace_panel);
	m_speed_check->setText("Plot Speeds");
	m_speed_check->setToolTip("Show speeds instead of paces.");
	m_speed_check->setChecked(false);
	connect(m_speed_check.get(), &QCheckBox::toggled, this, &TrackInfos::PlotPace);

	m_dist_binlen = std::make_shared<QDoubleSpinBox>(pace_panel);
	m_dist_binlen->setDecimals(2);
	m_dist_binlen->setValue(0.25);
	m_dist_binlen->setMinimum(0.01);
	m_dist_binlen->setMaximum(9.);
	m_dist_binlen->setSingleStep(0.05);
	m_dist_binlen->setSuffix(" km");
	connect(m_dist_binlen.get(),
		static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
		this, &TrackInfos::PlotPace);

	QPushButton *btn_replot_pace = new QPushButton(pace_panel);
	btn_replot_pace->setText("Reset Plot");
	btn_replot_pace->setToolTip("Reset the plotting range.");
	connect(btn_replot_pace, &QAbstractButton::clicked, this, &TrackInfos::ResetPacePlotRange);

	QGridLayout *pace_panel_layout = new QGridLayout(pace_panel);
	pace_panel_layout->setContentsMargins(4, 4, 4, 4);
	pace_panel_layout->setVerticalSpacing(4);
	pace_panel_layout->setHorizontalSpacing(4);
	pace_panel_layout->addWidget(m_pace_plot.get(), 0, 0, 1, 4);
	pace_panel_layout->addWidget(m_speed_check.get(), 1, 0, 1, 1);
	pace_panel_layout->addWidget(new QLabel("Distance Binning:", pace_panel), 1, 1, 1, 1);
	pace_panel_layout->addWidget(m_dist_binlen.get(), 1, 2, 1, 1);
	pace_panel_layout->addWidget(btn_replot_pace, 1, 3, 1, 1);

	// map plot panel
	m_map = std::make_shared<MapDrawer>(map_panel);
	//m_map->renderer()->setAnimationEnabled(false);
	m_map->renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
	connect(m_map.get(), &MapDrawer::MouseMoved, this, &TrackInfos::MapMouseMove);
	connect(m_map.get(), &MapDrawer::MousePressed, [this](QMouseEvent *evt)
	{
		PlotMouseClick(evt, m_map_context.get(), m_map.get());
	});

	m_map_context = std::make_shared<QMenu>(map_panel);
	QIcon iconSaveSvg = QIcon::fromTheme("image-x-generic");
	QAction *actionSaveSvg = new QAction(iconSaveSvg, "Save Image...", m_map_context.get());
	connect(actionSaveSvg, &QAction::triggered, this, &TrackInfos::SaveMapSvg);
	m_map_context->addAction(actionSaveSvg);

	m_mapfile = std::make_shared<QLineEdit>(map_panel);
	m_mapfile->setPlaceholderText("Directory with Map Files (.osm.pbf)");
	m_mapfile->setToolTip("Directory containing map files.");

	QPushButton *btn_browse_map = new QPushButton(map_panel);
	btn_browse_map->setText("Select Map Dir...");
	btn_browse_map->setToolTip("Select a directory with map files (.osm or .osm.pbf).");
	connect(btn_browse_map, &QAbstractButton::clicked, this, &TrackInfos::SelectMap);

	QPushButton *btn_calc_map = new QPushButton(map_panel);
	btn_calc_map->setText("Calculate Map");
	btn_calc_map->setToolTip("(Re-)Calculate the map for the current track.");
	connect(btn_calc_map, &QAbstractButton::clicked, [this]()
	{
		PlotMap(false);
	});

	QGridLayout *map_panel_layout = new QGridLayout(map_panel);
	map_panel_layout->setContentsMargins(4, 4, 4, 4);
	map_panel_layout->setVerticalSpacing(4);
	map_panel_layout->setHorizontalSpacing(4);
	map_panel_layout->addWidget(m_map.get(), 0, 0, 1, 4);
	map_panel_layout->addWidget(m_mapfile.get(), 1, 0, 1, 2);
	map_panel_layout->addWidget(btn_browse_map, 1, 2, 1, 1);
	map_panel_layout->addWidget(btn_calc_map, 1, 3, 1, 1);

	// text infos
	m_infos = std::make_shared<QTextEdit>(this);
	m_infos->setReadOnly(true);

	m_split = std::make_shared<QSplitter>(this);
	m_split->setOrientation(Qt::Vertical);
	m_split->addWidget(m_tab.get());
	m_split->addWidget(m_infos.get());
	m_split->setStretchFactor(0, 10);
	m_split->setStretchFactor(1, 1);

	QGridLayout *main_layout = new QGridLayout(this);
	main_layout->setContentsMargins(4, 4, 4, 4);
	main_layout->setVerticalSpacing(4);
	main_layout->setHorizontalSpacing(4);
	main_layout->addWidget(m_split.get(), 0, 0, 1, 1);
}


TrackInfos::~TrackInfos()
{
}


/**
 * sets the current track and shows it in the plotter
 */
void TrackInfos::ShowTrack(const t_track& track)
{
	m_track = &track;

	// print track infos
	m_infos->setHtml(track.PrintHtml(g_prec_gui).c_str());

	PlotTrack();
	PlotPace();
	PlotAlt();
	PlotMap(true);
}


void TrackInfos::CalcTrackPlotRange()
{
	m_min_long_plot = m_min_long;
	m_max_long_plot = m_max_long;
	m_min_lat_plot = m_min_lat;
	m_max_lat_plot = m_max_lat;

	if(!m_track_plot)
		return;

	if(m_same_range && m_same_range->isChecked())
	{
		int h = 0, w = 0;

		if(m_tab->currentIndex() == TAB_TRACK
			&& m_track_plot->isVisible())  // track
		{
			h = m_track_plot->viewport().height();
			w = m_track_plot->viewport().width();
		}
		else if(m_tab->currentIndex() == TAB_MAP
			&& m_map->isVisible())         // map
		{
			h = m_map->height();
			w = m_map->width();
		}

		if(h <= 0 || w <= 0)
		{
			h = 3;
			w = 4;
		}

		t_real aspect = t_real(h) / t_real(w);

		t_real range_long = m_max_long - m_min_long;
		t_real range_lat = m_max_lat - m_min_lat;

		if(range_long*aspect > range_lat)
		{
			t_real mid_lat = m_min_lat_plot + range_lat * 0.5;

			range_lat = range_long * aspect;
			m_min_lat_plot = mid_lat - range_lat * 0.5;
			m_max_lat_plot = mid_lat + range_lat * 0.5;
		}
		else if(range_long*aspect < range_lat)
		{
			t_real mid_long = m_min_long_plot + range_long * 0.5;

			range_long = range_lat / aspect;
			m_min_long_plot = mid_long - range_long * 0.5;
			m_max_long_plot = mid_long + range_long * 0.5;
		}
	}
}


void TrackInfos::ResetTrackPlotRange()
{
	CalcTrackPlotRange();

	if(!m_track_plot || !m_same_range)
		return;

	t_real range_long = m_max_long_plot - m_min_long_plot;
	t_real range_lat = m_max_lat_plot - m_min_lat_plot;

	m_track_plot->xAxis->setRange(
		(m_min_long_plot - range_long / 20.) * t_real(180) / num::pi_v<t_real>,
		(m_max_long_plot + range_long / 20.) * t_real(180) / num::pi_v<t_real>);

	m_track_plot->yAxis->setRange(
		(m_min_lat_plot - range_lat / 20.) * t_real(180) / num::pi_v<t_real>,
		(m_max_lat_plot + range_lat / 20.) * t_real(180) / num::pi_v<t_real>);

	m_track_plot->replot();
}


void TrackInfos::PlotTrack()
{
	if(!m_track)
		return;

	// prepare track plot
	std::tie(m_min_lat, m_max_lat) = m_track->GetLatitudeRange();
	std::tie(m_min_long, m_max_long) = m_track->GetLongitudeRange();
	CalcTrackPlotRange();

	// plot track
	if(!m_track_plot)
		return;
	QVector<t_real> latitudes, longitudes;
	latitudes.reserve(m_track->GetPoints().size());
	longitudes.reserve(m_track->GetPoints().size());

	for(const t_track_pt& pt : m_track->GetPoints())
	{
		latitudes.push_back(pt.latitude * t_real(180) / num::pi_v<t_real>);
		longitudes.push_back(pt.longitude * t_real(180) / num::pi_v<t_real>);
	}

	m_track_plot->clearPlottables();

	QCPCurve *curve = new QCPCurve(m_track_plot->xAxis, m_track_plot->yAxis);
	curve->setData(longitudes, latitudes);
	//curve->setLineStyle(QCPCurve::lsLine);

	QPen pen = curve->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0x00, 0x00, 0xff, 0xff});
	curve->setPen(pen);

	ResetTrackPlotRange();
}


void TrackInfos::ResetAltPlotRange()
{
	if(!m_alt_plot)
		return;

	t_real alt_range = m_max_alt - m_min_alt;

	m_alt_plot->xAxis->setRange(m_min_dist_alt, m_max_dist_alt);
	m_alt_plot->yAxis->setRange(
		m_min_alt - alt_range / 20.,
		m_max_alt + alt_range / 20.);

	m_alt_plot->replot();
}


void TrackInfos::PlotAlt()
{
	if(!m_track || !m_alt_plot)
		return;

	m_alt_plot->clearPlottables();

	const bool plot_time = m_time_check->isChecked();
	if(plot_time)
		m_alt_plot->xAxis->setLabel("Time (min)");
	else
		m_alt_plot->xAxis->setLabel("Distance (km)");

	QVector<t_real> dist, alt;

	dist.reserve(m_track->GetPoints().size());
	alt.reserve(m_track->GetPoints().size());

	m_min_alt = std::numeric_limits<t_real>::max();
	m_max_alt = -m_min_alt;

	for(const t_track_pt& pt : m_track->GetPoints())
	{
		if(plot_time)
			dist.push_back(pt.elapsed_total / 60.);
		else
			dist.push_back(pt.distance_total / 1000.);
		alt.push_back(pt.elevation);

		m_min_alt = std::min(m_min_alt, pt.elevation);
		m_max_alt = std::max(m_max_alt, pt.elevation);
	}

	if(!alt.size())
		return;

	if(plot_time)
	{
		m_min_dist_alt = *dist.begin();
		m_max_dist_alt = *dist.rbegin() * 1.01;
	}
	else
	{
		// use same distance range as in pace plot
		m_min_dist_alt = m_min_dist;
		m_max_dist_alt = m_max_dist;
	}

	QCPGraph *curve = new QCPGraph(m_alt_plot->xAxis, m_alt_plot->yAxis);
	curve->setData(dist, alt);
	//curve->setLineStyle(QCPCurve::lsLine);

	QPen pen = curve->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0x00, 0x00, 0xff, 0xff});
	curve->setPen(pen);

	ResetAltPlotRange();
}


void TrackInfos::ResetPacePlotRange()
{
	if(!m_pace_plot)
		return;

	t_real pace_range = m_max_pace - m_min_pace;

	m_pace_plot->xAxis->setRange(m_min_dist, m_max_dist);
	m_pace_plot->yAxis->setRange(
		m_min_pace - pace_range / 20.,
		m_max_pace + pace_range / 20.);

	m_pace_plot->replot();
}


void TrackInfos::PlotPace()
{
	if(!m_track || !m_pace_plot)
		return;

	m_pace_plot->clearPlottables();

	const t_real dist_bin = m_dist_binlen->value() * 1000.;
	const bool plot_speed = m_speed_check->isChecked();
	if(plot_speed)
		m_pace_plot->yAxis->setLabel("Speed (km/h)");
	else
		m_pace_plot->yAxis->setLabel("Pace (min/km)");

	auto [times, dists] = m_track->GetTimePerDistance<QVector>(dist_bin, false);
	auto [times_planar, dists_planar] = m_track->GetTimePerDistance<QVector>(dist_bin, true);
	if(!times.size() || times.size() != dists.size()
		|| !times_planar.size() || times_planar.size() != dists_planar.size())
		return;

	for(int i = 0; i < times.size(); ++i)
	{
		times[i] /= 60. * (dist_bin / 1000.);  // to [min/km]
		dists[i] -= dist_bin / 2.;             // centre histogram bar
		dists[i] /= 1000.;                     // to [km]

		if(plot_speed)
			times[i] = speed_to_pace<t_real>(times[i]);
	}

	for(int i = 0; i < times_planar.size(); ++i)
	{
		times_planar[i] /= 60. * (dist_bin / 1000.);  // to [min/km]
		dists_planar[i] -= dist_bin / 2.;             // centre histogram bar
		dists_planar[i] /= 1000.;                     // to [km]

		if(plot_speed)
			times_planar[i] = speed_to_pace<t_real>(times_planar[i]);
	}

	auto [min_dist_iter, max_dist_iter] = std::minmax_element(dists.begin(), dists.end());
	auto [min_time_iter, max_time_iter] = std::minmax_element(times.begin(), times.end());
	auto [min_dist_pl_iter, max_dist_pl_iter] = std::minmax_element(dists_planar.begin(), dists_planar.end());
	auto [min_time_pl_iter, max_time_pl_iter] = std::minmax_element(times_planar.begin(), times_planar.end());

	m_min_dist = 0.; //std::min(*min_dist_iter, *min_dist_pl_iter);
	m_max_dist = std::max(*max_dist_iter, *max_dist_pl_iter) + dist_bin / 2000.;
	m_min_pace = std::min(*min_time_iter, *min_time_pl_iter);
	m_max_pace = std::max(*max_time_iter, *max_time_pl_iter);

	QCPBars *curve_planar = new QCPBars(m_pace_plot->xAxis, m_pace_plot->yAxis);
	curve_planar->setWidth(dist_bin / 1000.);
	curve_planar->setData(dists_planar, times_planar);
	curve_planar->setName("Planar");

	QPen pen_planar = curve_planar->pen();
	pen_planar.setWidthF(2.);
	pen_planar.setColor(QColor{0xff, 0, 0, 0xff});
	curve_planar->setPen(pen_planar);

	QBrush brush_planar = curve_planar->brush();
	brush_planar.setStyle(Qt::SolidPattern);
	brush_planar.setColor(QColor{0xff, 0, 0, 0x99});
	curve_planar->setBrush(brush_planar);

	//QCPCurve *curve = new QCPCurve(m_pace_plot->xAxis, m_pace_plot->yAxis);
	QCPBars *curve = new QCPBars(m_pace_plot->xAxis, m_pace_plot->yAxis);
	curve->setWidth(dist_bin / 1000.);
	curve->setData(dists, times);
	curve->setName("Non-Planar");
	//curve->setLineStyle(QCPCurve::lsLine);

	QPen pen = curve->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0, 0, 0xff, 0xff});
	curve->setPen(pen);

	QBrush brush = curve->brush();
	brush.setStyle(Qt::SolidPattern);
	brush.setColor(QColor{0, 0, 0xff, 0x99});
	curve->setBrush(brush);

	ResetPacePlotRange();
}


/**
 * browse for map directories (or files)
 */
void TrackInfos::SelectMap()
{
	auto filedlg = std::make_shared<QFileDialog>(
		this, "Select Map Directory", m_mapdir.c_str(),
		"Map Files (*.osm *.pbf);;All Files (* *.*)");
	filedlg->setAcceptMode(QFileDialog::AcceptOpen);
	//filedlg->setDefaultSuffix(".pbf");
	filedlg->setOptions(QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);
	filedlg->setFileMode(/*QFileDialog::ExistingFile*/ QFileDialog::Directory);

	if(!filedlg->exec())
		return;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return;

	m_mapfile->setText(files[0]);
	m_mapdir = files[0].toStdString();
}


/**
 * render an image of the map corresponding to the current track
 */
void TrackInfos::PlotMap(bool load_cached)
{
#ifndef _TRACKS_USE_OSMIUM_
	return;
#endif

	if(!m_map)
		return;

	// clear map
	m_map_image.clear();
	m_map->load(m_map_image);

	if(m_mapfile->text() == "")
	{
		emit StatusMessageChanged("No source maps available. Please specify a map directory with .osm.pbf files.");
		return;
	}

	t_real lon_range = m_max_long_plot - m_min_long_plot;
	t_real lat_range = m_max_lat_plot - m_min_lat_plot;

	// track
	std::vector<typename t_map::t_vertex> thetrack;
	std::optional<std::string> cached_map_name;
	if(m_track)
	{
		thetrack.reserve(m_track->GetPoints().size());
		for(const t_track_pt& pt : m_track->GetPoints())
		{
			typename t_map::t_vertex vert
			{
				.longitude = static_cast<t_real_map>(pt.longitude),
				.latitude = static_cast<t_real_map>(pt.latitude),
			};

			thetrack.emplace_back(std::move(vert));
		}

		t_size track_hash = m_track->GetHash();
		std::ostringstream ostr_cached;
		if(g_temp_dir != "")
		{
			ostr_cached << g_temp_dir.toStdString();
			ostr_cached << QString{QDir::separator()}.toStdString();
		}
		ostr_cached << std::hex << track_hash << ".trackmap";
		cached_map_name = ostr_cached.str();
	}

	// map loading progress
	QProgressDialog progress_dlg{this};
	progress_dlg.setMinimumDuration(500);
	progress_dlg.setAutoClose(true);
	progress_dlg.setWindowModality(Qt::WindowModal);
	progress_dlg.setWindowTitle(TRACKS_TITLE);
	progress_dlg.setLabelText("Calculating map...");

	// map loading progress callback
	std::function<bool(t_size_map, t_size_map)> progress = [&progress_dlg](
		t_size_map offs, t_size_map size) -> bool
	{
		progress_dlg.setRange(0, static_cast<int>(size));
		progress_dlg.setValue(static_cast<int>(offs));

		return !progress_dlg.wasCanceled();
	};

	// map
	t_map map;
	map.SetSkipBuildings(!g_map_show_buildings);
	map.SetSkipLabels(!g_map_show_labels);
	map.SetTrack(std::move(thetrack));

	bool map_loaded = false;

	// try to load a cached map
	if(load_cached)
		map_loaded = map.Load(*cached_map_name);

	// else generate a new map
	else if(!load_cached && !map_loaded)
	{
		// cut out a map that has some margins around the actual data area
		map_loaded = map.ImportDir(m_mapfile->text().toStdString(),
			static_cast<t_real_map>(m_min_long_plot - lon_range*g_map_overdraw),
			static_cast<t_real_map>(m_max_long_plot + lon_range*g_map_overdraw),
			static_cast<t_real_map>(m_min_lat_plot - lat_range*g_map_overdraw),
			static_cast<t_real_map>(m_max_lat_plot + lat_range*g_map_overdraw),
			&progress);

		if(map_loaded && cached_map_name)
		{
			// cache generated map
			map.Save(*cached_map_name);
		}

		if(!map_loaded)
		{
			progress_dlg.cancel();
			QMessageBox::critical(this, "Error",
				"No suitable source maps could be found. "
				"Please provide .osm.pbf files.");
		}
	}

	if(map_loaded)
	{
		// plot the map data area as svg image
		std::ostringstream ostr;
		map.ExportSvg(ostr, static_cast<t_real_map>(g_map_scale),
			static_cast<t_real_map>(m_min_long_plot - lon_range*g_map_overdraw/2.),
			static_cast<t_real_map>(m_max_long_plot + lon_range*g_map_overdraw/2.),
			static_cast<t_real_map>(m_min_lat_plot - lat_range*g_map_overdraw/2.),
			static_cast<t_real_map>(m_max_lat_plot + lat_range*g_map_overdraw/2.));

		// load the generated svg image
		m_map_image = QByteArray{ostr.str().c_str(), static_cast<int>(ostr.str().size())};
		m_map->load(m_map_image);
	}

	if(map_loaded)
		emit StatusMessageChanged("Map successfully loaded.");
	else if(!load_cached)
		emit StatusMessageChanged("Error: No map could be generated.");
	else
		emit StatusMessageChanged("No map available for this track.");


	/*MapPlotter map;
	if(map.Import(m_mapfile->text().toStdString(),
		m_min_long_plot, m_max_long_plot,
		m_min_lat_plot, m_max_lat_plot))
	{
		map.SetPlotRange(
			m_min_long_plot - lon_range*g_map_overdraw
			m_max_long_plot + lon_range*g_map_overdraw,
			m_min_lat_plot - lat_range*g_map_overdraw,
			m_max_lat_plot + lat_range*g_map_overdraw);
		map.Plot(m_track_plot);
	}*/
}


/**
 * save a plot as a pdf image
 */
void TrackInfos::SavePlotPdf(QCustomPlot *plot, const char *name)
{
	if(!m_track)
	{
		QMessageBox::warning(this, "Warning", "No track is loaded.");
		return;
	}

	auto filedlg = std::make_shared<QFileDialog>(
		this, "Save Plot as Image", m_svgdir.c_str(),
		"PDF Files (*.pdf)");
	filedlg->setAcceptMode(QFileDialog::AcceptSave);
	filedlg->setDefaultSuffix("pdf");
	filedlg->selectFile(name);
	filedlg->setFileMode(QFileDialog::AnyFile);

	if(!filedlg->exec())
		return;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return;

	if(!plot->savePdf(files[0]))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be saved.").arg(files[0]));
		return;
	}

	fs::path file{files[0].toStdString()};
	m_svgdir = file.parent_path().string();
}


/**
 * save the map as an svg image
 */
void TrackInfos::SaveMapSvg()
{
	if(!m_map || m_map_image.size() == 0)
	{
		QMessageBox::warning(this, "Warning", "No map is loaded.");
		return;
	}

	auto filedlg = std::make_shared<QFileDialog>(
		this, "Save Map as Image", m_svgdir.c_str(),
		"SVG Files (*.svg)");
	filedlg->setAcceptMode(QFileDialog::AcceptSave);
	filedlg->setDefaultSuffix("svg");
	filedlg->selectFile("map");
	filedlg->setFileMode(QFileDialog::AnyFile);

	if(!filedlg->exec())
		return;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return;

	std::ofstream ofstr{files[0].toStdString()};
	if(!ofstr)
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be saved.").arg(files[0]));
		return;
	}
	ofstr.write(m_map_image.data(), m_map_image.size());

	fs::path file{files[0].toStdString()};
	m_svgdir = file.parent_path().string();
}


void TrackInfos::Clear()
{
	m_track = nullptr;
	m_infos->clear();

	if(m_track_plot)
	{
		m_track_plot->clearPlottables();
		m_track_plot->replot();
	}

	if(m_alt_plot)
	{
		m_alt_plot->clearPlottables();
		m_alt_plot->replot();
	}

	if(m_pace_plot)
	{
		m_pace_plot->clearPlottables();
		m_pace_plot->replot();
	}

	if(m_map)
	{
		m_map_image.clear();
		m_map->load(m_map_image);
	}
}


/**
 * the mouse has moved in the track plot widget
 */
void TrackInfos::TrackPlotMouseMove(QMouseEvent *evt)
{
	if(!m_track_plot)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif

	t_real longitude = m_track_plot->xAxis->pixelToCoord(x);
	t_real latitude = m_track_plot->yAxis->pixelToCoord(y);

	emit PlotCoordsChanged(longitude, latitude);
}


/**
 * the mouse has moved in the pace plot widget
 */
void TrackInfos::PacePlotMouseMove(QMouseEvent *evt)
{
        if(!m_pace_plot)
                return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif

	t_real dist = m_pace_plot->xAxis->pixelToCoord(x);
	t_real pace = m_pace_plot->yAxis->pixelToCoord(y);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << "Distance: " << dist << " km";
	if(m_speed_check->isChecked())
		ostr << ", Speed: " << pace << " km/h.";
	else
		ostr << ", Pace: " << get_pace_str(pace) << ".";

	emit StatusMessageChanged(ostr.str().c_str());
}


/**
 * the mouse has moved in the altitude plot widget
 */
void TrackInfos::AltPlotMouseMove(QMouseEvent *evt)
{
        if(!m_alt_plot)
                return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif

	t_real dist = m_pace_plot->xAxis->pixelToCoord(x);
	t_real alt = m_pace_plot->yAxis->pixelToCoord(y);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);

	if(m_time_check->isChecked())
		ostr << "Time: " << dist << " min";
	else
		ostr << "Distance: " << dist << " km";
	ostr << ", Altitude: " << alt << " m.";

	emit StatusMessageChanged(ostr.str().c_str());
}


/**
 * the mouse has moved in the map widget
 */
void TrackInfos::MapMouseMove(QMouseEvent *evt)
{
	if(!m_map)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif

	y = m_map->height() - y - 1.;

	x /= t_real(m_map->width() - 1);
	y /= t_real(m_map->height() - 1);

	// map into the plot range given in the call to ExportSvg
	t_real lon_range = m_max_long_plot - m_min_long_plot;
	t_real lat_range = m_max_lat_plot - m_min_lat_plot;

	t_real lon = std::lerp(m_min_long_plot - lon_range/20., m_max_long_plot + lon_range/20., x);
	t_real lat = std::lerp(m_min_lat_plot - lat_range/20., m_max_lat_plot + lat_range/20., y);

	lon *= t_real(180) / num::pi_v<t_real>;
	lat *= t_real(180) / num::pi_v<t_real>;

	emit PlotCoordsChanged(lon, lat);
}


/**
 * the mouse has been clicked in one of the plot widgets
 */
void TrackInfos::PlotMouseClick(QMouseEvent *evt, QMenu *context, QWidget *plot)
{
	if(!plot || !context)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QPoint pos = evt->position();
#else
	QPoint pos = evt->pos();
#endif

	if(evt->buttons() & Qt::RightButton)
	{
		QPoint posGlobal = plot->mapToGlobal(pos);
		posGlobal.rx() += 4;
		posGlobal.ry() += 4;

		context->popup(posGlobal);
	}
}


QSize TrackInfos::sizeHint() const
{
	QSize size = QWidget::sizeHint();
	size.rwidth() += 128;
	return size;
}


void TrackInfos::SaveSettings(QSettings& settings)
{
	QByteArray split{m_split->saveState()};
	settings.setValue("track_info/split", split);
	settings.setValue("track_info/keep_aspect", m_same_range->isChecked());
	settings.setValue("track_info/recent_maps", m_mapdir.c_str());
	settings.setValue("track_info/recent_svgs", m_svgdir.c_str());
	settings.setValue("track_info/recent_tab", m_tab->currentIndex());
	settings.setValue("track_info/distance_bin", m_dist_binlen->value());
	settings.setValue("track_info/speed_check", m_speed_check->isChecked());
	settings.setValue("track_info/time_check", m_time_check->isChecked());
}


void TrackInfos::RestoreSettings(QSettings& settings)
{
	QByteArray split = settings.value("track_info/split").toByteArray();
	if(split.size())
		m_split->restoreState(split);

	if(settings.contains("track_info/keep_aspect"))
		m_same_range->setChecked(settings.value("track_info/keep_aspect").toBool());

	if(settings.contains("track_info/recent_maps"))
	{
		m_mapdir = settings.value("track_info/recent_maps").toString().toStdString();
		m_mapfile->setText(m_mapdir.c_str());
	}

	if(settings.contains("track_info/recent_svgs"))
		m_svgdir = settings.value("track_info/recent_svgs").toString().toStdString();

	if(settings.contains("track_info/recent_tab"))
		m_tab->setCurrentIndex(settings.value("track_info/recent_tab").toInt());

	if(settings.contains("track_info/distance_bin"))
		m_dist_binlen->setValue(settings.value("track_info/distance_bin").toDouble());

	if(settings.contains("track_info/speed_check"))
		m_speed_check->setChecked(settings.value("track_info/speed_check").toBool());

	if(settings.contains("track_info/time_check"))
		m_time_check->setChecked(settings.value("track_info/time_check").toBool());
}
