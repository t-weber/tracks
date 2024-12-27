/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_infos.h"
#include "helpers.h"
namespace fs = __map_fs;

#include <QtWidgets/QTabWidget>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QMessageBox>
#include <QtSvg/QSvgRenderer>

#include <sstream>
#include <cmath>
#include <numbers>
namespace num = std::numbers;


TrackInfos::TrackInfos(QWidget* parent) : QWidget{parent}
{
	QTabWidget *tab = new QTabWidget(this);
	QWidget *plot_panel = new QWidget(tab);
	QWidget *map_panel = new QWidget(tab);
	tab->addTab(plot_panel, "Track");
	tab->addTab(map_panel, "Map");
#ifndef _TRACKS_USE_OSMIUM_
	tab->setTabEnabled(1, false);
	map_panel->setEnabled(false);
#endif

	// track plot panel
	m_plot = std::make_shared<QCustomPlot>(plot_panel);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Longitude (Degrees)");
	m_plot->yAxis->setLabel("Latitude (Degrees)");

	m_same_range = std::make_shared<QCheckBox>(plot_panel);
	m_same_range->setText("Keep Aspect");
	m_same_range->setToolTip("Correct the angular range aspect ratio for longitudes and latitudes.");
	m_same_range->setChecked(true);
	connect(m_same_range.get(), &QAbstractButton::toggled, this, &TrackInfos::ResetPlotRange);

	QPushButton *btn_replot = new QPushButton(plot_panel);
	btn_replot->setText("Reset Plot");
	btn_replot->setToolTip("Reset the range of the longitude and latitude to show all track data.");
	connect(btn_replot, &QAbstractButton::clicked, this, &TrackInfos::ResetPlotRange);

	QGridLayout *plot_panel_layout = new QGridLayout(plot_panel);
	plot_panel_layout->setContentsMargins(4, 4, 4, 4);
	plot_panel_layout->setVerticalSpacing(4);
	plot_panel_layout->setHorizontalSpacing(4);
	plot_panel_layout->addWidget(m_plot.get(), 0, 0, 1, 4);
	plot_panel_layout->addWidget(m_same_range.get(), 1, 0, 1, 1);
	plot_panel_layout->addWidget(btn_replot, 1, 3, 1, 1);

	// map plot panel
	m_map = std::make_shared<MapDrawer>(map_panel);
	//m_map->renderer()->setAnimationEnabled(false);
	m_map->renderer()->setAspectRatioMode(Qt::KeepAspectRatio);

	m_map_context = std::make_shared<QMenu>(map_panel);

	QIcon iconSaveSvg = QIcon::fromTheme("image-x-generic");
	QAction *actionSaveSvg = new QAction(iconSaveSvg, "Save Image...", m_map_context.get());
	connect(actionSaveSvg, &QAction::triggered, this, &TrackInfos::SaveMapSvg);

	m_map_context->addAction(actionSaveSvg);

	m_mapfile = std::make_shared<QLineEdit>(map_panel);
	m_mapfile->setPlaceholderText("Directory with Map Files (.osm or .osm.pbf)");
	m_mapfile->setToolTip("Map file to draw.");

	QPushButton *btn_browse_map = new QPushButton(map_panel);
	btn_browse_map->setText("Select Map Dir...");
	btn_browse_map->setToolTip("Select a map file to draw.");
	connect(btn_browse_map, &QAbstractButton::clicked, this, &TrackInfos::SelectMap);

	QPushButton *btn_calc_map = new QPushButton(map_panel);
	btn_calc_map->setText("Calculate Map");
	btn_calc_map->setToolTip("Calculate the map for the current track.");
	connect(btn_calc_map, &QAbstractButton::clicked, this, &TrackInfos::PlotMap);

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
	m_split->addWidget(tab);
	m_split->addWidget(m_infos.get());
	m_split->setStretchFactor(0, 10);
	m_split->setStretchFactor(1, 1);

	QGridLayout *main_layout = new QGridLayout(this);
	main_layout->setContentsMargins(4, 4, 4, 4);
	main_layout->setVerticalSpacing(4);
	main_layout->setHorizontalSpacing(4);
	main_layout->addWidget(m_split.get(), 0, 0, 1, 1);

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::PlotMouseMove);
	connect(m_map.get(), &MapDrawer::MouseMoved, this, &TrackInfos::MapMouseMove);
	connect(m_map.get(), &MapDrawer::MousePressed, this, &TrackInfos::MapMouseClick);
}


TrackInfos::~TrackInfos()
{
}


