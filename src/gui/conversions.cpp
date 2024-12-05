/**
 * speed conversions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#include "conversions.h"
#include "helpers.h"
#include "lib/calc.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>

#include <sstream>
#include <limits>


Conversions::Conversions(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Speed Conversions");
	setSizeGripEnabled(true);

	// plot
	m_plot = std::make_shared<QCustomPlot>(this);
	m_plot->setSelectionRectMode(QCP::srmZoom);
	m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));
	m_plot->xAxis->setLabel("Speed (km/h)");
	m_plot->yAxis->setLabel("Pace (min/km)");

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &Conversions::PlotMouseMove);

	// plot settings
	QLabel *min_speed_label = new QLabel{"Min. Speed:", this};
	QLabel *max_speed_label = new QLabel{"Max. Speed:", this};

	m_min_speed = std::make_shared<QDoubleSpinBox>(this);
	m_max_speed = std::make_shared<QDoubleSpinBox>(this);

	m_min_speed->setValue(5.);
	m_min_speed->setMinimum(0.01);
	m_min_speed->setMaximum(99.);
	m_min_speed->setDecimals(2);
	m_min_speed->setSuffix(" km/h");
	m_max_speed->setValue(20.);
	m_max_speed->setMinimum(0.01);
	m_max_speed->setMaximum(99.);
	m_max_speed->setDecimals(2);
	m_max_speed->setSuffix(" km/h");

	auto value_changed = static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);
	connect(m_min_speed.get(), value_changed, this, &Conversions::PlotSpeeds);
	connect(m_max_speed.get(), value_changed, this, &Conversions::PlotSpeeds);

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
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_plot.get(), 0, 0, 1, 4);
	mainlayout->addWidget(min_speed_label, 1, 0, 1, 1);
	mainlayout->addWidget(max_speed_label, 1, 2, 1, 1);
	mainlayout->addWidget(m_min_speed.get(), 1, 1, 1, 1);
	mainlayout->addWidget(m_max_speed.get(), 1, 3, 1, 1);
	mainlayout->addWidget(panel, 2, 0, 1, 4);

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


void Conversions::PlotSpeeds()
{
	std::size_t num_points = 256;

	t_real min_speed = m_min_speed->value(), max_speed = m_max_speed->value();
	t_real min_pace = std::numeric_limits<t_real>::max(), max_pace = -min_pace;

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

		min_pace = std::min(min_pace, pace);
		max_pace = std::max(max_pace, pace);

		speeds.push_back(speed);
		paces.push_back(pace);
	}

	QCPCurve *curve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
	curve->setData(speeds, paces);
	curve->setLineStyle(QCPCurve::lsLine);
	QPen pen = curve->pen();
	pen.setWidthF(2.);
	curve->setPen(pen);

	m_plot->xAxis->setRange(
		min_speed - (max_speed - min_speed) / 20.,
		max_speed + (max_speed - min_speed) / 20.);
	m_plot->yAxis->setRange(
		min_pace - (max_pace - min_pace) / 20.,
		max_pace + (max_pace - min_pace) / 20.);
	m_plot->replot();
}


void Conversions::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

	t_real speed = m_plot->xAxis->pixelToCoord(evt->x());
	//t_real pace = m_plot->yAxis->pixelToCoord(evt->y());
	t_real pace = speed_to_pace<t_real>(speed);

	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << speed << " km/h \xe2\x89\x98 " << pace << " min/km";
	m_status->setText(ostr.str().c_str());
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
