/**
 * speed conversions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#include "conversions.h"
#include "lib/calc.h"
#include "../helpers.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>

#include <sstream>
#include <limits>


Conversions::Conversions(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Speed Conversion");
	setSizeGripEnabled(true);

	// plot
	m_plot = std::make_shared<QCustomPlot>(this);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Speed (km/h)");
	m_plot->yAxis->setLabel("Pace (min/km)");

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &Conversions::PlotMouseMove);

	// plot reset button
	QPushButton *btn_replot = new QPushButton(this);
	btn_replot->setText("Reset Plot");
	connect(btn_replot, &QAbstractButton::clicked, this, &Conversions::ResetSpeedPlotRange);

	// plot settings
	QLabel *min_speed_label = new QLabel{"Min. Speed:", this};
	QLabel *max_speed_label = new QLabel{"Max. Speed:", this};
	QLabel *speed_label = new QLabel{"Speed:", this};
	QLabel *pace_label = new QLabel{"Pace:", this};

	m_min_speed = std::make_shared<QDoubleSpinBox>(this);
	m_max_speed = std::make_shared<QDoubleSpinBox>(this);

	m_min_speed->setValue(5.);
	m_min_speed->setRange(0.01, 99.);
	m_min_speed->setSingleStep(0.5);
	m_min_speed->setDecimals(2);
	m_min_speed->setSuffix(" km/h");

	m_max_speed->setValue(20.);
	m_max_speed->setRange(0.01, 99.);
	m_max_speed->setSingleStep(0.5);
	m_max_speed->setDecimals(2);
	m_max_speed->setSuffix(" km/h");

	QFrame *line = new QFrame{this};
	line->setFrameStyle(QFrame::HLine);
	line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_speed = std::make_shared<QDoubleSpinBox>(this);
	m_pace = std::make_shared<QDoubleSpinBox>(this);

	m_speed->setValue(10.);
	m_speed->setRange(0.01, 99.);
	m_speed->setSingleStep(0.1);
	m_speed->setDecimals(2);
	m_speed->setSuffix(" km/h");

	m_pace->setValue(speed_to_pace<t_real>(m_speed->value()));
	m_pace->setRange(speed_to_pace<t_real>(m_speed->maximum()), speed_to_pace<t_real>(m_speed->minimum()));
	m_pace->setSingleStep(0.1);
	m_pace->setDecimals(2);
	m_pace->setSuffix(" min/km");

	auto value_changed = static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);
	connect(m_min_speed.get(), value_changed, this, &Conversions::PlotSpeeds);
	connect(m_max_speed.get(), value_changed, this, &Conversions::PlotSpeeds);
	connect(m_speed.get(), value_changed, this, &Conversions::CalcPace);
	connect(m_pace.get(), value_changed, this, &Conversions::CalcSpeed);

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

	mainlayout->addWidget(m_plot.get(), 0, 0, 1, 4);
	mainlayout->addWidget(btn_replot, 1, 3, 1, 1);

	mainlayout->addWidget(min_speed_label, 2, 0, 1, 1);
	mainlayout->addWidget(max_speed_label, 2, 2, 1, 1);
	mainlayout->addWidget(m_min_speed.get(), 2, 1, 1, 1);
	mainlayout->addWidget(m_max_speed.get(), 2, 3, 1, 1);

	mainlayout->addWidget(line, 3, 0, 1, 4);
	mainlayout->addWidget(speed_label, 4, 0, 1, 1);
	mainlayout->addWidget(pace_label, 4, 2, 1, 1);
	mainlayout->addWidget(m_speed.get(), 4, 1, 1, 1);
	mainlayout->addWidget(m_pace.get(), 4, 3, 1, 1);

	mainlayout->addWidget(panel, 5, 0, 1, 4);

	// restore settings
	QSettings settings{this};
	if(settings.contains("dlg_conversions/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_conversions/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		this->resize(512, 512);
	}

	PlotSpeeds();
}


Conversions::~Conversions()
{
}


void Conversions::ResetSpeedPlotRange()
{
	t_real min_speed = m_min_speed->value();
	t_real max_speed = m_max_speed->value();

	m_plot->xAxis->setRange(
		min_speed - (max_speed - min_speed) / 20.,
		max_speed + (max_speed - min_speed) / 20.);

	m_plot->yAxis->setRange(
		m_min_pace - (m_max_pace - m_min_pace) / 20.,
		m_max_pace + (m_max_pace - m_min_pace) / 20.);

	m_plot->replot();
}


void Conversions::PlotSpeeds()
{
	std::size_t num_points = 256;

	t_real min_speed = m_min_speed->value();
	t_real max_speed = m_max_speed->value();
	m_min_pace = std::numeric_limits<t_real>::max();
	m_max_pace = -m_min_pace;

	if(min_speed > max_speed)
		std::swap(min_speed, max_speed);

	if(!m_plot)
		return;
	m_plot->clearPlottables();

	QVector<t_real> speeds, paces;
	speeds.reserve(num_points);
	paces.reserve(num_points);

	for(std::size_t pt = 0; pt < num_points; ++pt)
	{
		t_real speed = std::lerp(min_speed, max_speed, t_real(pt)/t_real(num_points - 1));
		t_real pace = speed_to_pace<t_real>(speed);

		m_min_pace = std::min(m_min_pace, pace);
		m_max_pace = std::max(m_max_pace, pace);

		speeds.push_back(speed);
		paces.push_back(pace);
	}

	QCPGraph *graph = new QCPGraph(m_plot->xAxis, m_plot->yAxis);
	graph->setData(speeds, paces, true);
	graph->setLineStyle(QCPGraph::lsLine);
	QPen pen = graph->pen();
	pen.setWidthF(2.);
	pen.setColor(QColor{0x00, 0x00, 0xff, 0xff});
	graph->setPen(pen);

	ResetSpeedPlotRange();
}


void Conversions::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	qreal x = evt->position().x();
#else
	qreal x = evt->x();
#endif

	t_real speed = m_plot->xAxis->pixelToCoord(x);
	t_real pace = speed_to_pace<t_real>(speed);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << speed << " km/h \xe2\x89\x98 " << get_pace_str(pace) << ".";
	m_status->setText(ostr.str().c_str());
}


void Conversions::CalcSpeed()
{
	m_speed->blockSignals(true);
	m_speed->setValue(speed_to_pace<t_real>(m_pace->value()));
	m_speed->blockSignals(false);
}


void Conversions::CalcPace()
{
	m_pace->blockSignals(true);
	m_pace->setValue(speed_to_pace<t_real>(m_speed->value()));
	m_pace->blockSignals(false);
}


void Conversions::accept()
{
	// save settings
	QSettings settings{this};
	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_conversions/wnd_geo", geo);

	QDialog::accept();
}


void Conversions::reject()
{
	QDialog::reject();
}
