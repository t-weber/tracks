/**
 * settings
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2021
 * @license see 'LICENSE' file
 */

#include "settings.h"

#include <QtCore/QSettings>
#include <QtGui/QGuiApplication>
#include <QtGui/QPalette>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QFileDialog>


const QColor& get_foreground_colour()
{
	QPalette palette = dynamic_cast<QGuiApplication*>(
		QGuiApplication::instance())->palette();
	const QColor& col = palette.color(QPalette::WindowText);

	return col;
}


const QColor& get_background_colour()
{
	QPalette palette = dynamic_cast<QGuiApplication*>(
		QGuiApplication::instance())->palette();
	const QColor& col = palette.color(QPalette::Window);

	return col;
}


// ----------------------------------------------------------------------------
// settings dialog
// ----------------------------------------------------------------------------

Settings::Settings(QWidget *parent, bool scrolling)
	: QDialog(parent)
{
	setWindowTitle("Settings");
	setSizeGripEnabled(true);


	// layout
	if(scrolling)
	{
		QScrollArea *scroll = new QScrollArea(this);
		QWidget *scrollwidget = new QWidget(scroll);
		scroll->setWidgetResizable(true);
		scroll->setWidget(scrollwidget);
		scroll->setFrameStyle(QFrame::NoFrame);

		m_grid = std::make_shared<QGridLayout>(scrollwidget);

		QGridLayout *mainlayout = new QGridLayout(this);
		mainlayout->setContentsMargins(0, 0, 0, 0);
		mainlayout->setSpacing(0);
		mainlayout->addWidget(scroll, 0, 0, 1, 1);
	}
	else
	{
		m_grid = std::make_shared<QGridLayout>(this);
	}

	m_grid->setSpacing(4);
	m_grid->setContentsMargins(8, 8, 8, 8);


	// button box
	m_buttonbox = std::make_shared<QDialogButtonBox>(this);
	m_buttonbox->setStandardButtons(
		QDialogButtonBox::Ok |
		QDialogButtonBox::Apply |
		QDialogButtonBox::Cancel |
		QDialogButtonBox::RestoreDefaults);
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
		else if(role == QDialogButtonBox::ApplyRole)
			ApplySettings();
		else if(role == QDialogButtonBox::ResetRole)
			RestoreDefaultSettings();
	});


	// restore settings
	QSettings settings{this};
	if(settings.contains("dlg_settings/wnd_geo"))
	{
		QByteArray arr{settings.value("dlg_settings/wnd_geo").toByteArray()};
		this->restoreGeometry(arr);
	}
}


void Settings::SetNumGridColumns(unsigned int num)
{
	m_numGridColumns = num;
}


void Settings::SetCurGridColumn(unsigned int num)
{
	m_curGridColumn = num;
}


/**
 * adds a check box to the end of the grid layout
 */
void Settings::AddCheckbox(const QString& key, const QString& descr, bool value)
{
	bool initial_value = value;

	// look for already saved value
	QSettings settings{this};
	if(settings.contains(key))
		value = settings.value(key).toBool();

	QCheckBox *box = new QCheckBox(this);
	box->setText(descr);
	box->setChecked(value);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 0, 1, m_numGridColumns);

	m_checkboxes.push_back(std::make_tuple(box, key, initial_value));
}


/**
 * adds a spin box to the end of the grid layout
 */
void Settings::AddSpinbox(const QString& key, const QString& descr,
	int value, int min, int max, int step, const QString& suffix)
{
	int initial_value = value;

	// look for already saved value
	QSettings settings{this};
	if(settings.contains(key))
		value = settings.value(key).value<decltype(value)>();

	QLabel *label = new QLabel(this);
	label->setText(descr);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QSpinBox *box = new QSpinBox(this);
	box->setValue(value);
	box->setMinimum(min);
	box->setMaximum(max);
	box->setSingleStep(step);
	box->setSuffix(suffix);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(label, row, m_curGridColumn*m_numGridColumns + 0, 1, 1);
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 1, 1, m_numGridColumns - 1);

	m_spinboxes.push_back(std::make_tuple(box, key, initial_value));
}


