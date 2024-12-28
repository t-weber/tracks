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
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace __map_fs = std::filesystem;
#else
	#include <boost/filesystem.hpp>
	namespace __map_fs = boost::filesystem;
#endif

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/geometry.hpp>

#if __has_include(<osmium/handler.hpp>) && defined(_TRACKS_CFG_USE_OSMIUM_)
	#define _TRACKS_USE_OSMIUM_ 1

	#include <osmium/io/any_input.hpp>
	#include <osmium/visitor.hpp>
	#include <osmium/handler.hpp>
#endif


#define MAP_MAGIC "TRACKMAP"



template<class t_real = double>
struct MapVertex
{
	t_real longitude{};
	t_real latitude{};

	std::unordered_map<std::string, std::string> tags{};
	bool referenced{false};
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
	/**
	 * remove unreferenced vertices
	 */
	void PruneVertices()
	{
		for(auto iter = m_vertices.begin(); iter != m_vertices.end();)
		{
			if(iter->second.referenced)
			{
				++iter;
				continue;
			}

			iter = m_vertices.erase(iter);
		}
	}



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
			.referenced = true,  // mark all as referenced
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

		auto place_iter = vertex.tags.find("place");
		auto name_iter = vertex.tags.find("name");
		if(place_iter != vertex.tags.end() && name_iter != vertex.tags.end())
		{
			if(!m_skip_labels)
			{
				// place label
				m_label_vertices.emplace(std::make_pair(*id, std::move(vertex)));
			}
		}
		else
		{
			// vertex
			m_vertices.emplace(std::make_pair(*id, std::move(vertex)));
		}

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
					if(m_skip_buildings && (*key == "building" 
						|| (*key == "leisure" && *val == "swimming_pool")))
						return false;

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

					if(m_skip_buildings && (*key == "building"
						|| (*key == "leisure" && *val == "swimming_pool")))
						return false;

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

