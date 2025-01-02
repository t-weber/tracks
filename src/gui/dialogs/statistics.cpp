/**
 * speed statistics
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#include "statistics.h"
#include "lib/calc.h"
#include "../helpers.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <sstream>
#include <limits>
#include <cmath>


Statistics::Statistics(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Pace Statistics");
	setSizeGripEnabled(true);

	// plot
	m_plot = std::make_shared<QCustomPlot>(this);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Date");
	m_plot->yAxis->setLabel("Pace (min/km)");
	m_plot->legend->setVisible(true);
	auto *ticker = new QCPAxisTickerDateTime{};
	ticker->setDateTimeSpec(Qt::LocalTime);
	ticker->setDateTimeFormat("yyyy-MM-dd");
	m_plot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>{ticker});

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &Statistics::PlotMouseMove);

	// speed/pace checkbox
	m_speed_check = std::make_shared<QCheckBox>(this);
	m_speed_check->setText("Plot speeds instead.");
	m_speed_check->setToolTip("Show speeds instead of paces.");
	m_speed_check->setChecked(false);
	connect(m_speed_check.get(), &QCheckBox::toggled, this, &Statistics::PlotSpeeds);

	// plot reset button
	QPushButton *btn_replot = new QPushButton(this);
	btn_replot->setText("Reset Plot");
	connect(btn_replot, &QAbstractButton::clicked, this, &Statistics::ResetSpeedPlotRange);

	// track length check boxes
	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		m_length_checks[idx] = std::make_shared<QCheckBox>(this);
		if(idx == 0)
		{
			m_length_checks[idx]->setText(
				QString("\xe2\x89\xa5 %1 km").arg(s_lengths[idx]));
		}
		else
		{
			m_length_checks[idx]->setText(
				QString("%1 - %2 km").arg(s_lengths[idx])
					.arg(s_lengths[idx-1]));
		}
		m_length_checks[idx]->setChecked(true);

		connect(m_length_checks[idx].get(), &QCheckBox::toggled,
			this, &Statistics::PlotSpeeds);
	}

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

	// horizontal lines
	QFrame *frame1 = new QFrame(this);
	frame1->setFrameStyle(QFrame::HLine);
	QFrame *frame2 = new QFrame(this);
	frame2->setFrameStyle(QFrame::HLine);

	// main grid
	int num_cols = 3;
	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(8, 8, 8, 8);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_plot.get(), 0, 0, 1, num_cols);
	mainlayout->addWidget(m_speed_check.get(), 1, 0, 1, 2);
	mainlayout->addWidget(btn_replot, 1, 2, 1, 1);
	mainlayout->addWidget(frame1, 2, 0, 1, num_cols);

	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		mainlayout->addWidget(m_length_checks[idx].get(),
			3 + idx/num_cols, idx%num_cols, 1, 1);
	}
	mainlayout->addWidget(frame2, 4 + s_num_lengths/4, 0, 1, num_cols);
	mainlayout->addWidget(panel, 5 + s_num_lengths/4, 0, 1, num_cols);

	// restore settings
	QSettings settings{this};

	if(settings.contains("dlg_statistics/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_statistics/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		this->resize(512, 512);
	}

	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		QString key = QString{"dlg_statistics/length_check_%1"}.arg(idx);
		if(!settings.contains(key))
			continue;
		m_length_checks[idx]->setChecked(settings.value(key).toBool());
	}

	if(settings.contains("dlg_statistics/speed_check"))
		m_speed_check->setChecked(settings.value("dlg_statistics/speed_check").toBool());
}


Statistics::~Statistics()
{
}


void Statistics::SetTrackDB(const t_tracks *trackdb)
{
	m_trackdb = trackdb;
	PlotSpeeds();
}


void Statistics::ResetSpeedPlotRange()
{
	m_plot->xAxis->setRange(
		m_min_epoch - (m_max_epoch - m_min_epoch) / 20.,
		m_max_epoch + (m_max_epoch - m_min_epoch) / 20.);

	m_plot->yAxis->setRange(
		m_min_pace - (m_max_pace - m_min_pace) / 20.,
		m_max_pace + (m_max_pace - m_min_pace) / 20.);

	m_plot->replot();
}


void Statistics::PlotSpeeds()
{
	if(!m_trackdb || !m_plot)
		return;

	const t_size num_tracks = m_trackdb->GetTrackCount();
	const bool plot_speed = m_speed_check && m_speed_check->isChecked();

	m_plot->clearPlottables();
	if(plot_speed)
		m_plot->yAxis->setLabel("Speed (km/h)");
	else
		m_plot->yAxis->setLabel("Pace (min/km)");

	m_min_epoch = std::numeric_limits<t_real>::max();
	m_max_epoch = -m_min_epoch;
	m_min_pace = std::numeric_limits<t_real>::max();
	m_max_pace = -m_min_pace;

	QVector<t_real> epochs[s_num_lengths], paces[s_num_lengths];
	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		epochs[idx].reserve(num_tracks);
		paces[idx].reserve(num_tracks);
	}

	for(std::size_t track_idx = 0; track_idx < num_tracks; ++track_idx)
	{
		const t_track *track = m_trackdb->GetTrack(track_idx);
		if(!track)
			continue;

		// get track time
		auto tp = track->GetStartTime();
		if(!tp)
			continue;

		t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
			tp->time_since_epoch()).count();

		// get track pace
		t_real t = track->GetTotalTime();
		t_real s = track->GetTotalDistance(false);
		t_real pace = plot_speed
			? (s / 1000.) / (t / 60. / 60.)  // speed
			: (t / 60.) / (s / 1000.);       // pace

		bool is_visible = false;
		for(t_size idx = 0; idx < s_num_lengths; ++idx)
		{
			t_real min_len = s_lengths[idx];
			t_real max_len = (idx == 0
				? std::numeric_limits<t_real>::max()
				: s_lengths[idx - 1]);

			if(s / 1000. >= min_len && s / 1000. < max_len
				&& m_length_checks[idx]->isChecked())
			{
				epochs[idx].push_back(epoch);
				paces[idx].push_back(pace);
				is_visible = true;
			}
		}

		if(is_visible)
		{
			// ranges
			m_min_epoch = std::min(m_min_epoch, epoch);
			m_max_epoch = std::max(m_max_epoch, epoch);

			m_min_pace = std::min(m_min_pace, pace);
			m_max_pace = std::max(m_max_pace, pace);
		}
	}

	// create graph
	auto add_graph = [this](const QVector<t_real>& epochs, const QVector<t_real>& paces,
		t_size idx, t_size eff_idx, t_size total)
	{
		QCPGraph *graph = new QCPGraph(m_plot->xAxis, m_plot->yAxis);
		graph->setName(m_length_checks[idx]->text());

		int r = total < 2 ? 0 : std::lerp(0, 0xff, t_real(eff_idx) / t_real(total - 1));
		int g = 0;
		int b = total < 2 ? 0xff : std::lerp(0xff, 0, t_real(eff_idx) / t_real(total - 1));

		QPen pen = graph->pen();
		pen.setWidthF(2.);
		pen.setColor(QColor{r, g, b, 0xff});

		QBrush brush = graph->brush();
		brush.setStyle(Qt::SolidPattern);
		brush.setColor(QColor{r, g, b, 0x99});

		graph->setData(epochs, paces, true);
		graph->setLineStyle(QCPGraph::lsLine);
		graph->setScatterStyle(QCPScatterStyle{QCPScatterStyle::ssCircle, pen, brush, 6.});
		graph->setPen(pen);
		//graph->setBrush(brush);
	};

	// how many lengths are checked?
	t_size num_checked = 0;
	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		if(m_length_checks[idx]->isChecked())
			++num_checked;
	}

	// plot the checked lengths
	t_size eff_idx = 0;
	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		if(!m_length_checks[idx]->isChecked())
			continue;

		add_graph(epochs[idx], paces[idx],
			idx, eff_idx, num_checked);
		++eff_idx;
	}

	ResetSpeedPlotRange();
}


void Statistics::PlotMouseMove(QMouseEvent *evt)
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
	t_real pace = m_plot->yAxis->pixelToCoord(y);

	// get date string
	auto tp = t_track::t_timept{static_cast<typename t_track::t_time_ty>(
		epoch * 1000.) * std::chrono::milliseconds{1}};
	std::string date = from_timepoint<typename t_track::t_clk, typename t_track::t_timept>(
		tp, true, false);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << "Date: " << date;
	if(m_speed_check->isChecked())
		ostr << ", Speed: " << pace << " km/h.";
	else
		ostr << ", Pace: " << get_pace_str(pace) << ".";
	m_status->setText(ostr.str().c_str());
}


void Statistics::accept()
{
	// save settings
	QSettings settings{this};

	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_statistics/wnd_geo", geo);

	for(t_size idx = 0; idx < s_num_lengths; ++idx)
	{
		settings.setValue(QString("dlg_statistics/length_check_%1").arg(idx),
			m_length_checks[idx]->isChecked());
	}

	settings.setValue("dlg_statistics/speed_check", m_speed_check->isChecked());

	QDialog::accept();
}


void Statistics::reject()
{
	QDialog::reject();
}
