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

#include <numbers>
namespace num = std::numbers;


TrackInfos::TrackInfos(QWidget* parent)
	: QWidget(parent)
{
	m_plot = std::make_shared<QCustomPlot>(this);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Longitude (Degrees)");
	m_plot->yAxis->setLabel("Latitude (Degrees)");

	m_infos = std::make_shared<QTextEdit>(this);
	m_infos->setReadOnly(true);

	m_split = std::make_shared<QSplitter>(this);
	m_split->setOrientation(Qt::Vertical);
	m_split->addWidget(m_plot.get());
	m_split->addWidget(m_infos.get());
	m_split->setStretchFactor(0, 10);
	m_split->setStretchFactor(1, 1);

	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_split.get(), 0, 0, 1, 1);

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::PlotMouseMove);
}


TrackInfos::~TrackInfos()
{
}


void TrackInfos::ShowTrack(const t_track& track)
{
	// print track infos
	m_infos->setHtml(track.PrintHtml(g_prec_gui).c_str());

	// prepare track plot
	auto [ min_lat, max_lat ] = track.GetLatitudeRange();
	auto [ min_long, max_long ] = track.GetLongitudeRange();

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

	m_plot->xAxis->setRange(
		(min_long - (max_long - min_long) / 20.) * t_real(180) / num::pi_v<t_real>,
		(max_long + (max_long - min_long) / 20.) * t_real(180) / num::pi_v<t_real>);
	m_plot->yAxis->setRange(
		(min_lat - (max_lat - min_lat) / 20.) * t_real(180) / num::pi_v<t_real>,
		(max_lat + (max_lat - min_lat) / 20.) * t_real(180) / num::pi_v<t_real>);
	m_plot->replot();
}


void TrackInfos::Clear()
{
	if(!m_plot)
		return;

	m_plot->clearPlottables();
	m_plot->replot();
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
}


void TrackInfos::RestoreSettings(QSettings& settings)
{
	QByteArray split = settings.value("track_info_split").toByteArray();
	if(split.size())
		m_split->restoreState(split);
}
