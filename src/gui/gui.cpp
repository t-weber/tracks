/**
 * tracks main gui
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "gui.h"

#include <QtCore/QSettings>
#include <QtCore/QMimeData>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStyleFactory>
#include <QtSvg/QSvgGenerator>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	#include <QtGui/QActionGroup>
#else
	#include <QtWidgets/QActionGroup>
#endif

#include "helpers.h"
#include "common/version.h"


#define GUI_THEME_UNSET   "Unset"


// ----------------------------------------------------------------------------
// main window
// ----------------------------------------------------------------------------
TracksWnd::TracksWnd(QWidget* pParent) : QMainWindow{pParent}
{
	m_recent.SetOpenFile("");
	m_recent.SetLastOpenFile("");
	SetActiveFile();

	// allow dropping of files onto the main window
	setAcceptDrops(true);

	RestoreSettings();
}


TracksWnd::~TracksWnd()
{
	Clear();
}


/**
 * create all gui elements
 */
void TracksWnd::SetupGUI()
{
	if(auto optIconFile = m_res.FindFile("main.svg"); optIconFile)
		setWindowIcon(QIcon{optIconFile->string().c_str()});

	QStatusBar *statusBar = new QStatusBar{this};
	statusBar->setSizeGripEnabled(true);
	setStatusBar(statusBar);

	//m_statusLabel = std::make_shared<QLabel>(this);
	//m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	//statusBar->addPermanentWidget(m_statusLabel.get(), 1);

	m_track = std::make_shared<DockWidgetWrapper<TrackInfos>>(this);
	m_track->setWindowTitle("Track Infos");
	m_track->setObjectName("TrackInfos");
	//setCentralWidget(m_track.get());
	addDockWidget(Qt::RightDockWidgetArea, m_track.get());

	m_tracks = std::make_shared<DockWidgetWrapper<TrackBrowser>>(this);
	m_tracks->setWindowTitle("Track Browser");
	m_tracks->setObjectName("TrackBrowser");
	addDockWidget(Qt::LeftDockWidgetArea, m_tracks.get());
	connect(m_tracks->GetWidget(), &TrackBrowser::NewTrackSelected,
		this, &TracksWnd::NewTrackSelected);
	connect(m_tracks->GetWidget(), &TrackBrowser::TrackNameChanged,
		this, &TracksWnd::TrackNameChanged);
	connect(m_tracks->GetWidget(), &TrackBrowser::TrackDeleted,
		this, &TracksWnd::TrackDeleted);
	connect(m_tracks->GetWidget(), &TrackBrowser::StatusMessageChanged,
		[this](const QString& msg)
	{
		SetStatusMessage(msg);
	});
	connect(m_track->GetWidget(), &TrackInfos::PlotCoordsChanged,
		this, &TracksWnd::PlotCoordsChanged);
	connect(m_track->GetWidget(), &TrackInfos::StatusMessageChanged,
		[this](const QString& msg)
	{
		SetStatusMessage(msg);
	});
	connect(m_track->GetWidget(), &TrackInfos::TrackChanged, [this]()
	{
		SetWindowModified(true);
	});

	setDockOptions(QMainWindow::AnimatedDocks);
	setDockNestingEnabled(false);


	// ------------------------------------------------------------------------
	// file menu
	// menu actions
	QIcon iconNew = QIcon::fromTheme("document-new");
	QAction *actionNew = new QAction{iconNew, "New", this};
	connect(actionNew, &QAction::triggered, this, &TracksWnd::FileNew);

	QIcon iconLoad = QIcon::fromTheme("document-open");
	QAction *actionLoad = new QAction{iconLoad, "Load...", this};
	connect(actionLoad, &QAction::triggered, this, &TracksWnd::FileLoad);

	QIcon iconRecent = QIcon::fromTheme("document-open-recent");
	auto menuRecent = std::make_shared<QMenu>("Load Recent Files", this);
	menuRecent->setIcon(iconRecent);
	m_recent.SetRecentMenu(menuRecent);

	QIcon iconSave = QIcon::fromTheme("document-save");
	QAction *actionSave = new QAction{iconSave, "Save", this};
	connect(actionSave, &QAction::triggered, this, &TracksWnd::FileSave);

	QIcon iconSaveAs = QIcon::fromTheme("document-save-as");
	QAction *actionSaveAs = new QAction{iconSaveAs, "Save as...", this};
	connect(actionSaveAs, &QAction::triggered, this, &TracksWnd::FileSaveAs);

	QIcon iconImport = QIcon::fromTheme("document-open");
	QAction *actionImport = new QAction{iconImport, "Import Track...", this};
	connect(actionImport, &QAction::triggered, this, &TracksWnd::FileImport);

	QIcon iconExit = QIcon::fromTheme("application-exit");
	QAction *actionExit = new QAction{iconExit, "Quit", this};
	actionExit->setMenuRole(QAction::QuitRole);
	connect(actionExit, &QAction::triggered, [this]()
	{
		this->close();
	});


	QMenu *menuFile = new QMenu{"File", this};
	menuFile->addAction(actionNew);
	menuFile->addSeparator();
	menuFile->addAction(actionLoad);
	menuFile->addMenu(m_recent.GetRecentMenu().get());
	menuFile->addSeparator();
	menuFile->addAction(actionSave);
	menuFile->addAction(actionSaveAs);
	menuFile->addSeparator();
	menuFile->addAction(actionImport);
	menuFile->addSeparator();
	menuFile->addAction(actionExit);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// file tool bar
	QToolBar *toolbarFile = new QToolBar{"File", this};

	toolbarFile->setObjectName("FileToolbar");
	toolbarFile->addAction(actionNew);
	toolbarFile->addAction(actionLoad);
	toolbarFile->addAction(actionSave);
	toolbarFile->addAction(actionSaveAs);
	toolbarFile->addAction(actionImport);

	addToolBar(toolbarFile);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// tracks menu
	QMenu *menuTracks = new QMenu{"Tracks", this};

	QIcon iconRecalc = QIcon::fromTheme("view-refresh");
	QAction *actionRecalc = new QAction{iconRecalc, "Recalculate", this};
	connect(actionRecalc, &QAction::triggered, [this]()
	{
		m_trackdb.Calculate();
		if(m_statistics)
			m_statistics->PlotSpeeds();
		if(m_reports)
			m_reports->CalcDistances();
		if(m_summary)
			m_summary->FillTable();

		// refresh selected track
		if(m_tracks)
			NewTrackSelected(m_tracks->GetWidget()->GetCurrentTrackIndex());

		SetStatusMessage("Recalculated all values.");
	});

	QIcon iconResort = QIcon::fromTheme("view-sort-descending");
	QAction *actionResort = new QAction{iconResort, "Sort List", this};
	connect(actionResort, &QAction::triggered, [this]()
	{
		PopulateTrackList(true);
		SetStatusMessage("Resorted all tracks.");
	});

	QIcon iconStatistics = QIcon::fromTheme("applications-science");
	QAction *actionStatistics = new QAction{iconStatistics, "Pace Statistics...", this};
	connect(actionStatistics, &QAction::triggered, this, &TracksWnd::ShowStatistics);

	QIcon iconReports = QIcon::fromTheme("x-office-presentation");
	QAction *actionReports = new QAction{iconReports, "Distance Reports...", this};
	connect(actionReports, &QAction::triggered, this, &TracksWnd::ShowReports);

	QIcon iconSummary = QIcon::fromTheme("x-office-spreadsheet");
	QAction *actionSummary = new QAction{iconSummary, "Track Summary...", this};
	connect(actionSummary, &QAction::triggered, this, &TracksWnd::ShowSummary);

	menuTracks->addAction(actionRecalc);
	menuTracks->addAction(actionResort);
	menuTracks->addSeparator();
	menuTracks->addAction(actionStatistics);
	menuTracks->addAction(actionReports);
	menuTracks->addAction(actionSummary);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// tracks tool bar
	QToolBar *toolbarTracks = new QToolBar{"Tracks", this};

	toolbarTracks->setObjectName("TracksToolbar");
	toolbarTracks->addAction(actionStatistics);
	toolbarTracks->addAction(actionReports);
	toolbarTracks->addAction(actionSummary);

	addToolBar(toolbarTracks);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// tools menu
	QMenu *menuTools = new QMenu{"Tools", this};

	QIcon iconConversions = QIcon::fromTheme("applications-office");
	QAction *actionConversions = new QAction{iconConversions, "Speed Conversion...", this};
	connect(actionConversions, &QAction::triggered, this, &TracksWnd::ShowConversions);

	QIcon iconDistances = QIcon::fromTheme("accessories-calculator");
	QAction *actionDistances = new QAction{iconConversions, "Distance Calculation...", this};
	connect(actionDistances, &QAction::triggered, this, &TracksWnd::ShowDistances);

	menuTools->addAction(actionConversions);
	menuTools->addAction(actionDistances);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// tools tool bar
	QToolBar *toolbarTools = new QToolBar{"Tools", this};

	toolbarTools->setObjectName("TracksToolbar");
	toolbarTools->addAction(actionConversions);
	toolbarTools->addAction(actionDistances);

	addToolBar(toolbarTools);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// settings menu
	auto set_gui_theme = [this](const QString& theme) -> void
	{
		if(QStyle* style = QStyleFactory::create(theme); style)
		{
			this->setStyle(style);
			QApplication::setStyle(style);

			m_gui_theme = theme;
		}

		if(theme == GUI_THEME_UNSET)
			m_gui_theme = GUI_THEME_UNSET;
	};

	auto set_gui_native = [this](bool b) -> void
	{
		QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, !b);
		QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, !b);

		m_gui_native = b;
	};

	QMenu *menuSettings = new QMenu{"Settings", this};

	QIcon iconSettings = QIcon::fromTheme("preferences-system");
	QAction *actionSettings = new QAction{iconSettings, "Settings...", this};
	actionSettings->setMenuRole(QAction::PreferencesRole);
	connect(actionSettings, &QAction::triggered, this, &TracksWnd::ShowSettings);

	QIcon iconGuiTheme = QIcon::fromTheme("preferences-desktop-theme");
	QMenu *menuGuiTheme = new QMenu{"GUI Theme", this};
	menuGuiTheme->setIcon(iconGuiTheme);
	QActionGroup *groupTheme = new QActionGroup{this};
	QStringList themes = QStyleFactory::keys();
	themes.push_front(GUI_THEME_UNSET);

	for(const auto& theme : themes)
	{
		QAction *actionTheme = new QAction{theme, this};
		connect(actionTheme, &QAction::triggered, [set_gui_theme, theme]()
		{
			set_gui_theme(theme);
		});

		actionTheme->setCheckable(true);
		if(theme == m_gui_theme)
			actionTheme->setChecked(true);

		menuGuiTheme->addAction(actionTheme);
		groupTheme->addAction(actionTheme);

		if(theme == GUI_THEME_UNSET)
			menuGuiTheme->addSeparator();
	}

	// set native gui
	QIcon iconGuiNative = QIcon::fromTheme("preferences-desktop");
	QAction *actionGuiNative = new QAction{iconGuiNative, "Native GUI", this};
	actionGuiNative->setCheckable(true);
	actionGuiNative->setChecked(m_gui_native);
	connect(actionGuiNative, &QAction::triggered, [set_gui_native](bool checked)
	{
		set_gui_native(checked);
	});

	// clear settings
	QIcon iconClearSettings = QIcon::fromTheme("edit-clear");
	QAction *actionClearSettings = new QAction{iconClearSettings,
		"Clear All Settings", this};
	connect(actionClearSettings, &QAction::triggered, [this]()
	{
		QSettings{this}.clear();
	});

	// restore layout
	QIcon iconRestoreLayout = QIcon::fromTheme("view-restore");
	QAction *actionRestoreLayout = new QAction{iconRestoreLayout,
		"Restore GUI Layout", this};
	connect(actionRestoreLayout, &QAction::triggered, [this]()
	{
		if(m_default_window_state.size())
			this->restoreState(m_default_window_state);
	});


	// toolbar submenu
	QIcon iconToolbars = QIcon::fromTheme("applications-system");
	QMenu *menuToolbars = new QMenu{"Toolbars && Panels", this};
	menuToolbars->setIcon(iconToolbars);
	menuToolbars->addAction(toolbarFile->toggleViewAction());
	menuToolbars->addAction(toolbarTracks->toggleViewAction());
	menuToolbars->addSeparator();
	menuToolbars->addAction(m_tracks->toggleViewAction());
	menuToolbars->addAction(m_track->toggleViewAction());

	menuSettings->addAction(actionSettings);
	menuSettings->addAction(actionClearSettings);
	menuSettings->addSeparator();
	menuSettings->addMenu(menuToolbars);
	menuSettings->addSeparator();
	menuSettings->addMenu(menuGuiTheme);
	menuSettings->addAction(actionGuiNative);
	menuSettings->addAction(actionRestoreLayout);
	// ------------------------------------------------------------------------


	// ------------------------------------------------------------------------
	// about menu
	QMenu *menuHelp = new QMenu{"Help", this};

	QIcon iconAbout = QIcon::fromTheme("help-about");
	QAction *actionAbout = new QAction{iconAbout, "About...", this};
	actionAbout->setMenuRole(QAction::AboutRole);
	connect(actionAbout, &QAction::triggered, this, &TracksWnd::ShowAbout);

	menuHelp->addAction(actionAbout);
	// ------------------------------------------------------------------------


	// keyboard shortcuts
	actionNew->setShortcut(QKeySequence::New);
	actionLoad->setShortcut(QKeySequence::Open);
	actionSave->setShortcut(QKeySequence::Save);
	actionSaveAs->setShortcut(QKeySequence::SaveAs);
	actionExit->setShortcut(QKeySequence::Quit);
	actionSettings->setShortcut(QKeySequence::Preferences);


	// apply settings
	set_gui_theme(m_gui_theme);
	set_gui_native(m_gui_native);


	// menu bar
	QMenuBar *menuBar = new QMenuBar{this};
	menuBar->addMenu(menuFile);
	menuBar->addMenu(menuTracks);
	menuBar->addMenu(menuTools);
	menuBar->addMenu(menuSettings);
	menuBar->addMenu(menuHelp);
	setMenuBar(menuBar);


	// restore recent files menu
	m_recent.CreateRecentFileMenu(
		[this](const QString& filename) -> bool
	{
		return this->FileLoadRecent(filename);
	});

	// save initial default window state
	m_default_window_state = saveState();

	// restore saved window state and geometry
	if(m_saved_window_geometry.size())
		restoreGeometry(m_saved_window_geometry);
	if(m_saved_window_state.size())
		restoreState(m_saved_window_state);

	QSettings settings{this};
	m_track->GetWidget()->RestoreSettings(settings);

	// reload last open file
	bool loaded_last_file = false;
	if(g_reload_last && m_recent.GetLastOpenFile() != "")
	{
		if(fs::exists(m_recent.GetLastOpenFile().toStdString()))
			loaded_last_file = LoadFile(m_recent.GetLastOpenFile());
	}

	if(loaded_last_file)
	{
		m_recent.SetOpenFile(m_recent.GetLastOpenFile());
		SetActiveFile();

		if(m_trackdb.GetTrackCount() > 1)
			m_tracks->GetWidget()->SelectTrack(0);

		m_tracks->GetWidget()->SetFocus();
	}
	else
	{
		FileNew();
	}
}


