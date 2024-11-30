/**
 * track browser
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "track_browser.h"
#include "globals.h"
#include "helpers.h"

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


/**
 * clear all widgets in the grid layout
 */
void TrackBrowser::Clear()
{
	while(m_layout->count())
	{
		QLayoutItem* item = m_layout->itemAt(0);
		if(!item)
			break;
		m_layout->removeItem(item);

		if(item->widget())
			delete item->widget();
		delete item;
	}
}
