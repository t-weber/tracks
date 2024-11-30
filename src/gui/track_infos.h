/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_INFOS_H__
#define __TRACKS_INFOS_H__

#include <QtWidgets/QWidget>
#include <QtWidgets/QGridLayout>
#include <qcustomplot.h>

#include <memory>

#include "types.h"


/**
 * widget for listing tracks
 */
class TrackInfos : public QWidget
{ Q_OBJECT
public:
	TrackInfos(QWidget *parent = nullptr);
	virtual ~TrackInfos();

	virtual QSize sizeHint() const override
	{
		QSize size = QWidget::sizeHint();
		size.rwidth() += 128;
		return size;
	}


protected:
	void Clear();

	void PlotMouseMove(QMouseEvent *evt);


private:
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QGridLayout> m_layout{};
};


#endif
