/**
 * speed conversions
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_CONVERSIONS_H__
#define __TRACKS_CONVERSIONS_H__

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QDialogButtonBox>

// https://gcc.gnu.org/onlinedocs/gcc/Diagnostic-Pragmas.html
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <qcustomplot.h>
#pragma GCC diagnostic pop

#include <memory>

#include "../globals.h"


class Conversions : public QDialog
{ Q_OBJECT
public:
	Conversions(QWidget *parent = nullptr);
	virtual ~Conversions();


protected:
	virtual void accept() override;
	virtual void reject() override;

	void PlotMouseMove(QMouseEvent *evt);
	void PlotSpeeds();
	void ResetSpeedPlotRange();

	void CalcSpeed();
	void CalcPace();


private:
	std::shared_ptr<QCustomPlot> m_plot{};
	std::shared_ptr<QDoubleSpinBox> m_min_speed{}, m_max_speed{};
	std::shared_ptr<QDoubleSpinBox> m_speed{}, m_pace{};
	std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	t_real m_min_pace{}, m_max_pace{};
};


#endif
