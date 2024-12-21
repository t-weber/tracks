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
#include <QtWidgets/QDialogButtonBox>

#include <memory>
#include <map>

#include "globals.h"
#include "lib/trackdb.h"


/**
 * widget for listing tracks
 */
class Summary : public QDialog
{ Q_OBJECT
public:
	Summary(QWidget *parent = nullptr);
	virtual ~Summary();

	Summary(const Summary&) = delete;
	Summary& operator=(const Summary&) = delete;

	void SetTrackDB(const t_tracks *trackdb);
	void FillTable();


protected:
	virtual void accept() override;
	virtual void reject() override;


private:
	std::shared_ptr<QTableWidget> m_table{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	const t_tracks *m_trackdb{};
};


#endif