/**
 * restore saved settings
 */
void TracksWnd::RestoreSettings()
{
	QSettings settings{this};

	if(settings.contains("wnd_geo"))
		m_saved_window_geometry = settings.value("wnd_geo").toByteArray();
	else
		resize(1024, 768);

	if(settings.contains("wnd_state"))
		m_saved_window_state = settings.value("wnd_state").toByteArray();

	if(settings.contains("wnd_theme"))
		m_gui_theme = settings.value("wnd_theme").toString();

	if(settings.contains("wnd_native"))
		m_gui_native = settings.value("wnd_native").toBool();

	m_recent.RestoreSettings(settings);

	// restore settings from settings dialog
	ShowSettings(true);
}


void TracksWnd::SaveSettings()
{
	QSettings settings{this};
	QByteArray geo{this->saveGeometry()}, state{this->saveState()};

	settings.setValue("wnd_geo", geo);
	settings.setValue("wnd_state", state);
	settings.setValue("wnd_theme", m_gui_theme);
	settings.setValue("wnd_native", m_gui_native);

	m_recent.SaveSettings(settings);
	m_track->GetWidget()->SaveSettings(settings);
}


void TracksWnd::SetStatusMessage(const QString& msg, int display_ms) const
{
	QStatusBar *status = statusBar();
	if(!status)
		return;
	status->showMessage(msg, display_ms);

	// permanent message
	//if(!m_statusLabel)
	//	return;
	//const_cast<TracksWnd*>(this)->m_statusLabel->setText(msg);
}


