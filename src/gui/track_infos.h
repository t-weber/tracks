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

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "common/types.h"


/**
 * widget for listing tracks
 */
class TrackInfos : public QWidget
{ Q_OBJECT
public:
	TrackInfos(QWidget *parent = nullptr);
	virtual ~TrackInfos();

	virtual QSize sizeHint() const override;


protected:
	void PlotMouseMove(QMouseEvent *evt);


private:
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QGridLayout> m_layout{};
};


#endif
