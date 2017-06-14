#ifndef DHCPANALYZER_H
#define DHCPANALYZER_H

#include <glibmm.h>
#include <set>
#include <string>

class DhcpAnalyzer
{
public:
    DhcpAnalyzer();
    virtual ~DhcpAnalyzer();
    void startListening();

    /* signals */
    inline sigc::signal<void,std::string> signal_macDetected()
    {
        return _signal_macDetected;
    }

protected:
    int _s;
    sigc::connection _conn;
    std::set<std::string> _seen;
    sigc::signal<void,std::string> _signal_macDetected;

    /* Slots */
    bool on_packet(Glib::IOCondition cond);
};

#endif // DHCPANALYZER_H
