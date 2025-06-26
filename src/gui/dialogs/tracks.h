/**
 * summary of all tracks
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 20-Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_SUMMARY_H__
#define __TRACKS_SUMMARY_H__

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialogButtonBox>

#include <memory>
#include <map>

#include "lib/trackdb.h"
#include "../globals.h"


/**
 * widget for listing tracks
 */
class TracksDlg : public QDialog
{ Q_OBJECT
public:
	TracksDlg(QWidget *parent = nullptr);
	virtual ~TracksDlg();

	TracksDlg(const TracksDlg&) = delete;
	TracksDlg& operator=(const TracksDlg&) = delete;

	void SetTrackDB(const t_tracks *trackdb);
	void FillTable();


protected:
	virtual void accept() override;
	virtual void reject() override;

	void TableDoubleClicked(int row, int col);

	void SearchTrack(const QString& name);
	void SearchNextTrack();


private:
	std::shared_ptr<QTableWidget> m_table{};
	std::shared_ptr<QLineEdit> m_search{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	const t_tracks *m_trackdb{};

	QList<QTableWidgetItem*> m_search_results{};


signals:
	void TrackSelected(t_size idx);
};


#endif
