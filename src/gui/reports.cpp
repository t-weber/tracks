/**
 * distance reports
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#include "reports.h"
#include "helpers.h"
#include "tableitems.h"
#include "lib/calc.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <sstream>
#include <limits>
#include <cmath>


// table columns
#define TAB_DATE      0
#define TAB_COUNT     1
#define TAB_DIST      2
#define TAB_DIST_SUM  3
#define TAB_NUM_COLS  4


Reports::Reports(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Distance Reports");
	setSizeGripEnabled(true);

	QWidget *plot_panel = new QWidget(this);

	// plot
	//m_plot = std::make_shared<QCustomPlot>(plot_panel);
	m_plot = new QCustomPlot(plot_panel);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Month");
	m_plot->yAxis->setLabel("Distance (km)");
	m_plot->legend->setVisible(false);

	auto *ticker = new QCPAxisTickerDateTime{};
	ticker->setDateTimeSpec(Qt::LocalTime);
	ticker->setDateTimeFormat("yyyy-MM");
	m_plot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>{ticker});

	connect(m_plot/*.get()*/, &QCustomPlot::mouseMove, this, &Reports::PlotMouseMove);

	// "all tracks" checkbox
	m_all_tracks = std::make_shared<QCheckBox>(plot_panel);
	m_all_tracks->setText("Per Track");
	m_all_tracks->setToolTip("Show the distances from all tracks individually.");
	m_all_tracks->setChecked(false);
	connect(m_all_tracks.get(), &QCheckBox::toggled, this, &Reports::PlotDistances);

	// distance sum checkbox
	m_cumulative = std::make_shared<QCheckBox>(plot_panel);
	m_cumulative->setText("Sum");
	m_cumulative->setToolTip("Show the sum of the distances.");
	m_cumulative->setChecked(false);
	connect(m_cumulative.get(), &QCheckBox::toggled, this, &Reports::PlotDistances);

	// plot reset button
	QPushButton *btn_replot = new QPushButton(plot_panel);
	btn_replot->setText("Reset Plot");
	connect(btn_replot, &QAbstractButton::clicked, this, &Reports::ResetDistPlotRange);

	QGridLayout *plot_panel_layout = new QGridLayout(plot_panel);
	plot_panel_layout->setContentsMargins(0, 0, 0, 0);
	plot_panel_layout->setVerticalSpacing(0);
	plot_panel_layout->setHorizontalSpacing(0);
	plot_panel_layout->addWidget(m_plot/*.get()*/, 0, 0, 1, 3);
	plot_panel_layout->addWidget(m_all_tracks.get(), 1, 0, 1, 1);
	plot_panel_layout->addWidget(m_cumulative.get(), 1, 1, 1, 1);
	plot_panel_layout->addWidget(btn_replot, 1, 2, 1, 1);

	// track table
	m_table = std::make_shared<QTableWidget>(this);
	m_table->setShowGrid(true);
	m_table->setSortingEnabled(true);
	m_table->setMouseTracking(true);
	m_table->setSelectionBehavior(QTableWidget::SelectRows);
	m_table->setSelectionMode(QTableWidget::SingleSelection);

	m_table->setColumnCount(TAB_NUM_COLS);
	m_table->setHorizontalHeaderItem(TAB_DATE, new QTableWidgetItem{"Date"});
	m_table->setHorizontalHeaderItem(TAB_COUNT, new QTableWidgetItem{"Tracks"});
	m_table->setHorizontalHeaderItem(TAB_DIST, new QTableWidgetItem{"Distance"});
	m_table->setHorizontalHeaderItem(TAB_DIST_SUM, new QTableWidgetItem{"Distance Sum"});

	m_table->horizontalHeader()->setDefaultSectionSize(150);
	m_table->verticalHeader()->setDefaultSectionSize(24);
	m_table->verticalHeader()->setVisible(false);

	// splitter
	m_split = std::make_shared<QSplitter>(this);
	m_split->setOrientation(Qt::Vertical);
	m_split->addWidget(plot_panel);
	m_split->addWidget(m_table.get());
	m_split->setStretchFactor(0, 10);
	m_split->setStretchFactor(1, 1);

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
	QWidget *status_panel = new QWidget{this};
	status_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QGridLayout *status_panel_layout = new QGridLayout(status_panel);
	status_panel_layout->setContentsMargins(0, 0, 0, 0);
	status_panel_layout->setVerticalSpacing(0);
	status_panel_layout->setHorizontalSpacing(0);
	status_panel_layout->addWidget(m_status.get(), 0, 0, 1, 3);
	status_panel_layout->addWidget(m_buttonbox.get(), 0, 3, 1, 1);

	// main grid
	QGridLayout *main_layout = new QGridLayout(this);
	main_layout->setContentsMargins(8, 8, 8, 8);
	main_layout->setVerticalSpacing(4);
	main_layout->setHorizontalSpacing(4);
	main_layout->addWidget(m_split.get(), 0, 0, 1, 1);
	main_layout->addWidget(status_panel, 1, 0, 1, 1);

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

	QByteArray split = settings.value("dlg_reports/split").toByteArray();
	if(split.size())
		m_split->restoreState(split);

	// table column widths
	if(settings.contains("dlg_reports/date_col"))
		m_table->setColumnWidth(TAB_DATE, settings.value("dlg_reports/date_col").toInt());
	else
		m_table->setColumnWidth(TAB_DATE, 150);

	if(settings.contains("dlg_reports/count_col"))
		m_table->setColumnWidth(TAB_COUNT, settings.value("dlg_reports/count_col").toInt());
	else
		m_table->setColumnWidth(TAB_COUNT, 100);

	if(settings.contains("dlg_reports/dist_col"))
		m_table->setColumnWidth(TAB_DIST, settings.value("dlg_reports/dist_col").toInt());
	else
		m_table->setColumnWidth(TAB_DIST, 150);

	if(settings.contains("dlg_reports/distsum_col"))
		m_table->setColumnWidth(TAB_DIST_SUM, settings.value("dlg_reports/distsum_col").toInt());
	else
		m_table->setColumnWidth(TAB_DIST_SUM, 150);
}