		//PruneVertices();
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
		t_real min_latitude = -10., t_real max_latitude = 10.,
		std::function<bool(t_size, t_size)> *progress = nullptr,
		bool check_bounds = false)
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
			Map<t_real, t_size> *super{};
			const t_real *min_lon{}, *max_lon{};
			const t_real *min_lat{}, *max_lat{};

			const osmium::io::Reader* reader{};
			std::optional<t_size> last_offs{};
			std::function<bool(t_size, t_size)> *progress{};


		public:
			OsmHandler(Map<t_real, t_size> *super,
				const t_real *min_lon, const t_real *max_lon,
				const t_real *min_lat, const t_real *max_lat)
				: super{super},
					min_lon{min_lon}, max_lon{max_lon},
					min_lat{min_lat}, max_lat{max_lat}
			{}


			void SetReader(const osmium::io::Reader *reader)
			{
				this->reader = reader;
			}


			void SetProgressFunction(std::function<bool(t_size, t_size)> *progress)
			{
				this->progress = progress;
			}


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

				auto place_iter = vertex.tags.find("place");
				auto name_iter = vertex.tags.find("name");
				if(place_iter != vertex.tags.end() && name_iter != vertex.tags.end())
				{
					if(!super->m_skip_labels)
					{
						// place label
						super->m_label_vertices.emplace(std::make_pair(node.id(), std::move(vertex)));
					}
				}
				else
				{
					// vertex
					super->m_vertices.emplace(std::make_pair(node.id(), std::move(vertex)));
				}

				update_progress();
			}


			void way(const osmium::Way& way)
			{
				if(!way.visible())
					return;

				t_segment seg;
				//seg.vertex_ids.reserve(way.nodes().size());

				// get segment tags
				bool is_background = false;
				for(const auto& tag : way.tags())
				{
					seg.tags.emplace(std::make_pair(tag.key(), tag.value()));

					if(!is_background && std::string(tag.key()) == "landuse")
						is_background = true;
					if(super->m_skip_buildings && (std::string(tag.key()) == "building"
						|| (std::string(tag.key()) == "leisure" && 
							std::string(tag.value()) == "swimming_pool")))
						return;
				}

				// get segment vertices
				t_size invalid_refs = 0;
				for(const auto& node : way.nodes())
				{
					t_size ref = node.ref();
					seg.vertex_ids.push_back(ref);

					auto vert_iter = super->m_vertices.find(ref);
					if(vert_iter == super->m_vertices.end())
						++invalid_refs;
					else
						vert_iter->second.referenced = true;
				}

				// only referring to invalid vertices?
				if(invalid_refs == seg.vertex_ids.size())
					return;

				if(seg.vertex_ids.size() >= 2 && *seg.vertex_ids.begin() == *seg.vertex_ids.rbegin())
					seg.is_area = true;
				if(is_background)
					super->m_segments_background.emplace(std::make_pair(way.id(), std::move(seg)));
				else
					super->m_segments.emplace(std::make_pair(way.id(), std::move(seg)));

				update_progress();
			}


			void relation(const osmium::Relation& rel)
			{
				t_multisegment seg;

				// get tags
				for(const auto& tag : rel.tags())
				{
					if(super->m_skip_buildings && (std::string(tag.key()) == "building"
						|| (std::string(tag.key()) == "leisure" && 
							std::string(tag.value()) == "swimming_pool")))
						return;

					seg.tags.emplace(std::make_pair(tag.key(), tag.value()));
				}

				// get vertices
				t_size invalid_node_refs = 0;
				t_size invalid_inner_seg_refs = 0;
				t_size invalid_seg_refs = 0;

				for(const auto& member : rel.members())
				{
					t_size ref = member.ref();

					if(member.type() == osmium::item_type::node)
					{
						seg.vertex_ids.push_back(ref);

						auto vert_iter = super->m_vertices.find(ref);
						if(vert_iter == super->m_vertices.end())
							++invalid_node_refs;
						else
							vert_iter->second.referenced = true;
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

				super->m_multisegments.emplace(std::make_pair(rel.id(), std::move(seg)));

				update_progress();
			}


			void update_progress()
			{
				if(!reader || !progress)
					return;

				t_size offs = reader->offset();
				t_size size = reader->file_size();

				// offset already seen?
				if(last_offs && *last_offs == offs)
					return;
				last_offs = offs;

				if(!(*progress)(offs, size))
					throw std::runtime_error{"Stop requested."};
				//std::cout << offs << " / " << size << std::endl;
			}


			/*void area(const osmium::Area& area)
			{
			}


			void osm_object(const osmium::OSMObject&)
			{
				update_progress();
			}


			void tag_list(const osmium::TagList& tags)
			{
			}*/
		} osm_handler{this,
			&min_longitude, &max_longitude,
			&min_latitude, &max_latitude};

		try
		{
			osmium::io::Reader osm{mapname,
				osmium::osm_entity_bits::node | osmium::osm_entity_bits::way
				| osmium::osm_entity_bits::relation,
				osmium::io::read_meta::yes};
			bool contained = true;

			// if valid bounds are given, check if the map is contained within
			if(check_bounds &&
				min_longitude >= -num::pi_v<t_real> && max_longitude <= num::pi_v<t_real> &&
				min_latitude >= -num::pi_v<t_real>/t_real(2) && max_latitude <= num::pi_v<t_real>/t_real(2))
			{
				contained = false;

				osmium::io::Header header = osm.header();
				for(const osmium::Box& box : header.boxes())
				{
					if(!box.valid())
						continue;

					if(box.left() / t_real(180) * num::pi_v<t_real> > min_longitude)
						continue;
					if(box.bottom() / t_real(180) * num::pi_v<t_real> > min_latitude)
						continue;
					if(box.right() / t_real(180) * num::pi_v<t_real> < min_longitude)
						continue;
					if(box.top() / t_real(180) * num::pi_v<t_real> < min_latitude)
						continue;

					if(box.left() / t_real(180) * num::pi_v<t_real> > max_longitude)
						continue;
					if(box.bottom() / t_real(180) * num::pi_v<t_real> > max_latitude)
						continue;
					if(box.right() / t_real(180) * num::pi_v<t_real> < max_longitude)
						continue;
					if(box.top() / t_real(180) * num::pi_v<t_real> < max_latitude)
						continue;

					contained = true;
					break;
				}
			}

			if(!contained)
				return false;

			osm_handler.SetProgressFunction(progress);
			osm_handler.SetReader(&osm);
			osmium::apply(osm, osm_handler);
		}
		catch(const std::exception& ex)
		{
			std::cerr << ex.what() << std::endl;
			return false;
		}

		PruneVertices();
		return true;
	}

#else

	bool Import(const std::string& mapname,
		t_real = -10., t_real = 10.,
		t_real = -10., t_real = 10.,
		std::function<bool(t_size, t_size)>* = nullptr,
		bool check_bounds = false)
	{
		std::cerr << "Cannot import \"" << mapname
			<< "\" because osmium support is disabled."
			<< std::endl;
		return false;
	}

