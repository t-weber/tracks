/**
 * track browser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_BROWSER_H__
#define __TRACKS_BROWSER_H__

#include <QtWidgets/QWidget>
#include <QtWidgets/QListWidget>

#include <memory>
#include <optional>

#include "globals.h"


/**
 * widget for listing tracks
 */
class TrackBrowser : public QWidget
{ Q_OBJECT
public:
	TrackBrowser(QWidget *parent = nullptr);
	virtual ~TrackBrowser();

	void CreateHeaders();
	void AddTrack(const std::string& ident, t_size idx,
		std::optional<t_real> timestamp = std::nullopt);
	void ClearTracks();

	t_size GetCurrentTrackIndex() const;
	t_size GetTrackIndex(int row) const;
	void SetTrackIndex(int row, t_size idx);

	std::optional<t_real> GetTrackTime(int row) const;

	void DeleteSelectedTracks();


protected:
	virtual QSize sizeHint() const override;


private:
	std::shared_ptr<QListWidget> m_list{};


signals:
	void NewTrackSelected(t_size idx);
	void TrackNameChanged(t_size idx, const std::string& name);
	void TrackDeleted(t_size idx);
};


#endif
