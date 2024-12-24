/**
 * map file loader
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14 December 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_MAP_FILE_H__
#define __TRACKS_MAP_FILE_H__

#include <sstream>
#include <iostream>
#include <iomanip>
#include <numbers>
#include <limits>
#include <string>
#include <list>
#include <vector>
#include <tuple>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace __map_fs = std::filesystem;
#else
	#include <boost/filesystem.hpp>
	namespace __map_fs = boost::filesystem;
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/geometry.hpp>

#ifdef _TRACKS_USE_OSMIUM_
	#include <osmium/io/any_input.hpp>
	#include <osmium/visitor.hpp>
	#include <osmium/handler.hpp>
#endif



template<class t_real = double>
struct MapVertex
{
	t_real longitude{};
	t_real latitude{};

	std::unordered_map<std::string, std::string> tags{};
};



template<class t_size = std::size_t>
struct MapSegment
{
	std::list<t_size> vertex_ids{};
	bool is_area{false};

	std::unordered_map<std::string, std::string> tags{};
};



template<class t_size = std::size_t>
struct MapMultiSegment
{
	std::list<t_size> vertex_ids{};
	std::list<t_size> segment_inner_ids{};
	std::list<t_size> segment_ids{};

	std::unordered_map<std::string, std::string> tags{};
};



template<class t_real = double, class t_size = std::size_t>
class Map
{
public:
	using t_vertex = MapVertex<t_real>;
	using t_segment = MapSegment<t_size>;
	using t_multisegment = MapMultiSegment<t_size>;



public:
	Map() = default;
	~Map() = default;



protected:
	bool ImportVertexXml(const boost::property_tree::ptree& node)
	{
		namespace num = std::numbers;

		auto id = node.get_optional<t_size>("<xmlattr>.id");
		auto lon = node.get_optional<t_real>("<xmlattr>.lon");
		auto lat = node.get_optional<t_real>("<xmlattr>.lat");
		auto vis = node.get_optional<bool>("<xmlattr>.visible");
		if(vis && !*vis)
			return false;

		if(!id || !lon || !lat)
			return false;

		t_vertex vertex
		{
			.longitude = *lon / t_real(180) * num::pi_v<t_real>,
			.latitude = *lat / t_real(180) * num::pi_v<t_real>,
		};

		if(auto tags = node.get_child_optional(""))
		{
			for(const auto& tag : *tags)
			{
				if(tag.first != "tag")
					continue;

				auto key = tag.second.get_optional<std::string>("<xmlattr>.k");
				auto val = tag.second.get_optional<std::string>("<xmlattr>.v");

				if(!key || !val)
					continue;

				vertex.tags.emplace(std::make_pair(*key, *val));
			}
		}

		// vertex ranges
		m_min_latitude = std::min(m_min_latitude, vertex.latitude);
		m_max_latitude = std::max(m_max_latitude, vertex.latitude);
		m_min_longitude = std::min(m_min_longitude, vertex.longitude);
		m_max_longitude = std::max(m_max_longitude, vertex.longitude);

		m_vertices.emplace(std::make_pair(*id, std::move(vertex)));
		return true;
	}



	bool ImportSegmentXml(const boost::property_tree::ptree& node)
	{
		auto id = node.get_optional<t_size>("<xmlattr>.id");
		auto vis = node.get_optional<bool>("<xmlattr>.visible");
		if(vis && !*vis)
			return false;

		t_segment seg;
		bool is_background = false;

		if(auto tags = node.get_child_optional(""))
		{
			for(const auto& tag : *tags)
			{
				if(tag.first == "nd")
				{
					auto vertex_id = tag.second.get_optional<t_size>("<xmlattr>.ref");
					if(!vertex_id)
						continue;

					seg.vertex_ids.push_back(*vertex_id);
				}
				else if(tag.first == "tag")
				{
					auto key = tag.second.get_optional<std::string>("<xmlattr>.k");
					auto val = tag.second.get_optional<std::string>("<xmlattr>.v");

					if(!key || !val)
						continue;

					if(!is_background && *key == "landuse")
						is_background = true;

					seg.tags.emplace(std::make_pair(*key, *val));
				}
			}
		}

		if(seg.vertex_ids.size() >= 2 && *seg.vertex_ids.begin() == *seg.vertex_ids.rbegin())
			seg.is_area = true;
		if(is_background)
			m_segments_background.emplace(std::make_pair(*id, std::move(seg)));
		else
			m_segments.emplace(std::make_pair(*id, std::move(seg)));
		return true;
	}



	bool ImportMultiSegmentXml(const boost::property_tree::ptree& node)
	{
		auto id = node.get_optional<t_size>("<xmlattr>.id");
		auto vis = node.get_optional<bool>("<xmlattr>.visible");
		if(vis && !*vis)
			return false;

		t_multisegment seg;

		if(auto tags = node.get_child_optional(""))
		{
			for(const auto& tag : *tags)
			{
				if(tag.first == "member")
				{
					auto seg_ty = tag.second.get_optional<std::string>("<xmlattr>.type");
					auto seg_ref = tag.second.get_optional<t_size>("<xmlattr>.ref");
					auto seg_role = tag.second.get_optional<std::string>("<xmlattr>.role");
					if(!seg_ty || !seg_ref)
						continue;

					if(*seg_ty == "node")
						seg.vertex_ids.push_back(*seg_ref);
					else if(*seg_ty == "way" && seg_role && *seg_role == "inner")
						seg.segment_inner_ids.push_back(*seg_ref);
					else if(*seg_ty == "way")
						seg.segment_ids.push_back(*seg_ref);

				}
				else if(tag.first == "tag")
				{
					auto key = tag.second.get_optional<std::string>("<xmlattr>.k");
					auto val = tag.second.get_optional<std::string>("<xmlattr>.v");

					if(!key || !val)
						continue;

					seg.tags.emplace(std::make_pair(*key, *val));
				}
			}
		}

		m_multisegments.emplace(std::make_pair(*id, std::move(seg)));
		return true;
	}



public:
	/**
	 * import a map from an osm file
	 * @see https://wiki.openstreetmap.org/wiki/OSM_XML
	 * @see https://wiki.openstreetmap.org/wiki/Elements
	 */
	bool ImportXml(const std::string& mapname)
	{
		namespace fs = __map_fs;
		namespace ptree = boost::property_tree;

		fs::path mapfile{mapname};
		if(!fs::exists(mapfile))
			return false;

		ptree::ptree map_root;
		ptree::read_xml(mapname, map_root);

		const auto& osm = map_root.get_child_optional("osm");
		if(!osm)
			return false;

		m_filename = mapfile.filename().string();
		m_version = osm->get<std::string>("<xmlattr>.version", "<unknown>");
		m_creator = osm->get<std::string>("<xmlattr>.generator", "<unknown>");

		// reset vertex ranges
		m_min_latitude = std::numeric_limits<t_real>::max();
		m_max_latitude = -m_min_latitude;
		m_min_longitude = std::numeric_limits<t_real>::max();
		m_max_longitude = -m_min_longitude;

		const auto& map_nodes = osm->get_child_optional("");
		if(!map_nodes)
			return false;

		for(const auto& node : *map_nodes)
		{
			// node is a vertex
			if(node.first == "node")
				ImportVertexXml(node.second);

			// node is a line segment or area
			else if(node.first == "way")
				ImportSegmentXml(node.second);

			// node is a relation
			else if(node.first == "relation")
				ImportMultiSegmentXml(node.second);
		}  // node iteration

		return true;
	}



