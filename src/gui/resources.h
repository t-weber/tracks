/**
 * resource files
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2021
 * @license see 'LICENSE' file
 */

#ifndef __TRACKS_RESOURCE_H__
#define __TRACKS_RESOURCE_H__

#include <vector>
#include <optional>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace filesystem = std::filesystem;
#elif __has_include(<experimental/filesystem>)
	#include <experimental/filesystem>
	namespace filesystem = std::experimental::filesystem;
#else
	#include <boost/filesystem.hpp>
	namespace filesystem = boost::filesystem;
#endif


class Resources
{
public:
	Resources() = default;
	~Resources() = default;

	void AddPath(const filesystem::path& path);
	std::optional<filesystem::path> FindFile(const filesystem::path& file) const;

	void SetBinPath(const filesystem::path& path);
	const filesystem::path& GetBinPath() const;


private:
	// resource paths
	std::vector<filesystem::path> m_paths{};

	// program binary path
	filesystem::path m_bin_path{};
};

#endif