void TracksWnd::Clear()
{
	m_trackdb.ClearTracks();
	m_tracks->GetWidget()->ClearTracks();
	m_recent.SetOpenFile("");
	m_recent.SetLastOpenFile("");
	SetActiveFile();
}


/**
 * sorts the tracks and repopulates the list
 */
void TracksWnd::PopulateTrackList(bool resort)
{
	m_tracks->GetWidget()->ClearTracks();

	if(resort)
		m_trackdb.SortTracks();

	for(t_size trackidx = 0; trackidx < m_trackdb.GetTrackCount(); ++trackidx)
	{
		const t_track *track = m_trackdb.GetTrack(trackidx);
		if(!track)
			continue;

		t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
			track->GetStartTime()->time_since_epoch()).count();
		m_tracks->GetWidget()->AddTrack(track->GetFileName(), trackidx, epoch);
	}

	m_tracks->GetWidget()->CreateHeaders();
}


t_track* TracksWnd::GetTrack(t_size idx)
{
	t_track *track = m_trackdb.GetTrack(t_size(idx));
	if(!track)
	{
		QMessageBox::critical(this, "Error",
			QString("Invalid track index %1, only %2 tracks are available.")
			.arg(idx).arg(m_trackdb.GetTrackCount()));
		return nullptr;
	}

	return track;
}


