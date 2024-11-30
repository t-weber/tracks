/**
 * helper functions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2021
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_HELPERS_H__
#define __TRACKS_HELPERS_H__

#include <QtCore/QLocale>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>

#include <tuple>
#include <cmath>
#include <locale>

#include "types.h"
#include "globals.h"


/**
 * interpolate between two colours
 */
template<class t_real = qreal> //requires m::is_scalar<t_real>
QColor lerp(const QColor& col1, const QColor& col2, t_real _t)
{
	using t_colreal = decltype(col1.redF());
	t_colreal t = static_cast<t_colreal>(_t);

	return QColor::fromRgbF(
		std::lerp(col1.redF(), col2.redF(), t),
		std::lerp(col1.greenF(), col2.greenF(), t),
		std::lerp(col1.blueF(), col2.blueF(), t),
		std::lerp(col1.alphaF(), col2.alphaF(), t)
	);
}


/**
 * set C locale
 */
static inline void set_c_locale()
{
	std::ios_base::sync_with_stdio(false);

	::setlocale(LC_ALL, "C");
	std::locale::global(std::locale("C"));
	QLocale::setDefault(QLocale::C);
}


/**
 * show a dialog
 * @see https://doc.qt.io/qt-5/qdialog.html#code-examples
 */
static inline void show_dialog(QWidget *dlg)
{
	if(!dlg)
		return;

	dlg->show();
	dlg->raise();
	dlg->activateWindow();
}

#endif
