# piserver
Raspberry Pi Server wizard to serve Raspbian to network booting Pis

## How to rebuild piserver

### Get build dependencies

On Raspbian:

`sudo apt-get install devscripts debhelper cmake libldap2-dev libgtkmm-3.0-dev libarchive-dev libcurl4-openssl-dev intltool`

### Build the Debian package

`debuild`

debuild will compile everything, create a .deb package and put it in the parent directory.