void TracksWnd::NewTrackSelected(t_size idx)
{
	if(!m_track)
		return;

	if(idx >= m_trackdb.GetTrackCount())
	{
		// no track selected
		m_track->GetWidget()->Clear();
		return;
	}

	t_track *track = GetTrack(idx);
	m_track->GetWidget()->ShowTrack(track);
}


void TracksWnd::TrackNameChanged(t_size idx, const std::string& name)
{
	if(t_track *track = GetTrack(idx); track)
	{
		track->SetFileName(name);
		SetWindowModified(true);
	}
}


void TracksWnd::TrackDeleted(t_size idx)
{
	if(!m_track || idx >= m_trackdb.GetTrackCount())
		return;

	m_trackdb.DeleteTrack(idx);
}


void TracksWnd::PlotCoordsChanged(t_real longitude, t_real latitude)
{
	std::ostringstream ostr;
	ostr.precision(g_prec_gui);
	ostr << "Longitude: " << longitude << "°, Latitude: " << latitude << "°.";
	SetStatusMessage(ostr.str().c_str());
}


void TracksWnd::FileNew()
{
	if(!AskUnsaved())
		return;

	Clear();
	SetStatusMessage("Ready.");
}


/**
 * load session files
 */
bool TracksWnd::FileLoad()
{
	if(!AskUnsaved())
		return false;

	auto filedlg = std::make_shared<QFileDialog>(
		this, "Load Data", GetFileDir(),
		"Tracks Files (*.tracks);;All Files (* *.*)");
	filedlg->setAcceptMode(QFileDialog::AcceptOpen);
	filedlg->setDefaultSuffix("tracks");
	filedlg->setFileMode(QFileDialog::ExistingFile);

	if(!filedlg->exec())
		return false;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return false;

	Clear();
	if(!LoadFile(files[0]))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be loaded.").arg(files[0]));
		return false;
	}

	fs::path file{files[0].toStdString()};
	m_recent.SetRecentDir(file.parent_path().string().c_str());
	m_recent.SetOpenFile(files[0]);

	m_recent.AddRecentFile(m_recent.GetOpenFile(),
		[this](const QString& filename) -> bool
	{
		return this->FileLoadRecent(filename);
	});

	SetActiveFile();
	SetWindowModified(false);
	return true;
}


