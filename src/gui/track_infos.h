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

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "globals.h"


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
	void ResetPlotRange();


private:
	std::shared_ptr<QSplitter> m_split{};
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QTextEdit> m_infos{};
	std::shared_ptr<QCheckBox> m_same_range{};

	t_real m_min_long{}, m_max_long{};
	t_real m_min_lat{}, m_max_lat{};


signals:
	void PlotCoordsChanged(t_real, t_real);
};


#endif
