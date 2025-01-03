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
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMenu>

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "lib/trackdb.h"
#include "../globals.h"


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
	void PlotSpeeds();


protected:
	virtual void accept() override;
	virtual void reject() override;

	void PlotMouseMove(QMouseEvent *evt);
	void PlotMouseClick(QMouseEvent *evt);
	void ResetSpeedPlotRange();
	void SavePlotPdf();


private:
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QCheckBox> m_speed_check{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};
	std::shared_ptr<QMenu> m_context{};

	static constexpr const t_real s_lengths[] = { 25., 20., 15., 10., 5., 0. };
	static constexpr const t_size s_num_lengths = sizeof(s_lengths)/sizeof(s_lengths[0]);
	std::shared_ptr<QCheckBox> m_length_checks[s_num_lengths]{};

	t_real m_min_epoch{}, m_max_epoch{};
	t_real m_min_pace{}, m_max_pace{};

	const t_tracks *m_trackdb{};

	// directory with recently saved pdf files
	std::string m_pdfdir{};
};


#endif
