/**
 * speed statistics
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_STATS_H__
#define __TRACKS_STATS_H__

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDialogButtonBox>

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "globals.h"
#include "lib/trackdb.h"


/**
 * widget for listing tracks
 */
class Statistics : public QDialog
{ Q_OBJECT
public:
	Statistics(QWidget *parent = nullptr);
	virtual ~Statistics();

	Statistics(const Statistics&) = delete;
	Statistics& operator=(const Statistics&) = delete;

	void SetTrackDB(const t_tracks *trackdb);


protected:
	virtual void accept() override;
	virtual void reject() override;

	void PlotMouseMove(QMouseEvent *evt);
	void PlotSpeeds();


private:
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	const t_tracks *m_trackdb{};
};


#endif
