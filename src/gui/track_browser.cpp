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
#include <QtWidgets/QFrame>


TrackBrowser::TrackBrowser(QWidget* parent)
	: QWidget(parent)
{
	m_list = std::make_shared<QListWidget>(this);

	QGridLayout *mainlayout = new QGridLayout(this);
	mainlayout->setContentsMargins(4, 4, 4, 4);
	mainlayout->setVerticalSpacing(4);
	mainlayout->setHorizontalSpacing(4);
	mainlayout->addWidget(m_list.get(), 0, 0, 1, 1);
}


TrackBrowser::~TrackBrowser()
{
}


void TrackBrowser::AddTrack(const std::string& ident)
{
	if(!m_list)
		return;

	m_list->insertItem(m_list->count(),
		new QListWidgetItem{ident.c_str(), m_list.get()});
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
