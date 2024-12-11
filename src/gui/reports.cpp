/**
 * distance reports
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#include "reports.h"
#include "helpers.h"
#include "lib/calc.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <sstream>
#include <limits>
#include <cmath>


Reports::Reports(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Distance Reports");
	setSizeGripEnabled(true);

	// plot
	m_plot = std::make_shared<QCustomPlot>(this);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Month");
	m_plot->yAxis->setLabel("Distance (km)");
	m_plot->legend->setVisible(false);
	auto *ticker = new QCPAxisTickerDateTime{};
	ticker->setDateTimeSpec(Qt::LocalTime);
	ticker->setDateTimeFormat("yyyy-MM");
	m_plot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>{ticker});

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &Reports::PlotMouseMove);

	// "all tracks" checkbox
	m_all_tracks = std::make_shared<QCheckBox>(this);
	m_all_tracks->setText("Per Track");
	m_all_tracks->setToolTip("Show the distances from all tracks individually.");
	m_all_tracks->setChecked(false);
	connect(m_all_tracks.get(), &QCheckBox::toggled, this, &Reports::PlotDistances);

	// distance sum checkbox
	m_cumulative = std::make_shared<QCheckBox>(this);
	m_cumulative->setText("Sum");
	m_cumulative->setToolTip("Show the sum of the distances.");
	m_cumulative->setChecked(false);
	connect(m_cumulative.get(), &QCheckBox::toggled, this, &Reports::PlotDistances);

	// plot reset button
	QPushButton *btn_replot = new QPushButton(this);
	btn_replot->setText("Reset Plot");
	connect(btn_replot, &QAbstractButton::clicked, this, &Reports::ResetDistPlotRange);

	// status bar
	m_status = std::make_shared<QLabel>(this);

	// button box
	m_buttonbox = std::make_shared<QDialogButtonBox>(this);
	m_buttonbox->setStandardButtons(QDialogButtonBox::Ok);
	m_buttonbox->button(QDialogButtonBox::Ok)->setDefault(true);

	connect(m_buttonbox.get(), &QDialogButtonBox::clicked,
		[this](QAbstractButton *btn) -> void
	{
		// get button role
		QDialogButtonBox::ButtonRole role = m_buttonbox->buttonRole(btn);

		if(role == QDialogButtonBox::AcceptRole)
			this->accept();
		else if(role == QDialogButtonBox::RejectRole)
			this->reject();
	});

	// status panel and its grid
	QWidget *panel = new QWidget{this};
	panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QGridLayout *panellayout = new QGridLayout(panel);
	panellayout->setContentsMargins(0, 0, 0, 0);
	panellayout->setVerticalSpacing(0);
	panellayout->setHorizontalSpacing(0);
	panellayout->addWidget(m_status.get(), 0, 0, 1, 3);
	panellayout->addWidget(m_buttonbox.get(), 0, 3, 1, 1);

	// main grid
	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(8, 8, 8, 8);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_plot.get(), 0, 0, 1, 3);
	mainlayout->addWidget(m_all_tracks.get(), 1, 0, 1, 1);
	mainlayout->addWidget(m_cumulative.get(), 1, 1, 1, 1);
	mainlayout->addWidget(btn_replot, 1, 2, 1, 1);
	mainlayout->addWidget(panel, 2, 0, 1, 3);

	// restore settings
	QSettings settings{this};

	if(settings.contains("dlg_reports/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_reports/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		this->resize(512, 512);
	}

	if(settings.contains("dlg_reports/all_tracks"))
		m_all_tracks->setChecked(settings.value("dlg_reports/all_tracks").toBool());
	if(settings.contains("dlg_reports/sum_distances"))
		m_cumulative->setChecked(settings.value("dlg_reports/sum_distances").toBool());
}


Reports::~Reports()
{
}


void Reports::SetTrackDB(const t_tracks *trackdb)
{
	m_trackdb = trackdb;
	PlotDistances();
}


void Reports::ResetDistPlotRange()
{
	m_plot->xAxis->setRange(
		m_min_epoch - (m_max_epoch - m_min_epoch) / 20.,
		m_max_epoch + (m_max_epoch - m_min_epoch) / 20.);

	m_plot->yAxis->setRange(
		m_min_dist - (m_max_dist - m_min_dist) / 20.,
		m_max_dist + (m_max_dist - m_min_dist) / 20.);

	m_plot->replot();
}


void Reports::PlotDistances()
{
	if(!m_trackdb || !m_plot)
		return;

	m_plot->clearPlottables();

	m_min_epoch = std::numeric_limits<t_real>::max();
	m_max_epoch = -m_min_epoch;
	m_min_dist = std::numeric_limits<t_real>::max();
	m_max_dist = -m_min_dist;

	QVector<t_real> epochs, dists;
	t_real total_dist = 0.;

	bool cumulative = m_cumulative && m_cumulative->isChecked();

	if(m_all_tracks && m_all_tracks->isChecked())
	{
		const t_size num_tracks = m_trackdb->GetTrackCount();
		epochs.reserve(num_tracks);
		dists.reserve(num_tracks);

		for(std::size_t track_idx = 0; track_idx < num_tracks; ++track_idx)
		{
			// tracks are in reverse order
			const t_track *track = m_trackdb->GetTrack(num_tracks - track_idx - 1);
			if(!track)
				continue;

			// get track time
			auto tp = track->GetStartTime();
			if(!tp)
				continue;

			t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
				tp->time_since_epoch()).count();

			// get track distance
			t_real dist = track->GetTotalDistance(false) / 1000.;
			total_dist += dist;

			epochs.push_back(epoch);
			dists.push_back(cumulative ? total_dist : dist);

			// ranges
			m_min_epoch = std::min(m_min_epoch, epoch);
			m_max_epoch = std::max(m_max_epoch, epoch);

			m_min_dist = std::min(m_min_dist, dist);
			m_max_dist = std::max(m_max_dist, dist);
		}
	}
	else
	{
		auto monthly = m_trackdb->GetDistancePerMonth();
		const t_size num_tracks = monthly.size();
		epochs.reserve(num_tracks);
		dists.reserve(num_tracks);

		for(const auto& pair : monthly)
		{
			t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
				pair.first.time_since_epoch()).count();
			t_real dist = pair.second / 1000.;
			total_dist += dist;

			epochs.push_back(epoch);
			dists.push_back(cumulative ? total_dist : dist);

			// ranges
			m_min_epoch = std::min(m_min_epoch, epoch);
			m_max_epoch = std::max(m_max_epoch, epoch);

			m_min_dist = std::min(m_min_dist, dist);
			m_max_dist = std::max(m_max_dist, dist);
		}
	}

	if(cumulative)
	{
		m_min_dist = std::min(m_min_dist, total_dist);
		m_max_dist = std::max(m_max_dist, total_dist);
	}

	// create graph
	auto add_graph = [this](const QVector<t_real>& epochs, const QVector<t_real>& dists)
	{
		QCPGraph *graph = new QCPGraph(m_plot->xAxis, m_plot->yAxis);

		QPen pen = graph->pen();
		pen.setWidthF(2.);
		pen.setColor(QColor{0, 0, 0xff, 0xff});

		QBrush brush = graph->brush();
		brush.setStyle(Qt::SolidPattern);
		brush.setColor(QColor{0, 0, 0xff, 0x99});

		graph->setData(epochs, dists, true);
		graph->setLineStyle(QCPGraph::lsLine);
		graph->setScatterStyle(QCPScatterStyle{QCPScatterStyle::ssCircle, pen, brush, 6.});
		graph->setPen(pen);
		//graph->setBrush(brush);
	};

	// plot distances
	add_graph(epochs, dists);

	ResetDistPlotRange();
}


void Reports::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

	t_real epoch = m_plot->xAxis->pixelToCoord(evt->x());
	t_real distance = m_plot->yAxis->pixelToCoord(evt->y());

	// get date string
	auto tp = t_track::t_timept{static_cast<typename t_track::t_time_ty>(
		epoch * 1000.) * std::chrono::milliseconds{1}};
	std::string date = from_timepoint<typename t_track::t_clk, typename t_track::t_timept>(
		tp, true, false);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << "Date: " << date << ", Distance: " << distance << " km.";
	m_status->setText(ostr.str().c_str());
}


void Reports::accept()
{
	// save settings
	QSettings settings{this};

	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_reports/wnd_geo", geo);

	settings.setValue("dlg_reports/all_tracks", m_all_tracks->isChecked());
	settings.setValue("dlg_reports/sum_distances", m_cumulative->isChecked());

	QDialog::accept();
}


void Reports::reject()
{
	QDialog::reject();
}
