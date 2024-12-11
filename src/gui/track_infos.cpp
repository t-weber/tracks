/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_infos.h"
#include "helpers.h"

#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <numbers>
namespace num = std::numbers;


TrackInfos::TrackInfos(QWidget* parent)
	: QWidget(parent)
{
	QWidget *plot_panel = new QWidget(this);

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
	plot_panel_layout->setContentsMargins(0, 0, 0, 0);
	plot_panel_layout->setVerticalSpacing(0);
	plot_panel_layout->setHorizontalSpacing(0);
	plot_panel_layout->addWidget(m_plot.get(), 0, 0, 1, 4);
	plot_panel_layout->addWidget(m_same_range.get(), 1, 0, 1, 1);
	plot_panel_layout->addWidget(btn_replot, 1, 3, 1, 1);

	m_infos = std::make_shared<QTextEdit>(this);
	m_infos->setReadOnly(true);

	m_split = std::make_shared<QSplitter>(this);
	m_split->setOrientation(Qt::Vertical);
	m_split->addWidget(plot_panel);
	m_split->addWidget(m_infos.get());
	m_split->setStretchFactor(0, 10);
	m_split->setStretchFactor(1, 1);

	QGridLayout *main_layout = new QGridLayout(this);
	main_layout->setContentsMargins(4, 4, 4, 4);
	main_layout->setVerticalSpacing(4);
	main_layout->setHorizontalSpacing(4);
	main_layout->addWidget(m_split.get(), 0, 0, 1, 1);

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::PlotMouseMove);
}


TrackInfos::~TrackInfos()
{
}


void TrackInfos::ResetPlotRange()
{
	if(!m_plot || !m_same_range)
		return;

	t_real aspect = t_real(m_plot->viewport().height()) /
		t_real(m_plot->viewport().width());

	t_real min_long = m_min_long;
	t_real max_long = m_max_long;
	t_real min_lat = m_min_lat;
	t_real max_lat = m_max_lat;

	t_real range_long = max_long - min_long;
	t_real range_lat = max_lat - min_lat;

	if(m_same_range->isChecked())
	{
		if(range_long*aspect > range_lat)
		{
			t_real mid_lat = min_lat + range_lat * 0.5;

			range_lat = range_long * aspect;
			min_lat = mid_lat - range_lat * 0.5;
			max_lat = mid_lat + range_lat * 0.5;
		}
		else if(range_long*aspect < range_lat)
		{
			t_real mid_long = min_long + range_long * 0.5;

			range_long = range_lat / aspect;
			min_long = mid_long - range_long * 0.5;
			max_long = mid_long + range_long * 0.5;
		}
	}

	m_plot->xAxis->setRange(
		(min_long - range_long / 20.) * t_real(180) / num::pi_v<t_real>,
		(max_long + range_long / 20.) * t_real(180) / num::pi_v<t_real>);

	m_plot->yAxis->setRange(
		(min_lat - range_lat / 20.) * t_real(180) / num::pi_v<t_real>,
		(max_lat + range_lat / 20.) * t_real(180) / num::pi_v<t_real>);

	m_plot->replot();
}


void TrackInfos::ShowTrack(const t_track& track)
{
	// print track infos
	m_infos->setHtml(track.PrintHtml(g_prec_gui).c_str());

	// prepare track plot
	std::tie(m_min_lat, m_max_lat) = track.GetLatitudeRange();
	std::tie(m_min_long, m_max_long) = track.GetLongitudeRange();

	QVector<t_real> latitudes, longitudes;
	latitudes.reserve(track.GetPoints().size());
	longitudes.reserve(track.GetPoints().size());

	for(const t_track_pt& pt : track.GetPoints())
	{
		latitudes.push_back(pt.latitude * t_real(180) / num::pi_v<t_real>);
		longitudes.push_back(pt.longitude * t_real(180) / num::pi_v<t_real>);
	}

	// plot track
	if(!m_plot)
		return;
	m_plot->clearPlottables();

	QCPCurve *curve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
	curve->setData(longitudes, latitudes);
	curve->setLineStyle(QCPCurve::lsLine);
	QPen pen = curve->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0x00, 0x00, 0xff, 0xff});
	curve->setPen(pen);

	ResetPlotRange();
}


void TrackInfos::Clear()
{
	if(!m_plot)
		return;

	m_plot->clearPlottables();
	m_plot->replot();

	m_infos->clear();
}


void TrackInfos::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

	t_real longitude = m_plot->xAxis->pixelToCoord(evt->x());
	t_real latitude = m_plot->yAxis->pixelToCoord(evt->y());

	emit PlotCoordsChanged(longitude, latitude);
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
	settings.setValue("track_info_split", split);
	settings.setValue("track_info_keep_aspect", m_same_range->isChecked());
}


void TrackInfos::RestoreSettings(QSettings& settings)
{
	QByteArray split = settings.value("track_info_split").toByteArray();
	if(split.size())
		m_split->restoreState(split);

	if(settings.contains("track_info_keep_aspect"))
		m_same_range->setChecked(settings.value("track_info_keep_aspect").toBool());
}
