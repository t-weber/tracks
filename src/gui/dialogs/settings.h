/**
 * settings
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2021
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_SETTINGS_H__
#define __TRACKS_SETTINGS_H__

#include <QtCore/QVariant>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QColor>
#include <QtWidgets/QDialog>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>

#include <memory>
#include <vector>

#include "common/types.h"


// get basic colours
extern const QColor& get_foreground_colour();
extern const QColor& get_background_colour();



/**
 * settings dialog
 */
class Settings : public QDialog
{ Q_OBJECT
public:
	Settings(QWidget *parent = nullptr, bool scrolling = false, bool tabs = false);
	virtual ~Settings() = default;

	Settings(const Settings&) = delete;
	const Settings& operator=(const Settings&) = delete;

	void AddTabPage(const QString& title);
	void AddCheckbox(const QString& key, const QString& descr, bool value);
	void AddSpinbox(const QString& key, const QString& descr,
		int value, int min = 0, int max = 100, int step = 1,
		const QString& suffix = "");
	void AddDoubleSpinbox(const QString& key, const QString& descr,
		double value, double min = 0, double max = 100, double step = 1,
		int decimals = 2, const QString& suffix = "");
	void AddCombobox(const QString& key, const QString& descr,
		const QStringList& items, int idx);
	void AddEditbox(const QString& key, const QString& descr, const QString& initial_value);
	void AddDirectorybox(const QString& key, const QString& descr, const QString& initial_value);
	void AddSpacer(int size_v = -1);
	void AddTabSpacers(int size_v = -1);
	void AddLine();
	void FinishSetup();

	void SetNumGridColumns(unsigned int num);
	void SetCurGridColumn(unsigned int num);

	QVariant GetValue(const QString& key) const;


protected:
	virtual void accept() override;
	virtual void reject() override;

	void ApplySettings();
	void RestoreDefaultSettings();


private:
	unsigned int m_numGridColumns{3};
	unsigned int m_curGridColumn{0};

	std::shared_ptr<QGridLayout> m_main_grid{}, m_grid{};
	//std::shared_ptr<QTabWidget> m_tabs{};
	QTabWidget *m_tabs{};
	std::vector<std::shared_ptr<QGridLayout>> m_tab_grids{};
	std::shared_ptr<QDialogButtonBox> m_buttonbox{};

	// check box, key, and initial value
	using t_checkboxinfo = std::tuple<QCheckBox*, QString, bool>;
	std::vector<t_checkboxinfo> m_checkboxes{};

	// spin box, key, and initial value
	using t_spinboxinfo = std::tuple<QSpinBox*, QString, int>;
	std::vector<t_spinboxinfo> m_spinboxes{};

	// double spin box, key, and initial value
	using t_doublespinboxinfo = std::tuple<QDoubleSpinBox*, QString, double>;
	std::vector<t_doublespinboxinfo> m_doublespinboxes{};

	// combo box, key, and initial value
	using t_comboboxinfo = std::tuple<QComboBox*, QString, int>;
	std::vector<t_comboboxinfo> m_comboboxes{};

	// edit box, key, and initial value
	using t_editboxinfo = std::tuple<QLineEdit*, QString, QString>;
	std::vector<t_editboxinfo> m_editboxes{};


signals:
	void SignalApplySettings();
};

#endif
