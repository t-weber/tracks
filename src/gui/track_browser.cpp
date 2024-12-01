/**
 * track browser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_browser.h"
#include "globals.h"
#include "helpers.h"

#include <QtWidgets/QListWidgetItem>


TrackBrowser::TrackBrowser(QWidget* parent)
	: QWidget(parent)
{
	m_list = std::make_shared<QListWidget>(this);

	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_list.get(), 0, 0, 1, 1);

	connect(m_list.get(), &QListWidget::currentRowChanged,
		this, &TrackBrowser::NewTrackSelected);
	connect(m_list.get(), &QListWidget::itemChanged,
		[this](QListWidgetItem* item) -> void
	{
		if(!m_list || !item)
			return;

		int row = m_list->row(item);
		if(row < 0)
			return;

		std::string name = item->text().toStdString();
		t_size idx = t_size(row);
		emit TrackNameChanged(idx, name);
	});
}


TrackBrowser::~TrackBrowser()
{
}


void TrackBrowser::AddTrack(const std::string& name)
{
	if(!m_list)
		return;

	m_list->blockSignals(true);

	QListWidgetItem *item = new QListWidgetItem{name.c_str(), m_list.get()};
	item->setFlags(item->flags() | Qt::ItemIsEditable);
	m_list->insertItem(m_list->count(), item);

	m_list->blockSignals(false);
}


void TrackBrowser::ClearTracks()
{
	if(!m_list)
		return;

	m_list->clear();
}


QSize TrackBrowser::sizeHint() const
{
	QSize size = QWidget::sizeHint();
	size.rwidth() += 128;
	return size;
}
