#include <iostream>
#include <gtkmm.h>
#include "mainwindow.h"
#include "installwizard.h"
#include "piserver.h"

using namespace std;

int main(int argc, char *argv[])
{
    PiServer ps;

	setlocale (LC_ALL, "");
	bindtextdomain ("piserver", "/usr/share/locale");
	bind_textdomain_codeset ("piserver", "UTF-8");
	textdomain ("piserver");

    if (argc == 2 && strcmp(argv[1], "--update-ip") == 0 )
    {
        ps.updateIP();
        return 0;
    }

    auto app = Gtk::Application::create(argc, argv, "org.raspberrypi.piserver");

    if (::getuid() != 0)
    {
        Gtk::MessageDialog d(_("This program must be run as root. Try starting it with sudo."));
        d.run();
        return 1;
    }

    if (!ps.getSetting("installed", false))
    {
        InstallWizard w(app, &ps);
        w.exec();
        /* For some reason gtk doesn't allow the main loop to be run a second time.
           create fresh Application object for use by MainWindow */
        app = Gtk::Application::create(argc, argv, "org.raspberrypi.piserver");
    }

    if (ps.getSetting("installed", false))
    {
        MainWindow win(app, &ps);
        win.exec();
    }

    return 0;
}

