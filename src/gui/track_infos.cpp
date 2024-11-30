/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_infos.h"
#include "globals.h"
#include "helpers.h"

#include <QtWidgets/QFrame>


TrackInfos::TrackInfos(QWidget* parent)
	: QWidget(parent)
{
	m_plot = std::make_shared<QCustomPlot>(this);
	//m_plot->setSelectionRectMode(QCP::srmZoom);
	//m_plot->setInteraction(QCP::Interaction(int(QCP::iRangeZoom) | int(QCP::iRangeDrag)));

	m_plot->xAxis->setLabel("Longitude (Degrees)");
	m_plot->yAxis->setLabel("Latitude (Degrees)");

	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_plot.get(), 0, 0, 1, 1);

	connect(m_plot.get(), &QCustomPlot::mouseMove, this, &TrackInfos::PlotMouseMove);
}


TrackInfos::~TrackInfos()
{
}


void TrackInfos::PlotMouseMove(QMouseEvent *evt)
{
	if(!m_plot)
		return;

	t_real x = m_plot->xAxis->pixelToCoord(evt->x());
	t_real y = m_plot->yAxis->pixelToCoord(evt->y());

	std::string strCoord = std::to_string(x) + ", " + std::to_string(y);
	// TODO
}


/**
 * clear all widgets in the grid layout
 */
void TrackInfos::Clear()
{
	while(m_layout->count())
	{
		QLayoutItem* item = m_layout->itemAt(0);
		if(!item)
			break;
		m_layout->removeItem(item);

		if(item->widget())
			delete item->widget();
		delete item;
	}
}