void TrackInfos::CalcPlotRange()
{
	m_min_long_plot = m_min_long;
	m_max_long_plot = m_max_long;
	m_min_lat_plot = m_min_lat;
	m_max_lat_plot = m_max_lat;

	if(!m_plot || !m_same_range)
		return;

	t_real aspect = t_real(m_plot->viewport().height()) /
		t_real(m_plot->viewport().width());

	if(m_same_range->isChecked())
	{
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


void TrackInfos::ResetPlotRange()
{
	CalcPlotRange();

	if(!m_plot || !m_same_range)
		return;

	t_real range_long = m_max_long_plot - m_min_long_plot;
	t_real range_lat = m_max_lat_plot - m_min_lat_plot;

	m_plot->xAxis->setRange(
		(m_min_long_plot - range_long / 20.) * t_real(180) / num::pi_v<t_real>,
		(m_max_long_plot + range_long / 20.) * t_real(180) / num::pi_v<t_real>);

	m_plot->yAxis->setRange(
		(m_min_lat_plot - range_lat / 20.) * t_real(180) / num::pi_v<t_real>,
		(m_max_lat_plot + range_lat / 20.) * t_real(180) / num::pi_v<t_real>);

	m_plot->replot();
}


/**
 * sets the current track and shows it in the plotter
 */
void TrackInfos::ShowTrack(const t_track& track)
{
	m_track = &track;

	// print track infos
	m_infos->setHtml(track.PrintHtml(g_prec_gui).c_str());

	// prepare track plot
	std::tie(m_min_lat, m_max_lat) = track.GetLatitudeRange();
	std::tie(m_min_long, m_max_long) = track.GetLongitudeRange();
	CalcPlotRange();

	QVector<t_real> latitudes, longitudes;
	latitudes.reserve(track.GetPoints().size());
	longitudes.reserve(track.GetPoints().size());

	for(const t_track_pt& pt : track.GetPoints())
	{
		latitudes.push_back(pt.latitude * t_real(180) / num::pi_v<t_real>);
		longitudes.push_back(pt.longitude * t_real(180) / num::pi_v<t_real>);
	}

	// plot map and track
	if(!m_plot)
		return;
	m_plot->clearPlottables();

	//PlotMap();

	QCPCurve *curve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
	curve->setData(longitudes, latitudes);
	curve->setLineStyle(QCPCurve::lsLine);
	QPen pen = curve->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0x00, 0x00, 0xff, 0xff});
	curve->setPen(pen);

	ResetPlotRange();
}


/**
 * browse for map directories (or files)
 */
void TrackInfos::SelectMap()
{
	auto filedlg = std::make_shared<QFileDialog>(
		this, "Load Map File", m_mapdir.c_str(),
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

	fs::path dir{files[0].toStdString()};
	m_mapdir = dir/*.parent_path()*/.string();
}


/**
 * render an image of the map corresponding to the current track
 */
void TrackInfos::PlotMap()
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
		return;

	t_real lon_range = m_max_long_plot - m_min_long_plot;
	t_real lat_range = m_max_lat_plot - m_min_lat_plot;

	// track
	std::vector<typename t_map::t_vertex> thetrack;
	if(m_track)
	{
		thetrack.reserve(m_track->GetPoints().size());
		for(const t_track_pt& pt : m_track->GetPoints())
		{
			typename t_map::t_vertex vert
			{
				.longitude = pt.longitude,
				.latitude = pt.latitude,
			};

			thetrack.emplace_back(std::move(vert));
		}
	}

	// map loading progress
	QProgressDialog progress_dlg{this};
	progress_dlg.setMinimumDuration(500);
	progress_dlg.setAutoClose(true);
	progress_dlg.setWindowModality(Qt::WindowModal);
	progress_dlg.setLabelText("Calculating map...");

	// map loading progress callback
	std::function<bool(t_size, t_size)> progress = [&progress_dlg](
		t_size offs, t_size size) -> bool
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

	// cut out a map that has some margins around the actual data area
	if(map.ImportDir(m_mapfile->text().toStdString(),
		m_min_long_plot - lon_range/10., m_max_long_plot + lon_range/10.,
		m_min_lat_plot - lat_range/10., m_max_lat_plot + lat_range/10.,
		&progress))
	{
		// plot the data area
		std::ostringstream ostr;
		map.ExportSvg(ostr, g_map_scale,
			m_min_long_plot - lon_range/20., m_max_long_plot + lon_range/20.,
			m_min_lat_plot - lat_range/20., m_max_lat_plot + lat_range/20.);

		m_map_image = QByteArray{ostr.str().c_str(), static_cast<int>(ostr.str().size())};
		m_map->load(m_map_image);
	}


	/*MapPlotter map;
	if(map.Import(m_mapfile->text().toStdString(),
		m_min_long_plot, m_max_long_plot,
		m_min_lat_plot, m_max_lat_plot))
	{
		map.SetPlotRange(
			m_min_long_plot - lon_range/10., m_max_long_plot + lon_range/10.,
			m_min_lat_plot - lat_range/10., m_max_lat_plot + lat_range/10.);
		map.Plot(m_plot);
	}*/
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

	if(m_plot)
	{
		m_plot->clearPlottables();
		m_plot->replot();
	}

	if(m_map)
	{
		m_map_image.clear();
		m_map->load(m_map_image);
	}
}


/**
 * the mouse has moved in the plot widget
 */
void TrackInfos::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif

	t_real longitude = m_plot->xAxis->pixelToCoord(x);
	t_real latitude = m_plot->yAxis->pixelToCoord(y);

	emit PlotCoordsChanged(longitude, latitude);
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
 * the mouse has been clicked in the map widget
 */
void TrackInfos::MapMouseClick(QMouseEvent *evt)
{
	if(!m_map)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QPoint pos = evt->position();
#else
	QPoint pos = evt->pos();
#endif

	if(evt->buttons() & Qt::RightButton)
	{
		QPoint posGlobal = m_map->mapToGlobal(pos);
		posGlobal.rx() += 4;
		posGlobal.ry() += 4;

		m_map_context->popup(posGlobal);
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
}
