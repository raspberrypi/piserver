#ifndef ABSTRACTADDDISTRO_H
#define ABSTRACTADDDISTRO_H

#include <gtkmm.h>
#include <chrono>

class DownloadThread;
class DownloadExtractThread;
class PiServer;
class Distribution;

class AbstractAddDistro
{
protected:
    AbstractAddDistro(PiServer *ps);
    virtual ~AbstractAddDistro();
    void setupAddDistroFields(Glib::RefPtr<Gtk::Builder> builder, const std::string &cachedDistroInfo = "");

    Gtk::RadioButton *_repoRadio, *_localfileRadio, *_urlRadio;
    Gtk::TreeView *_distroTree;
    Gtk::FileChooserButton *_fileChooser;
    Glib::RefPtr<Gtk::ListStore> _repoStore;
    Gtk::Entry *_urlEntry;
    Gtk::ProgressBar *_progressBar;
    Gtk::Label *_progressLabel1, *_progressLabel2, *_progressLabel3;
    Distribution *_dist;

    void startInstallation();
    bool addDistroFieldsOk();
    virtual void setAddDistroOkButton() = 0;
    virtual void onInstallationSuccess() = 0;
    virtual void onInstallationFailed(const std::string &error) = 0;

private:
    PiServer *_ps;
    DownloadThread *_dt;
    DownloadExtractThread *_det;
    std::chrono::steady_clock::time_point _t1;
    sigc::connection _timer;

    void _setSensitive();
    void _parseDistroInfo(const std::string &data);
    std::string _getDistroname(const std::string &url);
    void _onRepolistDownloadComplete();
    void _onRepolistDownloadFailed();
    void _onInstallationSuccess();
    void _onInstallationFailed();
    bool _updateProgress();
};

#endif // ABSTRACTADDDISTRO_H