/**
 * adds a double box to the end of the grid layout
 */
void Settings::AddDoubleSpinbox(const QString& key, const QString& descr,
	double value, double min, double max, double step, int decimals,
	const QString& suffix)
{
	double initial_value = value;

	// look for already saved value
	QSettings settings{this};
	if(settings.contains(key))
		value = settings.value(key).value<decltype(value)>();

	QLabel *label = new QLabel(this);
	label->setText(descr);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QDoubleSpinBox *box = new QDoubleSpinBox(this);
	box->setDecimals(decimals);
	box->setValue(value);
	box->setMinimum(min);
	box->setMaximum(max);
	box->setSingleStep(step);
	box->setSuffix(suffix);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(label, row, m_curGridColumn*m_numGridColumns + 0, 1, 1);
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 1, 1, m_numGridColumns - 1);

	m_doublespinboxes.push_back(std::make_tuple(box, key, initial_value));
}


/**
 * adds a combo box to the end of the grid layout
 */
void Settings::AddCombobox(const QString& key, const QString& descr,
	const QStringList& items, int idx)
{
	int initial_idx = idx;

	// look for already saved value
	QSettings settings{this};
	if(settings.contains(key))
		idx = settings.value(key).value<decltype(idx)>();

	QLabel *label = new QLabel(this);
	label->setText(descr);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QComboBox *box = new QComboBox(this);
	box->addItems(items);
	box->setCurrentIndex(idx);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(label, row, m_curGridColumn*m_numGridColumns + 0, 1, 1);
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 1, 1, m_numGridColumns - 1);

	m_comboboxes.push_back(std::make_tuple(box, key, initial_idx));
}


/**
 * adds an edit box to the end of the grid layout
 */
void Settings::AddEditbox(const QString& key, const QString& descr,
	const QString& initial_value)
{
	// look for already saved value
	QString value = initial_value;
	QSettings settings{this};
	if(settings.contains(key))
		value = settings.value(key).toString();

	QLabel *label = new QLabel(this);
	label->setText(descr);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QLineEdit *box = new QLineEdit(this);
	box->setText(value);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(label, row, m_curGridColumn*m_numGridColumns + 0, 1, 1);
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 1, 1, m_numGridColumns - 1);

	m_editboxes.push_back(std::make_tuple(box, key, initial_value));
}


/**
 * adds a directory browser to the end of the grid layout
 */
void Settings::AddDirectorybox(const QString& key, const QString& descr,
	const QString& initial_value)
{
	// look for already saved value
	QString value = initial_value;
	QSettings settings{this};
	if(settings.contains(key))
		value = settings.value(key).toString();

	QLabel *label = new QLabel(this);
	label->setText(descr);
	label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	QLineEdit *box = new QLineEdit(this);
	box->setText(value);
	box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QPushButton *btn = new QPushButton(this);
	btn->setText("Browse...");
	btn->setToolTip("Browse for temporary directory.");
	btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	int row = m_grid->rowCount();
	m_grid->addWidget(label, row, m_curGridColumn*m_numGridColumns + 0, 1, 1);
	m_grid->addWidget(box, row, m_curGridColumn*m_numGridColumns + 1, 1, 1);
	m_grid->addWidget(btn, row, m_curGridColumn*m_numGridColumns + 2, 1, 1);

	m_editboxes.push_back(std::make_tuple(box, key, initial_value));

	// browse button handler
	connect(btn, &QAbstractButton::clicked, [this, box]()
	{
		auto filedlg = std::make_shared<QFileDialog>(this,
			"Select Directory", box->text());
		filedlg->setAcceptMode(QFileDialog::AcceptOpen);
		filedlg->setOptions(QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);
		filedlg->setFileMode(QFileDialog::Directory);

		if(!filedlg->exec())
			return;

		QStringList files = filedlg->selectedFiles();
		if(files.size() == 0 || files[0] == "")
			return;

		box->setText(files[0]);
	});
}


/**
 * adds a spacer to the end of the grid layout
 */
