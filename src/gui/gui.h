/**
 * tracks main gui
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_GUI_H__
#define __TRACKS_GUI_H__

#include <QtCore/QByteArray>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDockWidget>

#include <memory>
#include <vector>

#include "recent.h"
#include "resources.h"
#include "globals.h"

// docks
#include "docks/track_browser.h"
#include "docks/track_infos.h"

// dialogs
#include "dialogs/conversions.h"
#include "dialogs/distance_calc.h"
#include "dialogs/paces.h"
#include "dialogs/distances.h"
#include "dialogs/tracks.h"
#include "dialogs/settings.h"
#include "dialogs/about.h"

// lib
#include "common/types.h"
#include "lib/trackdb.h"



/**
 * wrapper making a widget into a dock widget
 */
template<class t_widget>
class DockWidgetWrapper : public QDockWidget
{
public:
	DockWidgetWrapper(QWidget *parent = nullptr)
		: QDockWidget(parent)
	{
		this->setWidget(GetWidget());
	}

	virtual ~DockWidgetWrapper() = default;

	const t_widget* GetWidget() const
	{
		return &m_widget;
	}

	t_widget* GetWidget()
	{
		return &m_widget;
	}


private:
	t_widget m_widget{this};
};



class TracksWnd : public QMainWindow
{ Q_OBJECT
public:
	using QMainWindow::QMainWindow;

	TracksWnd(QWidget* pParent = nullptr);
	virtual ~TracksWnd();

	TracksWnd(const TracksWnd&) = delete;
	const TracksWnd& operator=(const TracksWnd&) = delete;

	void RestoreSettings();
	void SaveSettings();

	void SetupGUI();

	void Clear();
	void FileNew();
	bool FileLoad();
	bool FileSave();
	bool FileSaveAs();
	bool FileImport();

	bool FileLoadRecent(const QString& filename);

	bool SaveFile(const QString& filename) const;
	bool LoadFile(const QString& filename);
	bool ImportFiles(const QStringList& filenames);

	// dialogs
	void ShowSettings(bool only_create = false);
	void ShowConversions();
	void ShowDistances();
	void ShowPaceStatistics();
	void ShowDistanceReports();
	void ShowTracksSummary();
	void ShowAbout();

	Resources& GetResources() { return m_res; }
	const Resources& GetResources() const { return m_res; }


protected:
	virtual void closeEvent(QCloseEvent *) override;

	virtual void dragEnterEvent(QDragEnterEvent *) override;
	virtual void dropEvent(QDropEvent *) override;

	void SetActiveFile();
	bool AskUnsaved();
	void CreateTempDir();

	QString GetFileDir();
	QString GetImportDir();

	bool IsWindowModified() const { return m_window_modified; }
	void SetWindowModified(bool b) { m_window_modified = b; }

	t_track* GetTrack(t_size idx);


private:
	QString m_gui_theme{};
	bool m_gui_native{false};

	bool m_window_modified{false};

	QByteArray m_default_window_state{};
	QByteArray m_saved_window_state{};
	QByteArray m_saved_window_geometry{};

	Resources m_res{};
	RecentFiles m_recent{this, 16};

	//std::shared_ptr<QLabel> m_statusLabel{};
	std::shared_ptr<Settings> m_settings{};
	std::shared_ptr<About> m_about{};

	// docks
	std::shared_ptr<DockWidgetWrapper<TrackBrowser>> m_tracks{};
	std::shared_ptr<DockWidgetWrapper<TrackInfos>> m_track{};

	// dialogs
	std::shared_ptr<Conversions> m_conversions{};
	std::shared_ptr<Distances> m_distances{};
	std::shared_ptr<PacesDlg> m_statistics{};
	std::shared_ptr<DistancesDlg> m_reports{};
	std::shared_ptr<TracksDlg> m_summary{};

	t_tracks m_trackdb{};


protected slots:
	void ApplySettings();

	void NewTrackSelected(t_size idx);
	void TrackNameChanged(t_size idx, const std::string& name);
	void TrackDeleted(t_size idx);

	void PlotCoordsChanged(t_real longitude, t_real latitude,
		t_real distance, t_real time);
	void SetStatusMessage(const QString& msg, int display_ms = 0) const;
	void PopulateTrackList(bool resort = true);
};


#endif
