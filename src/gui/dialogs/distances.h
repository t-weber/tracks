/**
 * distance calculations
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Jan-2025
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_DISTANCES_H__
#define __TRACKS_DISTANCES_H__

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>

#include <memory>

#include "../globals.h"


class Distances : public QDialog
{ Q_OBJECT
public:
	Distances(QWidget *parent = nullptr);
	virtual ~Distances();


protected:
	void Calculate();

	virtual void accept() override;
	virtual void reject() override;


private:
	std::shared_ptr<QDoubleSpinBox> m_longitude_start{};
	std::shared_ptr<QDoubleSpinBox> m_latitude_start{};
	std::shared_ptr<QDoubleSpinBox> m_longitude_end{};
	std::shared_ptr<QDoubleSpinBox> m_latitude_end{};
	std::shared_ptr<QLineEdit> m_result{};
	std::shared_ptr<QCheckBox> m_accept_coords{};

	//std::shared_ptr<QLabel> m_status{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	bool m_active_elem{true};  // start or end coordinates active?


public slots:
	void PlotCoordsSelected(t_real longitude, t_real latitude);
};


#endif