/**
 * an item from the recent files menu has been clicked
 */
bool TracksWnd::FileLoadRecent(const QString& filename)
{
	if(!AskUnsaved())
		return false;

	Clear();
	if(!LoadFile(filename))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be loaded.").arg(filename));
		return false;
	}

	m_recent.SetOpenFile(filename);
	SetActiveFile();
	SetWindowModified(false);
	return true;
}


/**
 * save session files
 */
bool TracksWnd::FileSave()
{
	// no open file, use "save as..." instead
	if(m_recent.GetOpenFile() == "")
		return FileSaveAs();

	if(!SaveFile(m_recent.GetOpenFile()))
	{
		QMessageBox::critical(this, "Error", "File could not be saved.");
		return false;
	}

	SetWindowModified(false);
	return true;
}


/**
 * save session files as
 */
bool TracksWnd::FileSaveAs()
{
	auto filedlg = std::make_shared<QFileDialog>(
		this, "Save Data", GetFileDir(),
		"Tracks Files (*.tracks)");
	filedlg->setAcceptMode(QFileDialog::AcceptSave);
	filedlg->setDefaultSuffix("tracks");
	filedlg->selectFile("untitled");
	filedlg->setFileMode(QFileDialog::AnyFile);

	if(!filedlg->exec())
		return false;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return false;

	if(!SaveFile(files[0]))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be saved.").arg(files[0]));
		return false;
	}

	fs::path file{files[0].toStdString()};
	m_recent.SetRecentDir(file.parent_path().string().c_str());
	m_recent.SetOpenFile(files[0]);

	m_recent.AddRecentFile(m_recent.GetOpenFile(),
		[this](const QString& filename) -> bool
	{
		return this->FileLoadRecent(filename);
	});

	SetActiveFile();
	SetWindowModified(false);
	return true;
}


/**
 * import track files
 */
