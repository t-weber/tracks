/**
 * map drawing
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_MAP_PLOT_H__
#define __TRACKS_MAP_PLOT_H__

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <QtSvg/QSvgWidget>

#include "globals.h"
#include "lib/map.h"


// --------------------------------------------------------------------------------
/**
 * widget for plotting maps using QCustomPlot
 */
class MapPlotter : public t_map
{
public:
	MapPlotter();
	~MapPlotter();

	void SetPlotRange(t_real min_lon, t_real max_lon, t_real min_lat, t_real max_lat);
	bool Plot(std::shared_ptr<QCustomPlot> plot) const;


protected:
	t_real m_minmax_plot_longitude[2]{ -10., 10. };
	t_real m_minmax_plot_latitude[2]{ -10., 10. };
};
// --------------------------------------------------------------------------------


// --------------------------------------------------------------------------------
/**
 * widget for plotting maps using QSvgWidget
 */
class MapDrawer : public QSvgWidget
{ Q_OBJECT
public:
	MapDrawer(QWidget *parent);
	virtual ~MapDrawer();


protected:
	virtual void mouseMoveEvent(QMouseEvent *evt) override;


signals:
	void MouseMoved(QMouseEvent *);
};
// --------------------------------------------------------------------------------


#endif
