#include "abstractadddistro.h"
#include "downloadthread.h"
#include "downloadextractthread.h"
#include "importosthread.h"
#include "piserver.h"
#include "distribution.h"
#include <regex>
#include <filesystem>

using namespace std;
using nlohmann::json;

AbstractAddDistro::AbstractAddDistro(PiServer *ps)
    : _ps(ps), _dt(NULL), _det(NULL)
{
}

AbstractAddDistro::~AbstractAddDistro()
{
    if (_dt)
    {
        _dt->cancelDownload();
        delete _dt;
    }
    if (_det)
    {
        _det->cancelDownload();
        delete _det;
    }
    if (_timer.connected())
    {
        _timer.disconnect();
    }
    if (_sdPollTimer.connected())
    {
        _sdPollTimer.disconnect();
    }
}

void AbstractAddDistro::setupAddDistroFields(Glib::RefPtr<Gtk::Builder> builder, const std::string &cachedDistroInfo)
{
    builder->get_widget("distrotree", _distroTree);
    builder->get_widget("reporadio", _repoRadio);
    builder->get_widget("localfileradio", _localfileRadio);
    builder->get_widget("urlradio", _urlRadio);
    builder->get_widget("filechooser", _fileChooser);
    builder->get_widget("urlentry", _urlEntry);
    builder->get_widget("progressbar", _progressBar);
    builder->get_widget("progresslabel1", _progressLabel1);
    builder->get_widget("progresslabel2", _progressLabel2);
    builder->get_widget("progresslabel3", _progressLabel3);
    if (builder->get_object("sdstore"))
    {
        builder->get_widget("sdradio", _sdRadio);
        builder->get_widget("osnameentry", _osnameEntry);
        builder->get_widget("sdcombo", _sdCombo);
        builder->get_widget("sdgrid", _sdGrid);
        _sdRadio->signal_clicked().connect( sigc::mem_fun(this, &AbstractAddDistro::_setSensitive) );
        _sdStore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("sdstore") );
        _sdStore->clear();
        _pollSDdrives();
        _sdPollTimer = Glib::signal_timeout().connect_seconds( sigc::mem_fun(this, &AbstractAddDistro::_pollSDdrives), 1 );
        _osnameEntry->signal_changed().connect( sigc::mem_fun(this, &AbstractAddDistro::setAddDistroOkButton) );
        _sdCombo->signal_changed().connect( sigc::mem_fun(this, &AbstractAddDistro::setAddDistroOkButton) );
    }
    else
    {
        _sdRadio = nullptr; _osnameEntry = nullptr; _sdCombo = nullptr; _sdGrid = nullptr;
    }

    _repoStore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( builder->get_object("repostore") );
    _repoStore->clear();
    _repoRadio->signal_clicked().connect( sigc::mem_fun(this, &AbstractAddDistro::_setSensitive) );
    _localfileRadio->signal_clicked().connect( sigc::mem_fun(this, &AbstractAddDistro::_setSensitive) );
    _urlRadio->signal_clicked().connect( sigc::mem_fun(this, &AbstractAddDistro::_setSensitive) );
    _urlEntry->signal_changed().connect( sigc::mem_fun(this, &AbstractAddDistro::setAddDistroOkButton) );
    _distroTree->signal_cursor_changed().connect( sigc::mem_fun(this, &AbstractAddDistro::setAddDistroOkButton) );
    _fileChooser->signal_file_set().connect( sigc::mem_fun(this, &AbstractAddDistro::setAddDistroOkButton) );
    _setSensitive();

    if (cachedDistroInfo.empty())
    {
        _dt = new DownloadThread(PISERVER_REPO_URL);
        //_dt->setCacheFile(PISERVER_REPO_CACHEFILE);
        _dt->signalSuccess().connect( sigc::mem_fun(this, &AbstractAddDistro::_onRepolistDownloadComplete) );
        _dt->signalError().connect( sigc::mem_fun(this, &AbstractAddDistro::_onRepolistDownloadFailed) );
        _dt->start();
    }
    else
    {
        _parseDistroInfo(cachedDistroInfo);
    }
}