#ifdef _TRACKS_USE_OSMIUM_
	/**
	 * import a map from an osm or a pbf file
	 * @see https://github.com/osmcode/libosmium/blob/master/examples/
	 * @see https://osmcode.org/libosmium/manual.html
	 * @see https://docs.osmcode.org/libosmium/latest/
	 */
	bool Import(const std::string& mapname,
		t_real min_longitude = -10., t_real max_longitude = 10.,
		t_real min_latitude = -10., t_real max_latitude = 10.)
	{
		namespace num = std::numbers;

		// reset vertex ranges
		m_min_latitude = std::numeric_limits<t_real>::max();
		m_max_latitude = -m_min_latitude;
		m_min_longitude = std::numeric_limits<t_real>::max();
		m_max_longitude = -m_min_longitude;

		struct OsmHandler : public osmium::handler::Handler
		{
		private:
			Map<t_real, t_size> *super = nullptr;
			const t_real *min_lon = nullptr, *max_lon = nullptr;
			const t_real *min_lat = nullptr, *max_lat = nullptr;


		public:
			OsmHandler(Map<t_real, t_size> *super,
				const t_real *min_lon, const t_real *max_lon,
				const t_real *min_lat, const t_real *max_lat)
				: super{super},
					min_lon{min_lon}, max_lon{max_lon},
					min_lat{min_lat}, max_lat{max_lat}
			{}


			void node(const osmium::Node& node)
			{
				if(!node.visible())
					return;

				t_real lon = node.location().lon() / t_real(180) * num::pi_v<t_real>;
				t_real lat = node.location().lat() / t_real(180) * num::pi_v<t_real>;

				// is the vertex within requested bounds?
				if(lon < *min_lon || lon > *max_lon ||
					lat < *min_lat || lat > *max_lat)
					return;

				t_vertex vertex
				{
					.longitude = lon,
					.latitude = lat,
				};

				for(const auto& tag : node.tags())
					vertex.tags.emplace(std::make_pair(tag.key(), tag.value()));

				// vertex ranges
				super->m_min_latitude = std::min(super->m_min_latitude, vertex.latitude);
				super->m_max_latitude = std::max(super->m_max_latitude, vertex.latitude);
				super->m_min_longitude = std::min(super->m_min_longitude, vertex.longitude);
				super->m_max_longitude = std::max(super->m_max_longitude, vertex.longitude);

				super->m_vertices.emplace(std::make_pair(node.id(), std::move(vertex)));
			}


			void way(const osmium::Way& way)
			{
				if(!way.visible())
					return;

				t_segment seg;
				//seg.vertex_ids.reserve(way.nodes().size());

				t_size invalid_refs = 0;
				for(const auto& node : way.nodes())
				{
					t_size ref = node.ref();
					seg.vertex_ids.push_back(ref);

					if(super->m_vertices.find(ref) == super->m_vertices.end())
						++invalid_refs;
				}

				// only referring to invalid vertices?
				if(invalid_refs == seg.vertex_ids.size())
					return;

				bool is_background = false;
				for(const auto& tag : way.tags())
				{
					seg.tags.emplace(std::make_pair(tag.key(), tag.value()));
					if(!is_background && std::string(tag.key()) == "landuse")
						is_background = true;
				}

				if(seg.vertex_ids.size() >= 2 && *seg.vertex_ids.begin() == *seg.vertex_ids.rbegin())
					seg.is_area = true;
				if(is_background)
					super->m_segments_background.emplace(std::make_pair(way.id(), std::move(seg)));
				else
					super->m_segments.emplace(std::make_pair(way.id(), std::move(seg)));
			}


			void relation(const osmium::Relation& rel)
			{
				t_multisegment seg;

				t_size invalid_node_refs = 0;
				t_size invalid_inner_seg_refs = 0;
				t_size invalid_seg_refs = 0;

				for(const auto& member : rel.members())
				{
					t_size ref = member.ref();

					if(member.type() == osmium::item_type::node)
					{
						seg.vertex_ids.push_back(ref);

						if(super->m_vertices.find(ref) == super->m_vertices.end())
							++invalid_node_refs;
					}
					else if(member.type() == osmium::item_type::way && std::string(member.role()) == "inner")
					{
						seg.segment_inner_ids.push_back(ref);

						if(super->m_segments.find(ref) == super->m_segments.end())
							++invalid_inner_seg_refs;
					}
					else if(member.type() == osmium::item_type::way)
					{
						seg.segment_ids.push_back(ref);

						if(super->m_segments.find(ref) == super->m_segments.end())
							++invalid_seg_refs;
					}
				}

				// only referring to invalid objects?
				if(invalid_node_refs == seg.vertex_ids.size() &&
					invalid_inner_seg_refs == seg.segment_inner_ids.size() &&
					invalid_seg_refs == seg.segment_ids.size())
					return;

				for(const auto& tag : rel.tags())
					seg.tags.emplace(std::make_pair(tag.key(), tag.value()));

				super->m_multisegments.emplace(std::make_pair(rel.id(), std::move(seg)));
			}

			/*void area(const osmium::Area& area)
			{
			}

			void tag_list(const osmium::TagList& tags)
			{
			}*/
		};

		try
		{
			OsmHandler osm_handler{this,
				&min_longitude, &max_longitude,
				&min_latitude, &max_latitude};

			osmium::io::Reader osm{mapname};
			osmium::apply(osm, osm_handler);
		}
		catch(const std::exception& ex)
		{
			std::cerr << ex.what() << std::endl;
			return false;
		}

		return true;
	}