void Settings::AddSpacer(int size_v)
{
	QSizePolicy::Policy policy_h = QSizePolicy::Fixed;
	QSizePolicy::Policy policy_v = QSizePolicy::Fixed;

	// expanding spacer?
	if(size_v < 0)
	{
		policy_v = QSizePolicy::Expanding;
		size_v = 1;
	}

	QSpacerItem *spacer_end = new QSpacerItem(1, size_v, policy_h, policy_v);
	m_grid->addItem(spacer_end,
		m_grid->rowCount(), 0, 1, m_numGridColumns);
}


/**
 * adds a horizontal line to the end of the grid layout
 */
void Settings::AddLine()
{
	QFrame *frame = new QFrame(this);
	frame->setFrameStyle(QFrame::HLine);
	frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_grid->addWidget(frame,
		m_grid->rowCount(), 0, 1, m_numGridColumns);
}


/**
 * adds the button box at the end of the grid layout
 */
void Settings::FinishSetup()
{
	AddSpacer();
	m_grid->addWidget(m_buttonbox.get(),
		m_grid->rowCount(), 0, 1, m_numGridColumns);

	ApplySettings();
}


/**
 * get the settings value with the given key
 */
QVariant Settings::GetValue(const QString& key) const
{
	QVariant val;

	// look for the key among the check boxes
	for(auto& [box, box_key, initial] : m_checkboxes)
	{
		if(key != box_key)
			continue;
		val.setValue(box->isChecked());
		break;
	}

	if(val.isValid())
		return val;

	// look for the key among the spin boxes
	for(auto& [box, box_key, initial] : m_spinboxes)
	{
		if(key != box_key)
			continue;
		val.setValue(box->value());
		break;
	}

	if(val.isValid())
		return val;

	// look for the key among the double spin boxes
	for(auto& [box, box_key, initial] : m_doublespinboxes)
	{
		if(key != box_key)
			continue;
		val.setValue(box->value());
		break;
	}

	if(val.isValid())
		return val;

	// look for the key among the combo boxes
	for(auto& [box, box_key, initial] : m_comboboxes)
	{
		if(key != box_key)
			continue;
		val.setValue(box->currentIndex());
		break;
	}

	// look for the key among the edit boxes
	for(auto& [box, box_key, initial] : m_editboxes)
	{
		if(key != box_key)
			continue;
		val.setValue(box->text());
		break;
	}

	return val;
}


/**
 * save settings and notice any listeners
 */
void Settings::ApplySettings()
{
	// save current settings
	QSettings settings{this};
	for(const auto& [box, key, initial] : m_checkboxes)
		settings.setValue(key, box->isChecked());
	for(const auto& [box, key, initial] : m_spinboxes)
		settings.setValue(key, box->value());
	for(const auto& [box, key, initial] : m_doublespinboxes)
		settings.setValue(key, box->value());
	for(const auto& [box, key, initial] : m_comboboxes)
		settings.setValue(key, box->currentIndex());
	for(const auto& [box, key, initial] : m_editboxes)
		settings.setValue(key, box->text());

	// signal changes
	emit this->SignalApplySettings();
}


/**
 * restore the initial defaults
 */
void Settings::RestoreDefaultSettings()
{
	for(auto& [box, key, initial] : m_checkboxes)
		box->setChecked(initial);
	for(auto& [box, key, initial] : m_spinboxes)
		box->setValue(initial);
	for(auto& [box, key, initial] : m_doublespinboxes)
		box->setValue(initial);
	for(auto& [box, key, initial] : m_comboboxes)
		box->setCurrentIndex(initial);
	for(auto& [box, key, initial] : m_editboxes)
		box->setText(initial);

	// signal changes
	emit this->SignalApplySettings();
}


void Settings::accept()
{
	ApplySettings();

	// save settings
	QSettings settings{this};
	QByteArray geo{this->saveGeometry()};
	settings.setValue("dlg_settings/wnd_geo", geo);

	QDialog::accept();
}


void Settings::reject()
{
	QDialog::reject();
}

// ----------------------------------------------------------------------------
