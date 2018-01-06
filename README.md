# piserver
Raspberry Pi Server wizard to serve Raspbian to network booting Pis

## How to rebuild piserver

### Get dependencies

On Raspbian install the build dependencies:

```
sudo apt-get install devscripts debhelper cmake libldap2-dev libgtkmm-3.0-dev libarchive-dev libcurl4-openssl-dev intltool gksu git
```

If not using a Pi (or other armhf device), you also need the following runtime dependencies:

```
sudo apt-get install binfmt-support qemu-user-static
```


### Get the source

```
git clone --depth 1 https://github.com/raspberrypi/piserver.git
```

### Build the Debian package

```
cd piserver
debuild -uc -us
```

debuild will compile everything, create a .deb package and put it in the parent directory.