bool TracksWnd::FileImport()
{
	auto filedlg = std::make_shared<QFileDialog>(
		this, "Import Tracks", GetImportDir(),
		"Track Files (*.gpx);;All Files (* *.*)");
	filedlg->setAcceptMode(QFileDialog::AcceptOpen);
	filedlg->setDefaultSuffix("gpx");
	filedlg->setFileMode(QFileDialog::ExistingFiles);

	if(!filedlg->exec())
		return false;

	QStringList files = filedlg->selectedFiles();
	if(files.size() == 0 || files[0] == "")
		return false;

	if(!ImportFiles(files))
	{
		QMessageBox::critical(this, "Error",
			QString("Selected files could not be imported."));
		return false;
	}

	fs::path file{files[0].toStdString()};
	m_recent.SetRecentImportDir(file.parent_path().string().c_str());

	if(QMessageBox::question(this, "Sort Tracks?",
		"New tracks have been inserted to the end of the list. "
		"Re-sort the track list?") == QMessageBox::Yes)
	{
		PopulateTrackList(true);
		SetStatusMessage(QString("%1 track(s) imported and sorted.").arg(files.size()));
	}

	SetWindowModified(true);
	return true;
}


/**
 * save session files
 */
bool TracksWnd::SaveFile(const QString& filename) const
{
	bool ok = m_trackdb.Save(filename.toStdString());
	if(ok)
		SetStatusMessage(QString("Saved file \"%1\".").arg(filename));
	else
		SetStatusMessage(QString("Error saving file \"%1\".").arg(filename));
	return ok;
}


/**
 * load session files
 */
bool TracksWnd::LoadFile(const QString& filename)
{
	if(!m_trackdb.Load(filename.toStdString()))
	{
		SetStatusMessage(QString("Error loading file \"%1\".").arg(filename));
		return false;
	}

	PopulateTrackList(false);

	SetStatusMessage(QString("Loaded %1 tracks from file \"%2\". Total distance: %3 km.")
		.arg(m_trackdb.GetTrackCount())
		.arg(filename)
		.arg(m_trackdb.GetTotalDistance(false) / 1000.));
	return true;
}


/**
 * import track files
 */
bool TracksWnd::ImportFiles(const QStringList& filenames)
{
	for(const QString& filename : filenames)
	{
		t_track track{};
		track.SetDistanceFunction(g_dist_func);
		track.SetAscentEpsilon(g_asc_eps);

		if(!track.Import(filename.toStdString(), g_assume_dt))
		{
			SetStatusMessage(QString("Error importing file \"%1\".").arg(filename));
			return false;
		}

		t_real epoch = std::chrono::duration_cast<typename t_track::t_sec>(
			track.GetStartTime()->time_since_epoch()).count();
		m_tracks->GetWidget()->AddTrack(track.GetFileName(), m_trackdb.GetTrackCount(), epoch);
		m_trackdb.EmplaceTrack(std::move(track));
	}

	SetStatusMessage(QString("%1 track(s) imported.").arg(filenames.size()));
	return true;
}


/**
 * show settings dialog
 */
void TracksWnd::ShowSettings(bool only_create)
{
	if(!m_settings)
	{
		// populate the settings dialog
		m_settings = std::make_shared<Settings>(this);
		connect(m_settings.get(), &Settings::SignalApplySettings,
			this, &TracksWnd::ApplySettings);

		m_settings->AddSpinbox("settings/precision_gui",
			"Number precision:", g_prec_gui, 0, 99, 1);
		m_settings->AddDoubleSpinbox("settings/epsilon",
			"Calculation epsilon:", g_eps, 0., 1., 1e-6, 8);
		m_settings->AddDoubleSpinbox("settings/epsilon_ascent",
			"Ascent epsilon:", g_asc_eps, 0.1, 999., 1., 1, " m");
		m_settings->AddCombobox("settings/distance_function",
			"Distance calculation:",
			{ "Haversine Formula", "Thomas Formula",
			"Vincenty Formula", "Karney Formula" },
			g_dist_func);
		m_settings->AddLine();
		m_settings->AddDoubleSpinbox("settings/assume_dt",
			"Assumed time interval:", g_assume_dt, 0.1, 99., 1., 1, " s");
		m_settings->AddDoubleSpinbox("settings/map_scale",
			"Map scaling factor:", g_map_scale, 0.01, 99., 1., 2);
		m_settings->AddDoubleSpinbox("settings/map_overdraw",
			"Map overdrawing factor:", g_map_overdraw, 0., 9., 0.1, 2);
		m_settings->AddCheckbox("settings/show_buildings",
			"Show buildings in map", g_map_show_buildings);
		m_settings->AddCheckbox("settings/show_labels",
			"Show labels in map", g_map_show_labels);
		m_settings->AddLine();
		m_settings->AddCheckbox("settings/load_last_file",
			"Reload last file on startup", g_reload_last);
		m_settings->AddDirectorybox("settings/temp_dir",
			"Temporary directory:", g_temp_dir);
		m_settings->FinishSetup();
	}

	if(!only_create)
		show_dialog(m_settings.get());
}


