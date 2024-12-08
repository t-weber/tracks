/**
 * track browser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_browser.h"
#include "helpers.h"

#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMenu>

#include <limits>


// user data stored in list
#define TRACK_IDX    Qt::UserRole + 0
#define TRACK_EPOCH  Qt::UserRole + 1


TrackBrowser::TrackBrowser(QWidget* parent)
	: QWidget(parent)
{
	// track list
	m_list = std::make_shared<QListWidget>(this);
	m_list->setContextMenuPolicy(Qt::CustomContextMenu);
	m_list->setMouseTracking(true);
	m_list->setSortingEnabled(false);

	// list context menu
	QMenu *context_menu = new QMenu(m_list.get());
	context_menu->addAction(
		QIcon::fromTheme("list-remove"),
		"Delete Track", m_list.get(), [this]()
	{
		DeleteSelectedTracks();
	});

	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_list.get(), 0, 0, 1, 1);

	connect(m_list.get(), &QListWidget::currentRowChanged,
		[this](int row)
	{
		t_size idx = GetTrackIndex(row);
		emit NewTrackSelected(idx);
	});

	connect(m_list.get(), &QListWidget::itemChanged,
		[this](QListWidgetItem* item) -> void
	{
		if(!m_list || !item)
			return;

		int row = m_list->row(item);
		if(row < 0)
			return;

		std::string name = item->text().toStdString();
		t_size idx = GetTrackIndex(row);
		emit TrackNameChanged(idx, name);
	});

	connect(m_list.get(), &QListWidget::customContextMenuRequested,
		[this, context_menu](const QPoint& pt)
	{
		const QListWidgetItem* item = m_list->itemAt(pt);
		if(!item)
			return;

		context_menu->popup(m_list->mapToGlobal(pt));
	});
}


TrackBrowser::~TrackBrowser()
{
}


/**
 * create header items showing the month and year
 */
void TrackBrowser::CreateHeaders()
{
	if(!m_list)
		return;

	m_list->blockSignals(true);

	auto insert_header = [this](int row, int year, int month)
	{
		QString title = QString{"%1 / %2"}.arg(month).arg(year);

		QListWidgetItem *item = new QListWidgetItem{title/*, m_list.get()*/};
		item->setFlags(item->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsSelectable));

		// swap colours
		QBrush bg = item->background();
		QBrush fg = item->foreground();
		fg.setColor(QColor{0x00, 0x00, 0x00, 0xff});
		bg.setColor(QColor{0xff, 0xff, 0xff, 0xff});
		fg.setStyle(Qt::SolidPattern);
		bg.setStyle(Qt::SolidPattern);
		item->setBackground(fg);
		item->setForeground(bg);

		QFont font = item->font();
		font.setBold(true);
		item->setFont(font);

		m_list->insertItem(row, item);
	};

	for(int row = 0; row < m_list->count() - 1; ++row)
	{
		std::optional<t_real> cur_epoch = GetTrackTime(row);
		std::optional<t_real> next_epoch = GetTrackTime(row + 1);
		if(!cur_epoch || !next_epoch)
			continue;

		[[maybe_unused]] auto [cur_year, cur_mon, cur_day] =
			date_from_epoch<typename t_track::t_clk>(*cur_epoch);
		[[maybe_unused]] auto [next_year, next_mon, next_day] =
			date_from_epoch<typename t_track::t_clk>(*next_epoch);

		if(row == 0)
		{
			insert_header(row, cur_year, cur_mon);
		}

		if(cur_year != next_year || cur_mon != next_mon)
		{
			insert_header(row + 1, next_year, next_mon);
			++row;
		}
	}

	m_list->blockSignals(false);
}


void TrackBrowser::AddTrack(const std::string& name, t_size idx,
	std::optional<t_real> timestamp)
{
	if(!m_list)
		return;

	m_list->blockSignals(true);

	const int row = m_list->count();
	QListWidgetItem *item = new QListWidgetItem{name.c_str(), m_list.get()};
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	if(timestamp)
		item->setData(TRACK_EPOCH, QVariant{*timestamp});
	m_list->insertItem(row, item);
	SetTrackIndex(row, idx);

	m_list->blockSignals(false);
}


void TrackBrowser::ClearTracks()
{
	if(!m_list)
		return;

	m_list->clear();
}


t_size TrackBrowser::GetCurrentTrackIndex() const
{
	return GetTrackIndex(m_list->currentRow());
}


t_size TrackBrowser::GetTrackIndex(int row) const
{
	const t_size invalid = std::numeric_limits<t_size>::max();
	if(!m_list || row < 0)
		return invalid;

	QListWidgetItem *item = m_list->item(row);
	if(!item)
		return invalid;

	QVariant val = item->data(TRACK_IDX);
	if(!val.isValid())
		return invalid;

	return val.value<t_size>();
}


void TrackBrowser::SetTrackIndex(int row, t_size idx)
{
	if(!m_list || row < 0)
		return;

	QListWidgetItem *item = m_list->item(row);
	if(!item)
		return;

	QVariant val;
	val.setValue<t_size>(idx);
	item->setData(TRACK_IDX, val);
}


std::optional<t_real> TrackBrowser::GetTrackTime(int row) const
{
	if(!m_list || row < 0)
		return std::nullopt;

	QListWidgetItem *item = m_list->item(row);
	if(!item)
		return std::nullopt;

	QVariant val = item->data(TRACK_EPOCH);
	if(!val.isValid())
		return std::nullopt;

	return val.value<t_real>();
}


void TrackBrowser::DeleteSelectedTracks()
{
	auto decrease_indices = [this](int start_row, t_size deleted_idx)
	{
		for(int row = start_row; row < m_list->count(); ++row)
		{
			t_size idx = GetTrackIndex(row);
			if(idx <= deleted_idx)
				continue;
			SetTrackIndex(row, idx - 1);
		}
	};

	for(QListWidgetItem *item : m_list->selectedItems())
	{
		if(!item)
			continue;

		int row = m_list->row(item);
		t_size idx = GetTrackIndex(row);
		decrease_indices(row, idx);

		emit TrackDeleted(idx);
		delete item;
	}
}


QSize TrackBrowser::sizeHint() const
{
	QSize size = QWidget::sizeHint();
	size.rwidth() += 128;
	return size;
}
