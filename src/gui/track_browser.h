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
#include <QtWidgets/QGridLayout>

#include <memory>

#include "types.h"


/**
 * widget for listing tracks
 */
class TrackBrowser : public QWidget
{ Q_OBJECT
public:
	TrackBrowser(QWidget *parent = nullptr);
	virtual ~TrackBrowser();

	virtual QSize sizeHint() const override
	{
		QSize size = QWidget::sizeHint();
		size.rwidth() += 128;
		return size;
	}


protected:
	void Clear();


private:
	std::shared_ptr<QListWidget> m_list{};
	std::shared_ptr<QGridLayout> m_layout{};
};


#endif
