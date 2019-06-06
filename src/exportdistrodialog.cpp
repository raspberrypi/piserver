#include "exportdistrodialog.h"
#include "distribution.h"
#include "piserver.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <pwd.h>

using namespace std;

ExportDistroDialog::ExportDistroDialog(PiServer *ps, Distribution *distro, Gtk::Window *parent)
    : _ps(ps), _distro(distro), _parent(parent)
{
}

ExportDistroDialog::~ExportDistroDialog()
{
}

bool ExportDistroDialog::exec()
{
    string filename, dir;
    Glib::Date date;

    Gtk::FileChooserDialog fd(*_parent, _("Please choose a file name"), Gtk::FILE_CHOOSER_ACTION_SAVE);
    fd.set_transient_for(*_parent);
    auto filter = Gtk::FileFilter::create();
    filter->set_name(".tar.xz files");
    filter->add_pattern("*.tar.xz");
    fd.add_filter(filter);
    date.set_time_current();
    fd.set_current_name(_distro->name()+" "+date.format_string("%Y%m%d")+".tar.xz");
    fd.set_do_overwrite_confirmation(true);
    fd.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
    fd.add_button(Gtk::Stock::SAVE, Gtk::RESPONSE_OK);

    if (fd.run() != Gtk::RESPONSE_OK)
        return false;

    fd.hide();
    filename = fd.get_filename();
    dir = _distro->distroPath();
    if (!Glib::str_has_suffix(filename, ".tar.xz"))
        filename += ".tar.xz";

    try
    {
        const gchar *cmd[] = {"bsdtar", "-C", dir.c_str(), "--numeric-owner", "--one-file-system", "-cJf", filename.c_str(), ".", NULL};
        GError *error = NULL;

        if (!g_spawn_async(NULL, (gchar **) cmd, NULL,
                           (GSpawnFlags) (G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH), NULL, NULL, &_pid, &error))
            throw runtime_error(error->message);

        _progressDialog = new Gtk::MessageDialog(*_parent, _("Creating archive (can take 10+ minutes)"),
                                                 false, Gtk::MESSAGE_OTHER, Gtk::BUTTONS_NONE, false);
        sigc::connection cwatch = Glib::signal_child_watch().connect(sigc::mem_fun(this, &ExportDistroDialog::onTarFinished), _pid);
        _parent->get_window()->set_cursor(Gdk::Cursor::create(Gdk::WATCH));
        int result = _progressDialog->run();
        _parent->get_window()->set_cursor();
        cwatch.disconnect();
        g_spawn_close_pid(_pid);
        delete _progressDialog;
        _progressDialog = NULL;

        if (result != 0)
        {
            string msg = Glib::ustring::compose(_("Error occured while creating archive (statuscode=%1)"), result);

            if (result == Gtk::RESPONSE_DELETE_EVENT)
            {
                ::kill(_pid, SIGTERM);
                msg = _("Creating archive cancelled");
            }

            Gtk::MessageDialog d(msg);
            d.set_transient_for(*_parent);
            d.run();

            ::unlink(filename.c_str() );
        }
        else
        {
            struct passwd *userinfo;
            const char *user = getenv("SUDO_USER");
            if (!user)
                user = getenv("USER");

            if (user)
            {
                userinfo = ::getpwnam(user);
                if (userinfo)
                {
                    if (::chown(filename.c_str(), userinfo->pw_uid, userinfo->pw_gid) == -1) { }
                }
            }

            return true;
        }
    }
    catch (const runtime_error &r)
    {
        Gtk::MessageDialog d(r.what());
        d.set_transient_for(*_parent);
        d.run();
    }

    return false;
}

void ExportDistroDialog::onTarFinished(GPid pid, int status)
{
    if (pid == _pid && _progressDialog)
        _progressDialog->response(status);
}