#else
	bool Import(const std::string& mapname,
		t_real = -10., t_real = 10.,
		t_real = -10., t_real = 10.)
	{
		std::cerr << "Cannot import \"" << mapname
			<< "\" because osmium support is disabled."
			<< std::endl;
		return false;
	}
#endif  // _TRACKS_USE_OSMIUM_



	/**
	 * @see https://wiki.openstreetmap.org/wiki/Key:surface
	 */
	std::tuple<bool, int, int, int>
	GetSurfaceColour(const std::string& key, const std::string& val) const
	{
		if(key == "surface" && val == "asphalt")
			return std::make_tuple(true, 0x22, 0x22, 0x22);
		else if(key == "surface" && val == "concrete")
			return std::make_tuple(true, 0x33, 0x33, 0x33);
		else if(key == "natural" && val == "shingle")
			return std::make_tuple(true, 0x55, 0x55, 0xff);
		else if((key == "natural" || key == "surface") && val == "wood")
			return std::make_tuple(true, 0x00, 0x99, 0x00);
		else if(key == "natural" && val == "water")
			return std::make_tuple(true, 0x44, 0x44, 0xff);
		else if(key == "landuse" && val == "residential")
			return std::make_tuple(true, 0xaa, 0xaa, 0xaa);
		else if(key == "landuse" && val == "retail")
			return std::make_tuple(true, 0xff, 0x44, 0x44);
		else if(key == "landuse" && val == "industrial")
			return std::make_tuple(true, 0xaa, 0xaa, 0x44);
		else if(key == "landuse" && val == "forest")
			return std::make_tuple(true, 0x00, 0x99, 0x00);
		else if(key == "landuse" && (val == "grass" || val == "meadow"))
			return std::make_tuple(true, 0x44, 0xff, 0x44);
		else if(key == "waterway" && val == "river")
			return std::make_tuple(true, 0x55, 0x55, 0xff);
		else if(key == "building" /*&& val == "yes"*/)
			return std::make_tuple(true, 0xdd, 0xdd, 0xdd);
		else if(key == "leisure" && (val == "park" || val == "garden"))
			return std::make_tuple(true, 0x55, 0xff, 0x55);
		else if(key == "leisure" && val == "pitch")
			return std::make_tuple(true, 0x55, 0xbb, 0x55);

		return std::make_tuple(false, 0, 0, 0);
	}



	std::tuple<bool, std::string>
	GetSurfaceColourString(const std::string& key, const std::string& val,
		const std::string& def_col) const
	{
		auto [ ok, r, g, b ] = GetSurfaceColour(key, val);
		if(!ok)
			return std::make_tuple(ok, def_col);

		std::ostringstream col_ostr;
		col_ostr << "#"
			<< std::setw(2) << std::setfill('0') << std::hex << r
			<< std::setw(2) << std::setfill('0') << std::hex << g
			<< std::setw(2) << std::setfill('0') << std::hex << b;
		return std::make_tuple(ok, col_ostr.str());
	}



	/**
	 * @see https://wiki.openstreetmap.org/wiki/Key:highway
	 */
	std::tuple<bool, t_real>
	GetLineWidth(const std::string& key, const std::string& val,
		t_real def_line_width) const
	{
		if(key == "highway" && val == "motorway")
			return std::make_tuple(true, 70.);
		else if(key == "highway" && val == "motorway_link")
			return std::make_tuple(true, 65.);
		else if(key == "highway" && val == "trunk")
			return std::make_tuple(true, 60.);
		else if(key == "highway" && val == "primary")
			return std::make_tuple(true, 50.);
		else if(key == "highway" && val == "secondary")
			return std::make_tuple(true, 40.);
		else if(key == "highway" && val == "tertiary")
			return std::make_tuple(true, 30.);
		else if(key == "highway" && val == "residential")
			return std::make_tuple(true, 20.);
		else if(key == "highway" && val == "track")
			return std::make_tuple(true, 10.);

		return std::make_tuple(false, def_line_width);
	}



	/**
	 * write an svg file
	 * @see https://github.com/boostorg/geometry/tree/develop/example
	 */
	bool ExportSvg(const std::string& filename, t_real scale = 1) const
	{
		std::ofstream ofstr(filename);
		if(!ofstr)
			return false;

		return ExportSvg(ofstr, scale);
	}



	/**
	 * write an svg stream
	 * @see https://github.com/boostorg/geometry/tree/develop/example
	 */
	bool ExportSvg(std::ostream& ostr, t_real scale = 1,
		std::optional<t_real> min_lon = std::nullopt, std::optional<t_real> max_lon = std::nullopt,
		std::optional<t_real> min_lat = std::nullopt, std::optional<t_real> max_lat = std::nullopt) const
	{
		namespace geo = boost::geometry;
		namespace num = std::numbers;

		using t_vert = geo::model::point<t_real, 2, geo::cs::cartesian>;
		using t_line = geo::model::linestring<t_vert, std::vector>;
		using t_poly = geo::model::polygon<t_vert, true /*cw*/, false /*closed*/, std::vector>;
		using t_box = geo::model::box<t_vert>;
		using t_svg = geo::svg_mapper<t_vert, false, t_real>;

		// actual bounds from data
		t_real max_longitude = m_max_longitude;
		t_real min_longitude = m_min_longitude;
		t_real max_latitude = m_max_latitude;
		t_real min_latitude = m_min_latitude;

		// bounds for plot (which may be lower than the data bounds)
		if(min_lon)
			min_longitude = *min_lon;
		if(min_lat)
			min_latitude = *min_lat;
		if(max_lon)
			max_longitude = *max_lon;
		if(max_lat)
			max_latitude = *max_lat;

		//t_svg svg(ostr,
		//	scale * (max_longitude - min_longitude) * t_real(180) / num::pi_v<t_real>,
		//	scale * (max_latitude - min_latitude) * t_real(180) / num::pi_v<t_real>);
		int w = static_cast<int>(5000 * scale);
		int h = static_cast<int>(5000 * scale);
		t_svg svg(ostr, w, h,
			"width=\"" + std::to_string(w) + "px\" "
			"height=\"" + std::to_string(h) + "px\"");

		t_box frame(
			t_vert{
				min_longitude * t_real(180) / num::pi_v<t_real>,
				min_latitude * t_real(180) / num::pi_v<t_real> },
			t_vert{
				max_longitude * t_real(180) / num::pi_v<t_real>,
				max_latitude * t_real(180) / num::pi_v<t_real> });
		svg.add(frame);
		//svg.map(frame, "stroke:#ff0000; stroke-width:0.01px; fill:none;");

		// draw area
		std::unordered_set<t_size> seg_already_drawn;
		auto draw_seg = [this, &seg_already_drawn, &svg]
			(t_size id, const t_segment *seg = nullptr)
		{
			auto id_iter = seg_already_drawn.find(id);
			if(id_iter != seg_already_drawn.end())
				return;
			seg_already_drawn.insert(id);

			if(!seg)
			{
				auto iter = m_segments.find(id);
				if(iter == m_segments.end())
					return;

				seg = &iter->second;
			}

			if(!seg->is_area)
				return;
			t_poly poly;

			for(const t_size vert_id : seg->vertex_ids)
			{
				auto iter = m_vertices.find(vert_id);
				if(iter == m_vertices.end())
					continue;

				t_vert vert{
					iter->second.longitude * t_real(180) / num::pi_v<t_real>,
					iter->second.latitude * t_real(180) / num::pi_v<t_real>};
				poly.outer().push_back(vert);
			}

			std::string line_col = "#000000";
			std::string fill_col = "#ffffff";
			t_real line_width = 2.;
			bool found = false;

			for(const auto& [ tag_key, tag_val ] : seg->tags)
			{
				std::tie(found, fill_col) =
					GetSurfaceColourString(tag_key, tag_val, "#ffffff");
				if(found)
					break;
			}

			//svg.add(poly);
			svg.map(poly, "stroke:" + line_col +
				"; stroke-width:" + std::to_string(line_width) + "px" +
				"; fill:" + fill_col + ";", 1.);
		};

		// draw background areas
		for(const auto& [ id, seg ] : m_segments_background)
			draw_seg(id, &seg);

		// draw multi-areas
		for(const auto& [ multiseg_id, multiseg ] : m_multisegments)
		{
			for(const t_size id : multiseg.segment_ids)
				draw_seg(id);
			for(const t_size id : multiseg.segment_inner_ids)
				draw_seg(id);
		}

		// draw areas
		for(const auto& [ id, seg ] : m_segments)
			draw_seg(id, &seg);

		// draw streets
		for(const auto& [ id, seg ] : m_segments)
		{
			if(seg.is_area)
				continue;
			t_line line;

			for(const t_size vert_id : seg.vertex_ids)
			{
				auto iter = m_vertices.find(vert_id);
				if(iter == m_vertices.end())
					continue;

				t_vert vert{
					iter->second.longitude * t_real(180) / num::pi_v<t_real>,
					iter->second.latitude * t_real(180) / num::pi_v<t_real>};
				line.push_back(vert);
			}

			std::string line_col = "#222222";
			std::string fill_col = "none";
			t_real line_width = 8.;
			bool found_width = false, found_col = false;

			for(const auto& [ tag_key, tag_val ] : seg.tags)
			{
				if(!found_width)
					std::tie(found_width, line_width) =
						GetLineWidth(tag_key, tag_val, 8.);

				if(!found_col)
					std::tie(found_col, line_col) =
						GetSurfaceColourString(tag_key, tag_val, "#222222");

				if(found_width && found_col)
					break;
			}

			//svg.add(line);
			svg.map(line, "stroke:" + line_col +
				"; stroke-width:" + std::to_string(line_width) + "px"
				"; fill:" + fill_col + ";", 1.);
		}

		// draw track
		t_line line;
		for(const t_vertex& vertex : m_track)
		{
			t_vert vert{
				vertex.longitude * t_real(180) / num::pi_v<t_real>,
				vertex.latitude * t_real(180) / num::pi_v<t_real>};
			line.push_back(vert);
		}

		if(m_track.size())
		{
			//svg.add(line);
			svg.map(line, "stroke:#000000; "
				"stroke-width:48px; fill:none;", 1.);
			svg.map(line, "stroke:#ffff00; "
				"stroke-width:24px; fill:none;", 1.);
		}

		return true;
	}



	/**
	 * set a track to be displayed together with the map
	 */
	void SetTrack(std::vector<t_vertex>&& track)
	{
		m_track = std::forward<std::vector<t_vertex>&&>(track);
	}



protected:
	std::string m_filename{};
	std::string m_version{};
	std::string m_creator{};

	std::unordered_map<t_size, t_vertex> m_vertices{};
	std::unordered_map<t_size, t_segment> m_segments{};
	std::unordered_map<t_size, t_segment> m_segments_background{};
	std::unordered_map<t_size, t_multisegment> m_multisegments{};

	t_real m_min_latitude{}, m_max_latitude{};
	t_real m_min_longitude{}, m_max_longitude{};

	std::vector<t_vertex> m_track{};
};


#endif
