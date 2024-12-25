/**
 * track info viewer
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_INFOS_H__
#define __TRACKS_INFOS_H__

#include <QtCore/QByteArray>
#include <QtCore/QSettings>
#include <QtWidgets/QWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLineEdit>

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "globals.h"
#include "map.h"


/**
 * widget for listing tracks
 */
class TrackInfos : public QWidget
{ Q_OBJECT
public:
	TrackInfos(QWidget *parent = nullptr);
	virtual ~TrackInfos();

	void SaveSettings(QSettings& settings);
	void RestoreSettings(QSettings& settings);

	void Clear();
	void ShowTrack(const t_track& track);


protected:
	virtual QSize sizeHint() const override;

	void PlotMouseMove(QMouseEvent *evt);
	void CalcPlotRange();
	void ResetPlotRange();

	void MapMouseMove(QMouseEvent *evt);
	void SelectMap();
	void PlotMap();


private:
	std::shared_ptr<QSplitter> m_split{};
	std::shared_ptr<QTextEdit> m_infos{};
	std::shared_ptr<QCheckBox> m_same_range{};
	std::shared_ptr<QLineEdit> m_mapfile{};
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<MapDrawer> m_map{};

	// track coordinate range
	t_real m_min_long{}, m_max_long{};
	t_real m_min_lat{}, m_max_lat{};

	// plotted coordinate range
	t_real m_min_long_plot{}, m_max_long_plot{};
	t_real m_min_lat_plot{}, m_max_lat_plot{};

	// currently selected track
	const t_track *m_track{};

	// directory with recently used map files
	std::string m_mapdir{};


signals:
	void PlotCoordsChanged(t_real, t_real);
};


#endif
