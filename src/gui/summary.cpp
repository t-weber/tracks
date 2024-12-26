/**
 * summary of all tracks
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 20-Dec-2024
 * @license see 'LICENSE' file
 */

#include "summary.h"
#include "helpers.h"
#include "tableitems.h"
#include "lib/calc.h"

#include <QtCore/QSettings>
#include <QtCore/QByteArray>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>


// table columns
#define TAB_NAME      0
#define TAB_DATE      1
#define TAB_DURATION  2
#define TAB_DISTANCE  3
#define TAB_PACE      4
#define TAB_HEIGHT    5
#define TAB_NUM_COLS  6


#define TRACK_IDX     Qt::UserRole + 0


Summary::Summary(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle("Track Summary");
	setSizeGripEnabled(true);

	// track table
	m_table = std::make_shared<QTableWidget>(this);
	m_table->setShowGrid(true);
	m_table->setSortingEnabled(true);
	m_table->setMouseTracking(true);
	m_table->setSelectionBehavior(QTableWidget::SelectRows);
	m_table->setSelectionMode(QTableWidget::SingleSelection);
	m_table->setAlternatingRowColors(true);

	m_table->setColumnCount(TAB_NUM_COLS);
	m_table->setHorizontalHeaderItem(TAB_NAME, new QTableWidgetItem{"Title"});
	m_table->setHorizontalHeaderItem(TAB_DATE, new QTableWidgetItem{"Date"});
	m_table->setHorizontalHeaderItem(TAB_DURATION, new QTableWidgetItem{"Duration"});
	m_table->setHorizontalHeaderItem(TAB_DISTANCE, new QTableWidgetItem{"Distance"});
	m_table->setHorizontalHeaderItem(TAB_PACE, new QTableWidgetItem{"Pace"});
	m_table->setHorizontalHeaderItem(TAB_HEIGHT, new QTableWidgetItem{"Height"});

	m_table->horizontalHeader()->setDefaultSectionSize(150);
	m_table->verticalHeader()->setDefaultSectionSize(24);
	m_table->verticalHeader()->setVisible(true);

	connect(m_table.get(), &QTableWidget::cellDoubleClicked,
		this, &Summary::TableDoubleClicked);

	// status bar
	m_status = std::make_shared<QLabel>(this);

	// button box
	m_buttonbox = std::make_shared<QDialogButtonBox>(this);
	m_buttonbox->setStandardButtons(QDialogButtonBox::Ok);
	m_buttonbox->button(QDialogButtonBox::Ok)->setDefault(true);

	connect(m_buttonbox.get(), &QDialogButtonBox::clicked,
		[this](QAbstractButton *btn) -> void
	{
		// get button role
		QDialogButtonBox::ButtonRole role = m_buttonbox->buttonRole(btn);

		if(role == QDialogButtonBox::AcceptRole)
			this->accept();
		else if(role == QDialogButtonBox::RejectRole)
			this->reject();
	});

	// status panel and its grid
	QWidget *status_panel = new QWidget{this};
	status_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QGridLayout *status_panel_layout = new QGridLayout(status_panel);
	status_panel_layout->setContentsMargins(0, 0, 0, 0);
	status_panel_layout->setVerticalSpacing(0);
	status_panel_layout->setHorizontalSpacing(0);
	status_panel_layout->addWidget(m_status.get(), 0, 0, 1, 3);
	status_panel_layout->addWidget(m_buttonbox.get(), 0, 3, 1, 1);

	// main grid
	QGridLayout *main_layout = new QGridLayout(this);
	main_layout->setContentsMargins(8, 8, 8, 8);
	main_layout->setVerticalSpacing(4);
	main_layout->setHorizontalSpacing(4);
	main_layout->addWidget(m_table.get(), 0, 0, 1, 1);
	main_layout->addWidget(status_panel, 1, 0, 1, 1);

	// restore settings
	QSettings settings{this};

	if(settings.contains("dlg_summary/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_summary/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
	else
	{
		this->resize(640, 480);
	}

	// table column widths
	if(settings.contains("dlg_summary/name_col"))
		m_table->setColumnWidth(TAB_NAME, settings.value("dlg_summary/name_col").toInt());
	else
		m_table->setColumnWidth(TAB_NAME, 150);
	if(settings.contains("dlg_summary/data_col"))
		m_table->setColumnWidth(TAB_DATE, settings.value("dlg_summary/date_col").toInt());
	else
		m_table->setColumnWidth(TAB_DATE, 150);
	if(settings.contains("dlg_summary/duration_col"))
		m_table->setColumnWidth(TAB_DURATION, settings.value("dlg_summary/duration_col").toInt());
	else
		m_table->setColumnWidth(TAB_DURATION, 115);
	if(settings.contains("dlg_summary/distance_col"))
		m_table->setColumnWidth(TAB_DISTANCE, settings.value("dlg_summary/distance_col").toInt());
	else
		m_table->setColumnWidth(TAB_DISTANCE, 115);
	if(settings.contains("dlg_summary/pace_col"))
		m_table->setColumnWidth(TAB_PACE, settings.value("dlg_summary/pace_col").toInt());
	else
		m_table->setColumnWidth(TAB_PACE, 115);
	if(settings.contains("dlg_summary/height_col"))
		m_table->setColumnWidth(TAB_HEIGHT, settings.value("dlg_summary/height_col").toInt());
	else
		m_table->setColumnWidth(TAB_HEIGHT, 115);
}


Summary::~Summary()
{
}


void Summary::SetTrackDB(const t_tracks *trackdb)
{
	m_trackdb = trackdb;
	FillTable();
}


void Summary::FillTable()
{
	if(!m_trackdb || !m_table)
		return;

	m_table->setSortingEnabled(false);
	m_table->clearContents();
	m_table->setRowCount(m_trackdb->GetTrackCount());

	// iterate all tracks
	for(t_size track_idx = 0; track_idx < m_trackdb->GetTrackCount(); ++track_idx)
	{
		const t_track *track = m_trackdb->GetTrack(track_idx);
		const int row = static_cast<int>(track_idx);
		const auto start_time = track->GetStartTime();
		const t_real epoch = start_time ? std::chrono::duration_cast<typename t_track::t_sec>(
			start_time->time_since_epoch()).count() : 0.;
		const t_real duration = track->GetTotalTime() / 60.;
		const t_real distance = track->GetTotalDistance() / 1000.;
		const auto [ min_elev, max_elev ] = track->GetElevationRange();
		const t_real height = max_elev - min_elev;

		m_table->setItem(row, TAB_NAME, new QTableWidgetItem{track->GetFileName().c_str()});
		m_table->setItem(row, TAB_DATE, new DateTimeTableWidgetItem<
			typename t_track::t_clk, typename t_track::t_timept, t_real>(epoch));
		m_table->setItem(row, TAB_DURATION, new NumericTableWidgetItem<t_real>(duration, g_prec_gui, " min"));
		m_table->setItem(row, TAB_DISTANCE, new NumericTableWidgetItem<t_real>(distance, g_prec_gui, " km"));
		m_table->setItem(row, TAB_PACE, new NumericTableWidgetItem<t_real>(duration / distance, g_prec_gui, " min/km"));
		m_table->setItem(row, TAB_HEIGHT, new NumericTableWidgetItem<t_real>(height, g_prec_gui, " m"));

		// set all items read-only
		for(int col = 0; col < TAB_NUM_COLS; ++col)
		{
			m_table->item(row, col)->setFlags(
				m_table->item(row, col)->flags() & ~Qt::ItemIsEditable);
		}

		// set the track index
		QVariant val;
		val.setValue(track_idx);
		m_table->item(row, TAB_NAME)->setData(TRACK_IDX, val);
	}

	m_table->setSortingEnabled(true);
}


/**
 * show the double-clicked track in the main window
 */
void Summary::TableDoubleClicked(int row, [[maybe_unused]] int col)
{
	if(!m_table)
		return;

	QVariant val = m_table->item(row, TAB_NAME)->data(TRACK_IDX);
	if(!val.isValid())
		return;

	t_size track_idx = val.value<t_size>();
	emit TrackSelected(track_idx);
}


void Summary::accept()
{
	// save settings
	QSettings settings{this};

	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_summary/wnd_geo", geo);

	if(m_table)
	{
		// table column widths
		settings.setValue("dlg_summary/name_col", m_table->columnWidth(TAB_NAME));
		settings.setValue("dlg_summary/date_col", m_table->columnWidth(TAB_DATE));
		settings.setValue("dlg_summary/duration_col", m_table->columnWidth(TAB_DURATION));
		settings.setValue("dlg_summary/distance_col", m_table->columnWidth(TAB_DISTANCE));
		settings.setValue("dlg_summary/pace_col", m_table->columnWidth(TAB_PACE));
		settings.setValue("dlg_summary/height_col", m_table->columnWidth(TAB_HEIGHT));
	}

	QDialog::accept();
}


void Summary::reject()
{
	QDialog::reject();
}
