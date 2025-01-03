/**
 * distance reports
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_REPORTS_H__
#define __TRACKS_REPORTS_H__

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QSplitter>
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
#include <map>

#include "lib/trackdb.h"
#include "../globals.h"


/**
 * widget for listing tracks
 */
class Reports : public QDialog
{ Q_OBJECT
public:
	Reports(QWidget *parent = nullptr);
	virtual ~Reports();

	Reports(const Reports&) = delete;
	Reports& operator=(const Reports&) = delete;

	void SetTrackDB(const t_tracks *trackdb);
	void PlotDistances();
	void FillDistancesTable();
	void CalcDistances();


protected:
	virtual void accept() override;
	virtual void reject() override;

	void PlotMouseMove(QMouseEvent *evt);
	void PlotMouseClick(QMouseEvent *evt);
	void ResetDistPlotRange();
	void SavePlotPdf();


private:
	//std::shared_ptr<QCustomPlot> m_plot{};
	QCustomPlot *m_plot{};
	std::shared_ptr<QSplitter> m_split{};
	std::shared_ptr<QTableWidget> m_table{};
	std::shared_ptr<QCheckBox> m_all_tracks{}, m_cumulative{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};
	std::shared_ptr<QMenu> m_context{};

	const t_tracks *m_trackdb{};
	typename t_tracks::t_timept_map m_monthly{}, m_yearly{};

	t_real m_min_epoch{}, m_max_epoch{};
	t_real m_min_dist{}, m_max_dist{};

	// directory with recently saved pdf files
	std::string m_pdfdir{};
};


#endif
