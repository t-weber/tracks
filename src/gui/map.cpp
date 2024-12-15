/**
 * map drawing
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 15-Dec-2024
 * @license see 'LICENSE' file
 */

#include "map.h"

#include <numbers>
namespace num = std::numbers;


MapPlotter::MapPlotter()
{
}


MapPlotter::~MapPlotter()
{
}


void MapPlotter::SetPlotRange(
	t_real min_lon, t_real max_lon,
	t_real min_lat, t_real max_lat)
{
	m_minmax_plot_longitude[0] = min_lon;
	m_minmax_plot_longitude[1] = max_lon;
	m_minmax_plot_latitude[0] = min_lat;
	m_minmax_plot_latitude[1] = max_lat;
}


bool MapPlotter::Plot(std::shared_ptr<QCustomPlot> plot) const
{
	// draw streets
	for(const auto& [ id, seg ] : m_segments)
	{
		if(seg.is_area)
			continue;

		// get vertices
		QVector<t_real> lons, lats;
		lons.reserve(seg.vertex_ids.size());
		lats.reserve(seg.vertex_ids.size());

		for(const t_size vert_id : seg.vertex_ids)
		{
			auto iter = m_vertices.find(vert_id);
			if(iter == m_vertices.end())
				continue;

			t_real lon = iter->second.longitude;
			t_real lat = iter->second.latitude;

			// only consider vertices within the given bounding rectangle
			if(lon < m_minmax_plot_longitude[0] || lon > m_minmax_plot_longitude[1])
				continue;
			if(lat < m_minmax_plot_latitude[0] || lat > m_minmax_plot_latitude[1])
				continue;

			lons.push_back(lon * t_real(180) / num::pi_v<t_real>);
			lats.push_back(lat * t_real(180) / num::pi_v<t_real>);
		}

		// get colour and line width
		int r = 0, g = 0, b = 0;
		t_real line_width = 1.;
		bool found_width = false, found_col = false;

		for(const auto& [ tag_key, tag_val ] : seg.tags)
		{
			if(!found_width)
				std::tie(found_width, line_width) =
					GetLineWidth(tag_key, tag_val, 1.);

			if(!found_col)
				std::tie(found_col, r, g, b) =
					GetSurfaceColour(tag_key, tag_val);

			if(found_width && found_col)
				break;
		}

		QCPCurve *curve = new QCPCurve(plot->xAxis, plot->yAxis);
		curve->setData(lons, lats);
		curve->setLineStyle(QCPCurve::lsLine);
		QPen pen = curve->pen();
		pen.setWidthF(line_width / 5.);
		pen.setColor(QColor{r, g, b, 0xff});
		curve->setPen(pen);
	}

	return true;
}
