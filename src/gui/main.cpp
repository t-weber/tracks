/**
 * program entry point
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date Nov-2024
 * @license see 'LICENSE' file
 */

#include "gui.h"
#include "helpers.h"

#include <QtWidgets/QApplication>

#include <string>
#include <iostream>


int main(int argc, char** argv)
{
	try
	{
		// application
		auto app = std::make_unique<QApplication>(argc, argv);
		app->setOrganizationName("tw");
		app->setApplicationName("tracks");
		//app->setApplicationDisplayName("Tracks");
		app->setApplicationVersion("0.1");
		set_c_locale();

		// main window
		auto tracks = std::make_unique<TracksWnd>();

		// ressources
		Resources& res = tracks->GetResources();
		fs::path appdir = app->applicationDirPath().toStdString();
		res.SetBinPath(appdir);
		res.AddPath(appdir);
		res.AddPath(appdir / "res");
		res.AddPath(appdir / ".." / "res");

		// setup main window gui
		tracks->SetupGUI();
		tracks->FileNew();

		// show the main window
		show_dialog(tracks.get());
		return app->exec();
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
	}

	return -1;
}