void AbstractAddDistro::_setSensitive()
{
    _distroTree->set_sensitive( _repoRadio->get_active() );
    _fileChooser->set_sensitive( _localfileRadio->get_active() );
    _urlEntry->set_sensitive( _urlRadio->get_active() );
    if ( _repoRadio->get_active()
         && !_distroTree->get_selection()->count_selected_rows() )
    {
        _distroTree->set_cursor(Gtk::TreePath("0"));
    }
    if (_sdGrid)
    {
        _sdGrid->set_sensitive( _sdRadio->get_active() );
    }
    setAddDistroOkButton();
}

bool AbstractAddDistro::addDistroFieldsOk()
{
    return (_repoRadio->get_active() && _distroTree->get_selection()->count_selected_rows())
            || (_localfileRadio->get_active() && !_fileChooser->get_filename().empty() )
            || (_urlRadio->get_active() && !_getDistroname(_urlEntry->get_text()).empty() )
            || (_sdRadio && _sdRadio->get_active() && !_osnameEntry->get_text().empty()
                && _osnameEntry->get_text().find('/') == Glib::ustring::npos && _osnameEntry->get_text()[0] != '.'
                && _sdCombo->get_active_row_number() != -1) ;
}

void AbstractAddDistro::_onRepolistDownloadComplete()
{
    _parseDistroInfo( _dt->data() );
    delete _dt;
    _dt = NULL;
}

void AbstractAddDistro::_parseDistroInfo(const std::string &data)
{
    try
    {
        auto j = json::parse(data);
        auto oslist = j.at("os_list");

        for (auto &entry : oslist)
        {
            string name = entry.at("os_name").get<string>();
            string version = entry.at("release_date").get<string>();
            string tarball = entry.at("tarball").get<string>();
            int nominalSize = entry.at("nominal_size").get<int>();
            string sizeStr = to_string(nominalSize)+" MB";

            auto row = _repoStore->append();
            row->set_value(0, tarball);
            row->set_value(1, name);
            row->set_value(2, version);
            row->set_value(3, sizeStr);
            row->set_value(4, nominalSize);
        }
    }
    catch (exception &e)
    {
        Gtk::MessageDialog d( string(_("Error parsing downloaded repository information: "))+e.what() );
        d.run();
    }

    _setSensitive();
}

void AbstractAddDistro::_onRepolistDownloadFailed()
{
    Gtk::MessageDialog d( _("Error downloading repository information: ")+_dt->lastError() );
    d.run();
    delete _dt;
    _dt = NULL;
}

std::string AbstractAddDistro::_getDistroname(const std::string &url)
{
    static regex r("([^/]+)\\.tar\\.xz$");
    smatch s;
    string result;

    if (regex_search(url, s, r))
        result = s[1];

    return result;
}

void AbstractAddDistro::startInstallation()
{
    string distroName, url, version, sizestr;

    if (_sdPollTimer.connected())
    {
        _sdPollTimer.disconnect();
    }

    if ( _repoRadio->get_active() )
    {
        auto iter = _distroTree->get_selection()->get_selected();
        iter->get_value(0, url);
        iter->get_value(1, distroName);
        iter->get_value(2, version);
        iter->get_value(3, sizestr);

        int availMB = _ps->availableDiskSpace() / 1048576;
        if ( stoi(sizestr) >  availMB)
        {
            onInstallationFailed(_("Insufficient disk space. ")+to_string(availMB)+_(" MB available."));
            return;
        }

    }
    else if ( _urlRadio->get_active() )
    {
        url = _urlEntry->get_text();
        distroName = _getDistroname(url);
    }
    else if ( _localfileRadio->get_active() )
    {
        url = "file://"+_fileChooser->get_filename();
        distroName = _getDistroname(url);
    }
    else if ( _sdRadio && _sdRadio->get_active() )
    {
        distroName = _osnameEntry->get_text();
    }

    if (distroName.empty())
        distroName = "unnamed";

    if (_ps->getDistributions()->count(distroName)
            && _ps->getDistributions()->at(distroName)->version() == version)
    {
        onInstallationFailed(_("There is already an OS called '")+distroName+_("' installed."));
        return;
    }

    _progressLabel1->set_text(_("Starting download"));

    _dist = new Distribution(distroName, version);
    _dist->setLatestVersion(version);
    if ( _sdRadio && _sdRadio->get_active() )
    {
        _det = new ImportOSThread(_sdCombo->get_active_id(), _dist->distroPath());
    }
    else
    {
        _det = new DownloadExtractThread(url, _dist->distroPath());
    }
    _det->setPostInstallScript(PISERVER_POSTINSTALLSCRIPT);

    if (!_ps->getSetting("ldapServerType").empty())
    {
        string ldapConfig = "\n# BEGIN PISERVER EXTERNAL LDAP SERVER CONFIG\n"
                "uri "+_ps->getSetting("ldapUri")+"\n"
                "base "+_ps->getSetting("ldapDN")+"\n";
        if (!_ps->getSetting("ldapUser").empty())
        {
            ldapConfig +=
                "binddn "+_ps->getSetting("ldapUser")+"\n"
                "bindpw "+_ps->getSetting("ldapPassword")+"\n";
        }
        ldapConfig += _ps->getSetting("ldapExtraConfig");
        ldapConfig += "# END PISERVER EXTERNAL LDAP SERVER CONFIG\n";
        _det->setLdapConfig(ldapConfig);
    }

    _det->signalSuccess().connect( sigc::mem_fun(this, &AbstractAddDistro::_onInstallationSuccess) );
    _det->signalError().connect( sigc::mem_fun(this, &AbstractAddDistro::_onInstallationFailed) );
    _det->start();
    _t1 = chrono::steady_clock::now();
    _timer = Glib::signal_timeout().connect_seconds( sigc::mem_fun(this, &AbstractAddDistro::_updateProgress), 1 );
}

