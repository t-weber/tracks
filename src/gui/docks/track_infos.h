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
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QCalendarWidget>

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "map.h"
#include "../globals.h"


/**
 * widget for listing tracks
 */
class TrackInfos : public QWidget
{ Q_OBJECT
public:
	TrackInfos(QWidget *parent = nullptr);
	virtual ~TrackInfos();

	TrackInfos(const TrackInfos&) = delete;
	TrackInfos& operator=(const TrackInfos&) = delete;

	void SaveSettings(QSettings& settings);
	void RestoreSettings(QSettings& settings);

	void Clear();
	void ShowTrack(t_track *track);


protected:
	virtual QSize sizeHint() const override;

	void PlotMouseClick(QMouseEvent *evt, QMenu *context, QWidget *plot);
	void SavePlotPdf(QCustomPlot *plot, const char *name);

	std::pair<t_real, t_real> GetTrackPlotCoords(QMouseEvent *evt, bool deg = true) const;
	void TrackPlotMouseMove(QMouseEvent *evt);
	void TrackPlotMouseClick(QMouseEvent *evt);
	void CalcTrackPlotRange();
	void ResetTrackPlotRange();
	void PlotTrack();

	std::pair<t_real, t_real> GetMapCoords(QMouseEvent *evt, bool deg = true) const;
	void MapMouseMove(QMouseEvent *evt);
	void MapMouseClick(QMouseEvent *evt);
	void SelectMap();
	void PlotMap(bool load_cached = false);
	void SaveMapSvg();

	void PacePlotMouseMove(QMouseEvent *evt);
	void ResetPacePlotRange();
	void PlotPace();

	void AltPlotMouseMove(QMouseEvent *evt);
	void ResetAltPlotRange();
	void PlotAlt();

	void CommentChanged();


private:
	// main layout
	std::shared_ptr<QSplitter> m_split{}, m_split_infos{};
	std::shared_ptr<QTabWidget> m_tab{};
	std::shared_ptr<QTextEdit> m_infos{};
	std::shared_ptr<QCalendarWidget> m_calendar{};

	// track tab
	std::shared_ptr<QCustomPlot> m_track_plot{};
	std::shared_ptr<QCheckBox> m_same_range{};
	std::shared_ptr<QMenu> m_track_context{};

	// map tab
	std::shared_ptr<MapDrawer> m_map{};
	std::shared_ptr<QLineEdit> m_mapfile{};
	std::shared_ptr<QMenu> m_map_context{};

	// comment tab
	std::shared_ptr<QTextEdit> m_comments{};

	// altitude tab
	std::shared_ptr<QCustomPlot> m_alt_plot{};
	std::shared_ptr<QCheckBox> m_time_check{};
	std::shared_ptr<QCheckBox> m_smooth_check{};
	std::shared_ptr<QMenu> m_alt_context{};

	// pace tab
	std::shared_ptr<QCustomPlot> m_pace_plot{};
	std::shared_ptr<QDoubleSpinBox> m_dist_binlen{};
	std::shared_ptr<QCheckBox> m_speed_check{};
	std::shared_ptr<QMenu> m_pace_context{};

	// svg image of the map
	QByteArray m_map_image{};

	// track coordinate ranges
	t_real m_min_long{}, m_max_long{};
	t_real m_min_lat{}, m_max_lat{};

	// altitutde range
	t_real m_min_dist_alt{}, m_max_dist_alt{};
	t_real m_min_alt{}, m_max_alt{};

	// plotted coordinate ranges
	t_real m_min_long_plot{}, m_max_long_plot{};
	t_real m_min_lat_plot{}, m_max_lat_plot{};

	// pace plot ranges
	t_real m_min_dist{}, m_max_dist{};
	t_real m_min_pace{}, m_max_pace{};

	// currently selected track
	t_track *m_track{};

	// directory with recently used map files
	std::string m_mapdir{};

	// directory with recently used svg files
	std::string m_svgdir{};


signals:
	void PlotCoordsChanged(t_real, t_real, t_real, t_real);  // mouse moved in track or map
	void PlotCoordsSelected(t_real, t_real);                 // mouse clicked in track or map
	void StatusMessageChanged(const QString&);
	void TrackChanged();                                     // the track has been modified
};


#endif