#endif  // _TRACKS_USE_OSMIUM_



	/**
	 * try to import from all map files in a given directory
	 * until a map with matching bounds has been found
	 */
	bool ImportDir(const std::string& dirname,
		t_real min_longitude = -10., t_real max_longitude = 10.,
		t_real min_latitude = -10., t_real max_latitude = 10.,
		std::function<bool(t_size, t_size)> *progress = nullptr)
	{
		namespace fs = __map_fs;

		// if a file is given instead of a directory, load it directly
		// without bounds checks ...
		if(fs::is_regular_file(dirname))
		{
			return Import(dirname,
				min_longitude, max_longitude,
				min_latitude, max_latitude,
				progress, false);
		}

		// ... otherwise iterate all files in the directory until
		// the one with the correct bounds has been found
		using t_iter = fs::directory_iterator;
		t_iter dir{fs::path{dirname}};

		for(t_iter iter = fs::begin(dir); iter != fs::end(dir); ++iter)
		{
			const fs::path& file = *iter;

			if(!fs::is_regular_file(file))
				continue;
			if(std::string ext = file.extension().string();
				boost::to_lower_copy(ext) != ".osm" && boost::to_lower_copy(ext) != ".pbf")
				continue;

			std::string filename = file.filename().string();
			if(Import(filename,
				min_longitude, max_longitude,
				min_latitude, max_latitude,
				progress, true))
			{
				return true;
			}
		}

		return false;
	}



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
		else if(key == "building" /*&& val == "yes"*/)
			return std::make_tuple(true, 0xdd, 0xdd, 0xdd);
		else if(key == "landuse" && val == "residential")
			return std::make_tuple(true, 0xbb, 0xbb, 0xcc);
		else if(key == "landuse" && val == "retail")
			return std::make_tuple(true, 0xff, 0x44, 0x44);
		else if(key == "landuse" && val == "industrial")
			return std::make_tuple(true, 0xaa, 0xaa, 0x44);
		else if(key == "landuse" && val == "forest")
			return std::make_tuple(true, 0x00, 0x99, 0x00);
		else if(key == "landuse" && (val == "grass" || val == "meadow"))
			return std::make_tuple(true, 0x44, 0xff, 0x44);
		else if(key == "landuse" && (val == "farmland" || val == "farmyard"))
			return std::make_tuple(true, 0x88, 0x33, 0x22);
		else if(key == "quarter" && val == "suburb")
			return std::make_tuple(true, 0x99, 0x55, 0x55);
		else if(key == "natural" && val == "shingle")
			return std::make_tuple(true, 0x55, 0x55, 0xff);
		else if((key == "natural" || key == "surface") && val == "wood")
			return std::make_tuple(true, 0x00, 0x99, 0x00);
		else if(key == "natural" && val == "water")
			return std::make_tuple(true, 0x44, 0x44, 0xff);
		else if(key == "natural" && val == "scrub")
			return std::make_tuple(true, 0x22, 0xaa, 0x22);
		else if(key == "natural" && val == "bare_rock")
			return std::make_tuple(true, 0x7d, 0x7d, 0x80);
		else if(key == "natural" && val == "grassland")
			return std::make_tuple(true, 0x44, 0xff, 0x44);
		else if(key == "waterway" && val == "river")
			return std::make_tuple(true, 0x55, 0x55, 0xff);
		else if(key == "leisure" && (val == "park" || val == "garden"))
			return std::make_tuple(true, 0x55, 0xff, 0x55);
		else if(key == "leisure" && val == "pitch")
			return std::make_tuple(true, 0x55, 0xbb, 0x55);
		else if(key == "amenity" && (val == "research_institute" || val == "university"))
			return std::make_tuple(true, 0x99, 0x99, 0x99);
		else if(key == "amenity" && (val == "school" || val == "college"))
			return std::make_tuple(true, 0x88, 0x88, 0x88);

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
			(t_size id, const t_segment *seg = nullptr,
			const std::unordered_map<std::string, std::string>* more_tags = nullptr)
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

			if(more_tags)
			{
				// search additional tag map for colour
				for(const auto& [ tag_key, tag_val ] : *more_tags)
				{
					std::tie(found, fill_col) =
						GetSurfaceColourString(tag_key, tag_val, "#ffffff");
					if(found)
						break;
				}
			}

			if(!found)
			{
				// search tag map for colour
				for(const auto& [ tag_key, tag_val ] : seg->tags)
				{
					std::tie(found, fill_col) =
						GetSurfaceColourString(tag_key, tag_val, "#ffffff");
					if(found)
						break;
				}
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
				draw_seg(id, nullptr, &multiseg.tags);
			for(const t_size id : multiseg.segment_inner_ids)
				draw_seg(id, nullptr, &multiseg.tags);
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

		if(line.size())
		{
			//svg.add(line);
			svg.map(line, "stroke:#000000; "
				"stroke-width:48px; fill:none;", 1.);
			svg.map(line, "stroke:#ffff00; "
				"stroke-width:24px; fill:none;", 1.);

			// draw start and end points
			svg.map(*line.begin(), "stroke-width:16px; "
				"stroke:#000000; fill:#ff0000;", 42.);
			svg.map(*line.rbegin(), "stroke-width:16px; "
				"stroke:#000000; fill:#00ff00;", 42.);
		}

		// draw place labels
		for(const auto& [ id, vertex ] : m_label_vertices)
		{
			auto place_iter = vertex.tags.find("place");
			auto name_iter = vertex.tags.find("name");

			if(place_iter == vertex.tags.end() || name_iter == vertex.tags.end())
				continue;

			t_vert vert{
				vertex.longitude * t_real(180) / num::pi_v<t_real>,
				vertex.latitude * t_real(180) / num::pi_v<t_real>};

			svg.text(vert, name_iter->second,
				"font-family:sans-serif; font-size:180pt; "
				"font-style=normal; font-weight=bold; "
				"stroke-width:12px; stroke:#000000; fill:#cccc44;",
				0, 0, 1);
		}
		
		return true;
	}



	void SetSkipBuildings(bool b)
	{
		m_skip_buildings = b;
	}



	void SetSkipLabels(bool b)
	{
		m_skip_labels = b;
	}



	/**
	 * set a track to be displayed together with the map
	 */
	void SetTrack(std::vector<t_vertex>&& track)
	{
		m_track = std::forward<std::vector<t_vertex>&&>(track);
	}



	bool Save(std::ofstream& ofstr) const
	{
		if(!ofstr)
			return false;

		auto save_string = [&ofstr](const std::string& str)
		{
			const t_size len = str.size();
			ofstr.write(reinterpret_cast<const char*>(&len), sizeof(len));
			ofstr.write(reinterpret_cast<const char*>(str.data()), str.size() * sizeof(char));
		};

		auto save_tags = [&ofstr, &save_string](
			const std::unordered_map<std::string, std::string>& tags)
		{
			t_size num_tags = tags.size();
			ofstr.write(reinterpret_cast<const char*>(&num_tags), sizeof(num_tags));

			for(const auto& [key, val] : tags)
			{
				save_string(key);
				save_string(val);
			}
		};

		auto save_vertices = [&ofstr, &save_tags](
			const std::unordered_map<t_size, t_vertex>& vertices)
		{
			t_size num_vertices = vertices.size();
			ofstr.write(reinterpret_cast<const char*>(&num_vertices), sizeof(num_vertices));

			for(const auto& [idx, vertex] : vertices)
			{
				ofstr.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
				ofstr.write(reinterpret_cast<const char*>(&vertex.latitude), sizeof(vertex.latitude));
				ofstr.write(reinterpret_cast<const char*>(&vertex.longitude), sizeof(vertex.longitude));

				save_tags(vertex.tags);
			}
		};

		auto save_segments = [&ofstr, &save_tags](
			const std::unordered_map<t_size, t_segment>& segs)
		{
			t_size num_segs = segs.size();
			ofstr.write(reinterpret_cast<const char*>(&num_segs), sizeof(num_segs));

			for(const auto& [idx, seg] : segs)
			{
				ofstr.write(reinterpret_cast<const char*>(&idx), sizeof(idx));

				std::uint8_t flags = seg.is_area ? 1 : 0;
				ofstr.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

				t_size num_verts = seg.vertex_ids.size();
				ofstr.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
				for(t_size vert_id : seg.vertex_ids)
					ofstr.write(reinterpret_cast<const char*>(&vert_id), sizeof(vert_id));				

				save_tags(seg.tags);
			}
		};

		auto save_multisegments = [&ofstr, &save_tags](
			const std::unordered_map<t_size, t_multisegment>& segs)
		{
			t_size num_segs = segs.size();
			ofstr.write(reinterpret_cast<const char*>(&num_segs), sizeof(num_segs));

			for(const auto& [idx, seg] : segs)
			{
				ofstr.write(reinterpret_cast<const char*>(&idx), sizeof(idx));

				t_size num_verts = seg.vertex_ids.size();
				ofstr.write(reinterpret_cast<const char*>(&num_verts), sizeof(num_verts));
				for(t_size vert_id : seg.vertex_ids)
					ofstr.write(reinterpret_cast<const char*>(&vert_id), sizeof(vert_id));				

				t_size num_isegs = seg.segment_inner_ids.size();
				ofstr.write(reinterpret_cast<const char*>(&num_isegs), sizeof(num_isegs));
				for(t_size seg_id : seg.segment_inner_ids)
					ofstr.write(reinterpret_cast<const char*>(&seg_id), sizeof(seg_id));				

				t_size num_segs = seg.segment_ids.size();
				ofstr.write(reinterpret_cast<const char*>(&num_segs), sizeof(num_segs));
				for(t_size seg_id : seg.segment_ids)
					ofstr.write(reinterpret_cast<const char*>(&seg_id), sizeof(seg_id));				

				save_tags(seg.tags);
			}
		};

		ofstr.write(MAP_MAGIC, sizeof(MAP_MAGIC));

		ofstr.write(reinterpret_cast<const char*>(&m_min_latitude), sizeof(m_min_latitude));
		ofstr.write(reinterpret_cast<const char*>(&m_max_latitude), sizeof(m_max_latitude));
		ofstr.write(reinterpret_cast<const char*>(&m_min_longitude), sizeof(m_min_longitude));
		ofstr.write(reinterpret_cast<const char*>(&m_max_longitude), sizeof(m_max_longitude));

		std::uint8_t flags = 0;
		if(m_skip_buildings)
			flags |= (1 << 0);
		if(m_skip_labels)
			flags |= (1 << 1);
		ofstr.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

		save_vertices(m_vertices);
		save_vertices(m_label_vertices);
		save_segments(m_segments);
		save_segments(m_segments_background);
		save_multisegments(m_multisegments);

		return true;
	}



	bool Load(std::ifstream& ifstr)
	{
		if(!ifstr)
			return false;

		auto load_string = [&ifstr]() -> std::string
		{
			t_size len{};
			ifstr.read(reinterpret_cast<char*>(&len), sizeof(len));

			std::string str;
			str.resize(len);
			ifstr.read(reinterpret_cast<char*>(str.data()), str.size() * sizeof(char));

			return str;
		};

		auto load_tags = [&ifstr, &load_string]()
			-> std::unordered_map<std::string, std::string>
		{
			t_size len{};
			ifstr.read(reinterpret_cast<char*>(&len), sizeof(len));

			std::unordered_map<std::string, std::string> tags;

			for(t_size i = 0; i < len; ++i)
			{
				std::string key = load_string();
				std::string val = load_string();

				tags.emplace(std::make_pair(std::move(key), std::move(val)));
			}

			return tags;
		};

		auto load_vertices = [&ifstr, &load_tags]()
			-> std::unordered_map<t_size, t_vertex>
		{
			t_size len{};
			ifstr.read(reinterpret_cast<char*>(&len), sizeof(len));

			std::unordered_map<t_size, t_vertex> vertices;

			for(t_size i = 0; i < len; ++i)
			{
				t_vertex vertex;

				t_size idx{};
				ifstr.read(reinterpret_cast<char*>(&idx), sizeof(idx));
				ifstr.read(reinterpret_cast<char*>(&vertex.latitude), sizeof(vertex.latitude));
				ifstr.read(reinterpret_cast<char*>(&vertex.longitude), sizeof(vertex.longitude));

				vertex.tags = load_tags();
				vertex.referenced = true;

				vertices.emplace(std::make_pair(idx, std::move(vertex)));
			}

			return vertices;
		};

		auto load_segments = [&ifstr, &load_tags]()
			-> std::unordered_map<t_size, t_segment>
		{
			t_size len{};
			ifstr.read(reinterpret_cast<char*>(&len), sizeof(len));

			std::unordered_map<t_size, t_segment> segs;

			for(t_size i = 0; i < len; ++i)
			{
				t_segment seg;

				t_size idx{};
				ifstr.read(reinterpret_cast<char*>(&idx), sizeof(idx));

				std::uint8_t flags;
				ifstr.read(reinterpret_cast<char*>(&flags), sizeof(flags));
				seg.is_area = (flags != 0);

				t_size num_verts{};
				ifstr.read(reinterpret_cast<char*>(&num_verts), sizeof(num_verts));

				for(t_size j = 0; j < num_verts; ++j)
				{
					t_size vert_id{};
					ifstr.read(reinterpret_cast<char*>(&vert_id), sizeof(vert_id));

					seg.vertex_ids.push_back(vert_id);
				}

				seg.tags = load_tags();

				segs.emplace(std::make_pair(idx, std::move(seg)));
			}

			return segs;
		};

		auto load_multisegments = [&ifstr, &load_tags]()
			-> std::unordered_map<t_size, t_multisegment>
		{
			t_size len{};
			ifstr.read(reinterpret_cast<char*>(&len), sizeof(len));

			std::unordered_map<t_size, t_multisegment> segs;

			for(t_size i = 0; i < len; ++i)
			{
				t_multisegment seg;

				t_size idx{};
				ifstr.read(reinterpret_cast<char*>(&idx), sizeof(idx));

				t_size num_verts{};
				ifstr.read(reinterpret_cast<char*>(&num_verts), sizeof(num_verts));

				for(t_size j = 0; j < num_verts; ++j)
				{
					t_size vert_id{};
					ifstr.read(reinterpret_cast<char*>(&vert_id), sizeof(vert_id));

					seg.vertex_ids.push_back(vert_id);
				}

				t_size num_isegs{};
				ifstr.read(reinterpret_cast<char*>(&num_isegs), sizeof(num_isegs));

				for(t_size j = 0; j < num_isegs; ++j)
				{
					t_size seg_id{};
					ifstr.read(reinterpret_cast<char*>(&seg_id), sizeof(seg_id));

					seg.segment_inner_ids.push_back(seg_id);
				}

				t_size num_segs{};
				ifstr.read(reinterpret_cast<char*>(&num_segs), sizeof(num_segs));

				for(t_size j = 0; j < num_segs; ++j)
				{
					t_size seg_id{};
					ifstr.read(reinterpret_cast<char*>(&seg_id), sizeof(seg_id));

					seg.segment_ids.push_back(seg_id);
				}

				seg.tags = load_tags();

				segs.emplace(std::make_pair(idx, std::move(seg)));
			}

			return segs;
		};

		char magic[sizeof(MAP_MAGIC)];
		ifstr.read(magic, sizeof(magic));
		if(std::string(magic) != MAP_MAGIC)
			return false;

		ifstr.read(reinterpret_cast<char*>(&m_min_latitude), sizeof(m_min_latitude));
		ifstr.read(reinterpret_cast<char*>(&m_max_latitude), sizeof(m_max_latitude));
		ifstr.read(reinterpret_cast<char*>(&m_min_longitude), sizeof(m_min_longitude));
		ifstr.read(reinterpret_cast<char*>(&m_max_longitude), sizeof(m_max_longitude));

		std::uint8_t flags = 0;
		ifstr.read(reinterpret_cast<char*>(&flags), sizeof(flags));
		m_skip_buildings = (flags & (1 << 0)) != 0;
		m_skip_labels = (flags & (1 << 1)) != 0;

		m_vertices = load_vertices();
		m_label_vertices = load_vertices();
		m_segments = load_segments();
		m_segments_background = load_segments();
		m_multisegments = load_multisegments();

		return true;
	}



	bool Save(const std::string& filename) const
	{
		std::ofstream ofstr{filename, std::ios::binary};
		if(!ofstr)
			return false;

		return Save(ofstr);
	}



	bool Load(const std::string& filename)
	{
		std::ifstream ifstr{filename, std::ios::binary};
		if(!ifstr)
			return false;

		return Load(ifstr);
	}



protected:
	std::string m_filename{};
	std::string m_version{};
	std::string m_creator{};

	t_real m_min_longitude{}, m_max_longitude{};
	t_real m_min_latitude{}, m_max_latitude{};

	bool m_skip_buildings{false};
	bool m_skip_labels{true};

	std::unordered_map<t_size, t_vertex> m_vertices{};
	std::unordered_map<t_size, t_vertex> m_label_vertices{};
	std::unordered_map<t_size, t_segment> m_segments{};
	std::unordered_map<t_size, t_segment> m_segments_background{};
	std::unordered_map<t_size, t_multisegment> m_multisegments{};

	std::vector<t_vertex> m_track{};
};


#endif
