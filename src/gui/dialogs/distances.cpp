/**
 * distance calculations
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Jan-2025
 * @license see 'LICENSE' file
 */

#include "distances.h"
#include "lib/calc.h"
#include "../helpers.h"
#include "../globals.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

#include <sstream>


Distances::Distances(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Distance Calculation");
	setSizeGripEnabled(true);

	// start and end points
	m_longitude_start = std::make_shared<QDoubleSpinBox>(this);
	m_latitude_start = std::make_shared<QDoubleSpinBox>(this);
	m_longitude_end = std::make_shared<QDoubleSpinBox>(this);
	m_latitude_end = std::make_shared<QDoubleSpinBox>(this);

	auto value_changed = static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged);

	for(QDoubleSpinBox *box : { m_latitude_start.get(), m_latitude_end.get() })
	{
		box->setValue(0.);
		box->setRange(-90., 90.);
		box->setSingleStep(0.0001);
		box->setDecimals(6);
		box->setSuffix("°");
		box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		connect(box, value_changed, this, &Distances::Calculate);
	}

	for(QDoubleSpinBox *box : { m_longitude_start.get(), m_longitude_end.get() })
	{
		box->setValue(0.);
		box->setRange(-180., 180.);
		box->setSingleStep(0.0001);
		box->setDecimals(6);
		box->setSuffix("°");
		box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		connect(box, value_changed, this, &Distances::Calculate);
	}

	// result
	m_result = std::make_shared<QLineEdit>(this);
	m_result->setReadOnly(true);

	// status bar
	//m_status = std::make_shared<QLabel>(this);

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
	//panellayout->addWidget(m_status.get(), 0, 0, 1, 3);
	panellayout->addWidget(m_buttonbox.get(), 0, 3, 1, 1);

	// labels
	QLabel *labStartLong = new QLabel("Start Longitude:", this);
	QLabel *labStartLat = new QLabel("Start Latitude:", this);
	QLabel *labEndLong = new QLabel("End Longitude:", this);
	QLabel *labEndLat = new QLabel("End Latitude:", this);
	QLabel *labResult = new QLabel("Distance:", this);
	for(QLabel *lab : { labStartLong, labStartLat, labEndLong, labEndLat, labResult })
		lab->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	// main grid
	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(8, 8, 8, 8);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(labStartLong, 0, 0, 1, 1);
	mainlayout->addWidget(m_longitude_start.get(), 0, 1, 1, 1);
	mainlayout->addWidget(labStartLat, 0, 2, 1, 1);
	mainlayout->addWidget(m_latitude_start.get(), 0, 3, 1, 1);
	mainlayout->addWidget(labEndLong, 1, 0, 1, 1);
	mainlayout->addWidget(m_longitude_end.get(), 1, 1, 1, 1);
	mainlayout->addWidget(labEndLat, 1, 2, 1, 1);
	mainlayout->addWidget(m_latitude_end.get(), 1, 3, 1, 1);
	mainlayout->addWidget(labResult, 2, 0, 1, 1);
	mainlayout->addWidget(m_result.get(), 2, 1, 1, 1);
	mainlayout->addItem(new QSpacerItem(4, 4, QSizePolicy::Preferred, QSizePolicy::Expanding), 3, 0, 1, 4);
	mainlayout->addWidget(panel, 4, 0, 1, 4);

	// restore settings
	QSettings settings{this};
	if(settings.contains("dlg_distances/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_distances/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		this->resize(512, 128);
	}

	Calculate();
}


Distances::~Distances()
{
}


/**
 * calculate the distance between the start and end coordinates
 */
void Distances::Calculate()
{
	namespace num = std::numbers;

	t_real long1 = m_longitude_start->value() / 180. * num::pi_v<t_real>;
	t_real lat1 = m_latitude_start->value() / 180. * num::pi_v<t_real>;
	t_real long2 = m_longitude_end->value() / 180. * num::pi_v<t_real>;
	t_real lat2 = m_latitude_end->value() / 180. * num::pi_v<t_real>;

	// get distance function
	std::tuple<t_real, t_real> (*dist_func)(t_real lat1, t_real lat2,
		t_real lon1, t_real lon2,
		t_real elev1, t_real elev2) = &geo_dist<t_real>;

	switch(g_dist_func)
	{
		case 0: dist_func = &geo_dist<t_real>; break;
		case 1: dist_func = &geo_dist_2<t_real, 1>; break;
		case 2: dist_func = &geo_dist_2<t_real, 2>; break;
		case 3: dist_func = &geo_dist_2<t_real, 3>; break;
	}

	auto [ dist_planar, dist ] = (*dist_func)(lat1, lat2, long1, long2, 0., 0.);

	std::ostringstream result;
	result.precision(g_prec_gui);

	if(dist < 1000.)
		result << dist << " m";
	else
		result << (dist / 1000.) << " km";

	m_result->setText(result.str().c_str());
}


void Distances::accept()
{
	// save settings
	QSettings settings{this};
	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_distances/wnd_geo", geo);

	QDialog::accept();
}


void Distances::reject()
{
	QDialog::reject();
}
