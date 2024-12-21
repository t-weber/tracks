/**
 * table widget items
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Apr-2021, Dec-2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_TABITEMS_H__
#define __TRACKS_TABITEMS_H__

#include <QtWidgets/QTableWidgetItem>

#include <sstream>
#include <string>

#include "lib/timepoint.h"



template<class T = double>
class NumericTableWidgetItem : public QTableWidgetItem
{
public:
	NumericTableWidgetItem(const T& val, int prec = 6, const std::string& suffix = "")
		: QTableWidgetItem{}, m_val{val}, m_prec{prec}, m_suffix{suffix}
	{
		SetTextFromData();
	}


	virtual ~NumericTableWidgetItem() = default;


	virtual bool operator<(const QTableWidgetItem& _item) const override
	{
		T val{};

		if(const NumericTableWidgetItem<t_real> *item =
			dynamic_cast<const NumericTableWidgetItem<t_real>*>(&_item))
		{
			val = item->GetValue();
		}
		else
		{
			std::istringstream istr{_item.text().toStdString()};
			istr >> val;
		}

		return m_val < val;
	}


	virtual void setData(int itemdatarole, const QVariant& var) override
	{
		if(itemdatarole == Qt::EditRole)
		{
			m_val = var.value<T>();
			SetTextFromData();
		}

		QTableWidgetItem::setData(itemdatarole, var);
	}


	virtual QTableWidgetItem* clone() const override
	{
		return new NumericTableWidgetItem<T>(m_val, m_prec, m_suffix);
	}


	void SetTextFromData()
	{
		std::ostringstream ostr;
		ostr.precision(m_prec);
		ostr << m_val;

		setText((ostr.str() + m_suffix).c_str());
	}


	void SetSuffix(const std::string& str)
	{
		m_suffix = str;
	}


	void SetPrecision(int prec)
	{
		m_prec = prec;
	}


	T GetValue() const
	{
		return m_val;
	}


private:
	T m_val{};
	int m_prec{6};

	std::string m_suffix{};
};



template<class t_clk, class t_timept = typename t_clk::time_point, class t_epoch = double>
class DateTableWidgetItem : public QTableWidgetItem
{
public:
	DateTableWidgetItem(const t_epoch& val, bool show_month = true, const std::string& suffix = "")
		: QTableWidgetItem{}, m_val{val}, m_show_month{show_month}, m_suffix{suffix}
	{
		SetTextFromData();
	}


	virtual ~DateTableWidgetItem() = default;


	virtual bool operator<(const QTableWidgetItem& _item) const override
	{
		t_epoch val{};

		if(const DateTableWidgetItem<t_clk, t_timept, t_epoch> *item =
			dynamic_cast<const DateTableWidgetItem<t_clk, t_timept, t_epoch>*>(&_item))
		{
			val = item->GetValue();
		}
		else
		{
			std::istringstream istr{_item.text().toStdString()};
			istr >> val;
		}

		return m_val < val;
	}


	virtual void setData(int itemdatarole, const QVariant& var) override
	{
		if(itemdatarole == Qt::EditRole)
		{
			m_val = var.value<t_epoch>();
			SetTextFromData();
		}

		QTableWidgetItem::setData(itemdatarole, var);
	}


	virtual QTableWidgetItem* clone() const override
	{
		return new DateTableWidgetItem<t_clk, t_timept, t_epoch>(
			m_val, m_show_month, m_suffix);
	}


	void SetTextFromData()
	{
		auto [year, month, day] = date_from_epoch<t_clk, t_timept, t_epoch>(m_val);

		std::ostringstream ostr;

		if(m_show_month)
			ostr << std::setw(2) << std::setfill('0') << month << " / ";

		ostr << std::setw(4) << std::setfill('0') << year;
		ostr << m_suffix;

		setText(ostr.str().c_str());
	}


	void SetShowMonth(bool b)
	{
		m_show_month = b;
	}


	void SetSuffix(const std::string& str)
	{
		m_suffix = str;
	}


	t_epoch GetValue() const
	{
		return m_val;
	}


private:
	t_epoch m_val{};
	bool m_show_month{true};

	std::string m_suffix{};
};



template<class t_clk, class t_timept = typename t_clk::time_point, class t_epoch = double>
class DateTimeTableWidgetItem : public QTableWidgetItem
{
public:
	DateTimeTableWidgetItem(const t_epoch& val)
		: QTableWidgetItem{}, m_val{val}
	{
		SetTextFromData();
	}


	virtual ~DateTimeTableWidgetItem() = default;


	virtual bool operator<(const QTableWidgetItem& _item) const override
	{
		t_epoch val{};

		if(const DateTimeTableWidgetItem<t_clk, t_timept, t_epoch> *item =
			dynamic_cast<const DateTimeTableWidgetItem<t_clk, t_timept, t_epoch>*>(&_item))
		{
			val = item->GetValue();
		}
		else
		{
			std::istringstream istr{_item.text().toStdString()};
			istr >> val;
		}

		return m_val < val;
	}


	virtual void setData(int itemdatarole, const QVariant& var) override
	{
		if(itemdatarole == Qt::EditRole)
		{
			m_val = var.value<t_epoch>();
			SetTextFromData();
		}

		QTableWidgetItem::setData(itemdatarole, var);
	}


	virtual QTableWidgetItem* clone() const override
	{
		return new DateTimeTableWidgetItem<t_clk, t_timept, t_epoch>(m_val);
	}


	void SetTextFromData()
	{
		auto [year, month, day, hour, minute, second] =
			date_time_from_epoch<t_clk, t_timept, t_epoch>(m_val);

		std::ostringstream ostr;
		ostr << std::setw(2) << std::setfill('0') << day << "/";
		ostr << std::setw(2) << std::setfill('0') << month << "/";
		ostr << std::setw(4) << std::setfill('0') << year << " ";

		ostr << std::setw(2) << std::setfill('0') << hour << ":";
		ostr << std::setw(2) << std::setfill('0') << minute << ":";
		ostr << std::setw(2) << std::setfill('0') << second;

		setText(ostr.str().c_str());
	}


	t_epoch GetValue() const
	{
		return m_val;
	}


private:
	t_epoch m_val{};
};



#endif