Reports::~Reports()
{
}


void Reports::SetTrackDB(const t_tracks *trackdb)
{
	m_trackdb = trackdb;
	CalcDistances();
}


void Reports::FillDistancesTable()
{
	if(!m_trackdb || !m_table)
		return;

	m_table->setSortingEnabled(false);
	m_table->clearContents();
	m_table->setRowCount(m_monthly.size() + m_yearly.size());

	// yearly distances
	int row = 0;
	t_real total_dist = 0.;
	for(const auto& pair : m_yearly)
	{
		t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
			pair.first.time_since_epoch()).count();
		t_size num_tracks = pair.second.second;
		t_real dist = pair.second.first / 1000.;
		total_dist += dist;

		m_table->setItem(row, TAB_DATE, new DateTableWidgetItem<
			typename t_track::t_clk, typename t_track::t_timept, t_real>(epoch, false, " (full year)"));
		m_table->setItem(row, TAB_COUNT, new NumericTableWidgetItem<t_size>(num_tracks, g_prec_gui));
		m_table->setItem(row, TAB_DIST, new NumericTableWidgetItem<t_real>(dist, g_prec_gui, " km"));
		m_table->setItem(row, TAB_DIST_SUM, new NumericTableWidgetItem<t_real>(total_dist, g_prec_gui, " km"));

		// set read-only
		for(int col = 0; col < TAB_NUM_COLS; ++col)
		{
			m_table->item(row, col)->setFlags(
				m_table->item(row, col)->flags() & ~Qt::ItemIsEditable);
		}

		++row;
	}

	// monthly distances
	total_dist = 0.;
	for(const auto& pair : m_monthly)
	{
		t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
			pair.first.time_since_epoch()).count();
		t_size num_tracks = pair.second.second;
		t_real dist = pair.second.first / 1000.;
		total_dist += dist;

		m_table->setItem(row, TAB_DATE, new DateTableWidgetItem<
			typename t_track::t_clk, typename t_track::t_timept, t_real>(epoch, true));
		m_table->setItem(row, TAB_COUNT, new NumericTableWidgetItem<t_size>(num_tracks, g_prec_gui));
		m_table->setItem(row, TAB_DIST, new NumericTableWidgetItem<t_real>(dist, g_prec_gui, " km"));
		m_table->setItem(row, TAB_DIST_SUM, new NumericTableWidgetItem<t_real>(total_dist, g_prec_gui, " km"));

		// set read-only
		for(int col = 0; col < TAB_NUM_COLS; ++col)
		{
			m_table->item(row, col)->setFlags(
				m_table->item(row, col)->flags() & ~Qt::ItemIsEditable);
		}

		++row;
	}

	m_table->setSortingEnabled(true);
}


void Reports::CalcDistances()
{
	if(!m_trackdb)
		return;

	m_monthly = m_trackdb->GetDistancePerTime(false, false);
	m_yearly = m_trackdb->GetDistancePerTime(false, true);

	PlotDistances();
	FillDistancesTable();
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

	// show sum for all tracks
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

	// show monthly distance sum
	else
	{
		const t_size num_tracks = m_monthly.size();
		epochs.reserve(num_tracks);
		dists.reserve(num_tracks);

		for(const auto& pair : m_monthly)
		{
			t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
				pair.first.time_since_epoch()).count();
			t_real dist = pair.second.first / 1000.;
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
	qreal y = evt->position().y();
#else
	qreal x = evt->x();
	qreal y = evt->y();
#endif
	t_real epoch = m_plot->xAxis->pixelToCoord(x);
	t_real distance = m_plot->yAxis->pixelToCoord(y);

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

	QByteArray split{m_split->saveState()};
	settings.setValue("dlg_reports/split", split);

	if(m_table)
	{
		// table column widths
		settings.setValue("dlg_reports/date_col", m_table->columnWidth(TAB_DATE));
		settings.setValue("dlg_reports/count_col", m_table->columnWidth(TAB_COUNT));
		settings.setValue("dlg_reports/dist_col", m_table->columnWidth(TAB_DIST));
		settings.setValue("dlg_reports/distsum_col", m_table->columnWidth(TAB_DIST_SUM));
	}

	QDialog::accept();
}


void Reports::reject()
{
	QDialog::reject();
}
