# piserver
Raspberry Pi Server wizard to serve Raspbian to network booting Pis

## How to rebuild piserver

### Get dependencies

On Raspbian install the build dependencies:

```
sudo apt-get install build-essential devscripts debhelper cmake libldap2-dev libgtkmm-3.0-dev libarchive-dev libcurl4-openssl-dev libcap-dev intltool git
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
Can install it with dpkg:

```
cd ..
sudo dpkg -i piserver*.deb
```

### Extra dependencies if NOT running Raspbian or "Debian Stretch with Raspberry Pi desktop"

#### dnsmasq

Piserver requires dnsmasq version 2.77 or later.
If your Linux distribution comes with an older version, compile the latest version from source:

```
sudo apt-get install libnetfilter-conntrack-dev libidn11-dev libgmp-dev nettle-dev liblua5.2-dev
git clone --depth 1 http://thekelleys.org.uk/git/dnsmasq.git
cd dnsmasq
debuild -uc -us
cd ..
sudo dpkg -i dnsmasq-base_*.deb dnsmasq_*.deb
```

If you are running Raspbian this is not necessary. Although it is using an older dnsmasq version as well, it has been patched to include the newer features used (--dhcp-reply-delay and tftp-unique-root=mac options).

#### IP update hook

It is expected that the computer that is running piserver:

* is configured to use a static IP-address that never changes.
* -OR- uses dhcpcd as network manager. In which case it will notify piserver automatically if your server's IP ever changes.
* -OR- that you configure your network manager program (e.g. using a ifup/networkd-dispatcher hook script) to execute "piserver --update-ip" every time the system  obtains a new IP lease.