/**
 * apply changed settings from the settings dialog
 */
void TracksWnd::ApplySettings()
{
	if(!m_settings)
		return;

	g_prec_gui = m_settings->GetValue("settings/precision_gui").
		value<decltype(g_prec_gui)>();
	g_eps = m_settings->GetValue("settings/epsilon").
		value<decltype(g_eps)>();
	g_asc_eps = m_settings->GetValue("settings/epsilon_ascent").
		value<decltype(g_asc_eps)>();
	g_dist_func = m_settings->GetValue("settings/distance_function").
		value<decltype(g_dist_func)>();
	g_assume_dt = m_settings->GetValue("settings/assume_dt").
		value<decltype(g_assume_dt)>();
	g_map_scale = m_settings->GetValue("settings/map_scale").
		value<decltype(g_map_scale)>();
	g_map_overdraw = m_settings->GetValue("settings/map_overdraw").
		value<decltype(g_map_overdraw)>();
	g_map_show_buildings = m_settings->GetValue("settings/show_buildings").
		value<decltype(g_map_show_buildings)>();
	g_map_show_labels = m_settings->GetValue("settings/show_labels").
		value<decltype(g_map_show_labels)>();
	g_reload_last = m_settings->GetValue("settings/load_last_file").
		value<decltype(g_reload_last)>();
	g_temp_dir = m_settings->GetValue("settings/temp_dir").toString();

	CreateTempDir();
	m_trackdb.SetDistanceFunction(g_dist_func);
	m_trackdb.SetAscentEpsilon(g_asc_eps);
	update();
}


void TracksWnd::CreateTempDir()
{
	if(g_temp_dir == "")
		return;

	if(QDir{}.exists(g_temp_dir))
		return;

	if(!QDir{}.mkpath(g_temp_dir))
	{
		QMessageBox::critical(this, "Error",
			QString("Temporary directory \"%1\" could not be created.")
				.arg(g_temp_dir));
		g_temp_dir = "";
	}
}


/**
 * show speed conversion dialog
 */
void TracksWnd::ShowConversions()
{
	if(!m_conversions)
		m_conversions = std::make_shared<Conversions>(this);

	show_dialog(m_conversions.get());
}


/**
 * show distance calculation dialog
 */
void TracksWnd::ShowDistances()
{
	if(!m_distances)
	{
		m_distances = std::make_shared<Distances>(this);

		connect(m_track->GetWidget(), &TrackInfos::PlotCoordsSelected,
			m_distances.get(), &Distances::PlotCoordsSelected);
	}

	show_dialog(m_distances.get());
}


/**
 * show speed statistics dialog
 */
void TracksWnd::ShowStatistics()
{
	if(!m_statistics)
	{
		m_statistics = std::make_shared<Statistics>(this);
		m_statistics->SetTrackDB(&m_trackdb);
	}

	show_dialog(m_statistics.get());
}


/**
 * show speed statistics dialog
 */
void TracksWnd::ShowReports()
{
	if(!m_reports)
	{
		m_reports = std::make_shared<Reports>(this);
		m_reports->SetTrackDB(&m_trackdb);
	}

	show_dialog(m_reports.get());
}


/**
 * show track summary dialog
 */
void TracksWnd::ShowSummary()
{
	if(!m_summary)
	{
		m_summary = std::make_shared<Summary>(this);
		m_summary->SetTrackDB(&m_trackdb);

		connect(m_summary.get(), &Summary::TrackSelected,
			m_tracks->GetWidget(), &TrackBrowser::SelectTrack);
	}

	show_dialog(m_summary.get());
}


/**
 * show about dialog
 */
void TracksWnd::ShowAbout()
{
	if(!m_about)
	{
		QIcon icon = windowIcon();
		m_about = std::make_shared<About>(this, &icon);
	}

	show_dialog(m_about.get());
}


/**
 * show the active file in the window title
 */
void TracksWnd::SetActiveFile()
{
	const QString& filename = m_recent.GetOpenFile();
	setWindowFilePath(filename);

	QString mod{IsWindowModified() ? " *" : ""};
	if(filename == "")
	{
		setWindowTitle(QString(TRACKS_TITLE "%1").arg(mod));
	}
	else
	{
		setWindowTitle(QString(TRACKS_TITLE " \u2014 %1%2")
			.arg(QFileInfo{filename}.fileName())
			.arg(mod));
	}
}