void AbstractAddDistro::_onInstallationSuccess()
{
    _ps->addDistribution(_dist);
    delete _det;
    _det = NULL;
    onInstallationSuccess();
}

void AbstractAddDistro::_onInstallationFailed()
{
    string error = _det->lastError();
    delete _det;
    _det = NULL;
    delete _dist;
    onInstallationFailed(error);
}

bool AbstractAddDistro::_updateProgress()
{
    if (!_det)
        return false;

    chrono::duration<double> elapsed = chrono::steady_clock::now() - _t1;
    float mb = _det->dlNow() / 1048576.0;
    float totalMb = _det->dltotal() / 1048576.0;
    float downloadRate = mb / max(elapsed.count(), 1.0);
    int etahr = 0, etamin = 0, etasec = (totalMb-mb)/downloadRate;
    char buf[255];
    string left;

    if (etasec > 60)
        etamin = etasec/60;
    if (etamin > 60)
    {
        etahr = etamin/60;
        etamin = etamin % 60;
    }

    snprintf(buf, sizeof(buf), _("%.1f MB of %.1f MB"), mb, totalMb);
    _progressLabel1->set_text(buf);
    snprintf(buf, sizeof(buf), _("%.1f Mbit/sec"), downloadRate * 8.0);
    _progressLabel2->set_text(buf);

    if (etahr > 1)
    {
        left = to_string(etahr)+_(" hours ");
    }
    if (etamin > 1)
    {
        left += to_string(etamin)+_(" minutes ");
    }
    else if (etamin == 1)
    {
        left += _("1 minute ");
    }
    else if (etahr == 0 && etamin == 0)
    {
        left = _("Less than a minute ");
    }
    left += _("left");
    _progressLabel3->set_text(left);

    if (totalMb)
    {
        _progressBar->set_fraction(mb / totalMb);
    }

    return true;
}

bool AbstractAddDistro::_pollSDdrives()
{
    std::string sysclassblock = "/sys/class/block";

    for (const auto &entry : std::filesystem::directory_iterator(sysclassblock))
    {
        if (!std::filesystem::exists(entry.path() / "removable")
                || std::filesystem::exists(entry.path() / "partition")
                || !std::filesystem::exists(entry.path() / "device/vendor")
                || !std::filesystem::exists(entry.path() / "device/model")
                || Glib::file_get_contents(entry.path() / "removable") == "0\n")
        {
            continue;
        }

        std::string dev = entry.path().filename();
        if (_sdDrives.find(dev) != _sdDrives.end())
        {
            // Already in list
            continue;
        }
        std::string vendor = Glib::file_get_contents(entry.path() / "device/vendor");
        std::string model = Glib::file_get_contents(entry.path() / "device/model");
        // Remove trailing \n
        if (!vendor.empty())
            vendor.pop_back();
        if (!model.empty())
            model.pop_back();

        auto row = _sdStore->append();
        row->set_value(0, "/dev/"+dev);
        row->set_value(1, dev+": "+vendor+" "+model);
        if (_sdDrives.empty())
        {
            // First drive detected, set combobox to drive.
            _sdCombo->set_active(0);
        }
        _sdDrives.insert(dev);
    }

    return true;
}
