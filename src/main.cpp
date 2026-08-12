// SPDX-License-Identifier: GPL-2.0
// AI-generated (Claude)
#include <QApplication>
#include <git2.h>

#include "commands/command.h"
#include "mainwindow.h"

int main(int argc, char **argv)
{
	// core/save-git.cpp and core/git-access.cpp need libgit2 initialized
	// before any repository operation, same as every other Subsurface
	// entry point (subsurface-desktop-main.cpp, cli/main.cpp, etc.).
	git_libgit2_init();

	QApplication app(argc, argv);
	app.setApplicationName("suunto2subsurface");
	app.setOrganizationName("suunto2subsurface");

	// Command::init() creates the global QUndoStack that
	// core/save-git.cpp's commit-message generation
	// (Command::changesMade()) reads from -- without it, saving to a
	// git-backed dive log segfaults on a null undo stack. We never
	// actually push undo commands (this tool has no undo/redo UI), so
	// this stack just stays empty.
	Command::init();

	MainWindow window;
	window.show();

	return app.exec();
}
