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

#include <fstream>
#include <iostream>

#include "helpers.h"
#include "lib/trackdb.h"


#define TRACKS_WND_TITLE  "Tracks"
#define GUI_THEME_UNSET   "Unset"


// ----------------------------------------------------------------------------
// main window
// ----------------------------------------------------------------------------
TracksWnd::TracksWnd(QWidget* pParent)
	: QMainWindow{pParent},
		m_statusLabel{std::make_shared<QLabel>(this)}
{
	m_recent.SetOpenFile("");
	SetActiveFile();

	// allow dropping of files onto the main window
	setAcceptDrops(true);

	//m_recent.AddForbiddenDir("/home/tw/Documents");
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
	statusBar->addPermanentWidget(m_statusLabel.get(), 1);
	statusBar->setSizeGripEnabled(true);
	setStatusBar(statusBar);

	m_track = std::make_shared<DockWidgetWrapper<TrackInfos>>(this);
	m_track->setWindowTitle("Track Infos");
	m_track->setObjectName("TrackInfos");
	//setCentralWidget(m_track.get());
	addDockWidget(Qt::RightDockWidgetArea, m_track.get());

	m_tracks = std::make_shared<DockWidgetWrapper<TrackBrowser>>(this);
	m_tracks->setWindowTitle("Track Browser");
	m_tracks->setObjectName("TrackBrowser");
	addDockWidget(Qt::LeftDockWidgetArea, m_tracks.get());

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
	// tool bar
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


	// tool menu
	QIcon iconTools = QIcon::fromTheme("applications-system");
	QMenu *menuTools = new QMenu{"Tools", this};
	menuTools->setIcon(iconTools);
	menuTools->addAction(toolbarFile->toggleViewAction());
	menuTools->addSeparator();
	menuTools->addAction(m_tracks->toggleViewAction());
	menuTools->addAction(m_track->toggleViewAction());

	menuSettings->addAction(actionSettings);
	menuSettings->addAction(actionClearSettings);
	menuSettings->addSeparator();
	menuSettings->addMenu(menuTools);
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

	SetStatusMessage("Ready.");
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

	if(settings.contains("file_recent"))
		m_recent.SetRecentFiles(settings.value("file_recent").toStringList());

	if(settings.contains("file_recent_dir"))
		m_recent.SetRecentDir(settings.value("file_recent_dir").toString());

	if(settings.contains("file_recent_import_dir"))
		m_recent.SetRecentImportDir(settings.value("file_recent_import_dir").toString());

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
	settings.setValue("file_recent", m_recent.GetRecentFiles());
	settings.setValue("file_recent_dir", m_recent.GetRecentDir());
	settings.setValue("file_recent_import_dir", m_recent.GetRecentImportDir());
}


void TracksWnd::SetStatusMessage(const QString& msg)
{
	m_statusLabel->setText(msg);
}


void TracksWnd::Clear()
{
	m_tracks->GetWidget()->ClearTracks();
	m_recent.SetOpenFile("");
}


void TracksWnd::FileNew()
{
	if(!AskUnsaved())
		return;

	Clear();
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

	return true;
}


/**
 * an item from the recent files menu has been clicked
 */
bool TracksWnd::FileLoadRecent(const QString& filename)
{
	if(!AskUnsaved())
		return false;

	this->Clear();

	if(!LoadFile(filename))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be loaded.").arg(filename));
		return false;
	}

	m_recent.SetOpenFile(filename);
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

	if(SaveFile(files[0]))
	{
		fs::path file{files[0].toStdString()};
		m_recent.SetRecentDir(file.parent_path().string().c_str());
		m_recent.SetOpenFile(files[0]);

		m_recent.AddRecentFile(m_recent.GetOpenFile(),
			[this](const QString& filename) -> bool
		{
			return this->FileLoadRecent(filename);
		});

		return true;
	}
	else
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be saved.").arg(files[0]));
		return false;
	}
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
			QString("Selected files could not be imported.").arg(files[0]));
		return false;
	}

	fs::path file{files[0].toStdString()};
	m_recent.SetRecentImportDir(file.parent_path().string().c_str());

	return true;
}


/**
 * save session files
 */
bool TracksWnd::SaveFile(const QString& filename) const
{
	std::ofstream ofstr{filename.toStdString()};
	if(!ofstr)
		return false;

	// TODO

	return true;
}


/**
 * load session files
 */
bool TracksWnd::LoadFile(const QString& filename)
{
	std::ifstream ifstr{filename.toStdString()};
	if(!ifstr)
		return false;

	// TODO

	return true;
}


/**
 * import track files
 */
bool TracksWnd::ImportFiles(const QStringList& filenames)
{
	for(const QString& filename : filenames)
	{
		SingleTrack<t_real> track;
		if(!track.Import(filename.toStdString()))
		{
			QMessageBox::critical(this, "Error",
			QString("Track file \"%1\" could not be imported.").arg(filename));
			return false;
		}

		// TODO: load data

		m_tracks->GetWidget()->AddTrack(track.GetFileName());
	}

	return true;
}


/**
 * show settings dialog
 */
void TracksWnd::ShowSettings(bool only_create)
{
	if(!m_settings)
	{
		m_settings = std::make_shared<Settings>(this);
		connect(m_settings.get(), &Settings::SignalApplySettings,
			this, &TracksWnd::ApplySettings);

		m_settings->AddSpinbox("settings/precision_gui",
			"Number precision:", g_prec_gui, 0, 1, 1);
		m_settings->AddDoubleSpinbox("settings/epsilon",
			"Calculation epsilon:", g_eps, 0., 1., 1e-6, 8);
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

	update();
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
		setWindowTitle(QString(TRACKS_WND_TITLE "%1").arg(mod));
	}
	else
	{
		setWindowTitle(QString(TRACKS_WND_TITLE " \u2014 %1%2")
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
			return false;
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

	// load the dropped file
	Clear();
	if(!LoadFile(filename))
	{
		QMessageBox::critical(this, "Error",
			QString("File \"%1\" could not be loaded.").arg(filename));
		return;
	}

	fs::path file{filename.toStdString()};
	m_recent.SetRecentDir(file.parent_path().string().c_str());
	m_recent.SetOpenFile(filename);

	m_recent.AddRecentFile(m_recent.GetOpenFile(),
		[this](const QString& filename) -> bool
	{
		return this->FileLoadRecent(filename);
	});

	QMainWindow::dropEvent(evt);
}

// ----------------------------------------------------------------------------