/**
 * ask to save unsaved changes
 * @return ok to continue?
 */
bool TracksWnd::AskUnsaved()
{
	// unsaved changes?
	if(IsWindowModified())
	{
		QMessageBox::StandardButton btn = QMessageBox::question(
			this, "Save Changes?",
			"The workspace has unsaved changes. Save them now?",
			QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
			QMessageBox::Yes);

		if(btn == QMessageBox::Yes)
		{
			if(!FileSave())
				return false;
		}
		else if(btn == QMessageBox::Cancel)
		{
			return false;
		}
	}

	return true;
}


/**
 * get the directory to save session files in
 */
QString TracksWnd::GetFileDir()
{
	if(g_use_recent_dir)
		return m_recent.GetRecentDir();

	// use either the documents or the home dir
	QString path;
	QStringList dirs = QStandardPaths::standardLocations(
		QStandardPaths::DocumentsLocation);
	if(dirs.size())
		path = dirs[0];
	else
		path = QDir::homePath();

	QString subdir = "tracks_files";
	QDir dir(path);
	if(!dir.exists(subdir))
	{
		QMessageBox::StandardButton btn =
			QMessageBox::question(this, "Create document directory",
				QString("Create the directory \"%1\" under \"%2\"?").
					arg(subdir).
					arg(dir.absolutePath()),
				QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
				QMessageBox::Yes);

		if(btn == QMessageBox::Yes)
		{
			dir.mkdir(subdir);
			dir.cd(subdir);
		}
	}
	else
	{
		dir.cd(subdir);
	}

	return dir.absolutePath();
}


/**
 * get the directory to import tracks from
 */
QString TracksWnd::GetImportDir()
{
	if(g_use_recent_dir)
		return m_recent.GetRecentImportDir();

	// use either the documents or the home dir
	QString path;
	QStringList dirs = QStandardPaths::standardLocations(
		QStandardPaths::DocumentsLocation);
	if(dirs.size())
		path = dirs[0];
	else
		path = QDir::homePath();

	QDir dir(path);
	return dir.absolutePath();
}


void TracksWnd::closeEvent(QCloseEvent *evt)
{
	if(!AskUnsaved())
	{
		evt->ignore();
		return;
	}

	m_recent.SetLastOpenFile(m_recent.GetOpenFile());
	SaveSettings();

	QMainWindow::closeEvent(evt);
}


/**
 * accept a file dropped onto the main window
 * @see https://doc.qt.io/qt-5/dnd.html
 */
void TracksWnd::dragEnterEvent(QDragEnterEvent *evt)
{
	if(!evt)
		return;

	// accept urls
	if(evt->mimeData()->hasUrls())
		evt->accept();

	QMainWindow::dragEnterEvent(evt);
}


/**
 * accept a file dropped onto the main window
 * @see https://doc.qt.io/qt-5/dnd.html
 */
void TracksWnd::dropEvent(QDropEvent *evt)
{
	if(!evt)
		return;

	// get mime data dropped on the main window
	const QMimeData* dat = evt->mimeData();
	if(!dat || !dat->hasUrls())
		return;

	// get the list of urls dropped on the main window
	QList<QUrl> urls = dat->urls();
	if(urls.size() == 0)
		return;

	// use the first url for the file name
	const QUrl& url = *urls.begin();
	QString filename = url.path();

	fs::path file{filename.toStdString()};
	std::string ext = file.extension();

	// load or import the dropped file
	if(ext == ".tracks")
	{
		if(!AskUnsaved())
			return;

		Clear();
		if(!LoadFile(filename))
		{
			QMessageBox::critical(this, "Error",
				QString("File \"%1\" could not be loaded.").arg(filename));
			return;
		}

		m_recent.SetRecentDir(file.parent_path().string().c_str());
		m_recent.SetOpenFile(filename);
		SetActiveFile();

		m_recent.AddRecentFile(m_recent.GetOpenFile(),
			[this](const QString& filename) -> bool
		{
			return this->FileLoadRecent(filename);
		});
	}
	else if(ext == ".gpx")
	{
		if(!ImportFiles({ filename }))
		{
			QMessageBox::critical(this, "Error",
				QString("File \"%1\" could not be imported.").arg(filename));
			return;
		}

		m_recent.SetRecentImportDir(file.parent_path().string().c_str());
		SetWindowModified(true);
	}

	QMainWindow::dropEvent(evt);
}

// ----------------------------------------------------------------------------
