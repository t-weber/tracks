/**
 * resource files
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2021
 * @license see 'LICENSE' file
 */

#include "resources.h"

namespace fs = filesystem;


/**
 * add a resource search path entry
 */
void Resources::AddPath(const fs::path& path)
{
	m_paths.push_back(path);
}


/**
 * find a resource file
 */
std::optional<fs::path> Resources::FindFile(const fs::path& file) const
{
	for(const fs::path& path : m_paths)
	{
		fs::path respath = path / file;

		if(fs::exists(respath))
			return respath;
	}

	return std::nullopt;
}


void Resources::SetBinPath(const filesystem::path& path)
{
	m_bin_path = path;
}


const filesystem::path& Resources::GetBinPath() const
{
	return m_bin_path;
}
