# Slackware Repository

Repository for custom SlackBuild scripts, system utilities, and configurations for Slackware Linux.

## Active SlackBuilds Catalog

<!-- SLACKBUILDS_TABLE_START -->
| Package | Version | Description | Build Type | Homepage |
|---|---|---|---|---|
| **[apache-cassandra](slackbuilds/apache-cassandra/)** | `4.1.12` | distributed database | Binary Repackage | [Website](https://cassandra.apache.org/) |
| **[blis](slackbuilds/blis/)** | `2.1` | BLAS-like Library Instantiation Software | Autotools | [Website](https://github.com/flame/blis) |
| **[cassandra](slackbuilds/cassandra/)** | `2.1.22` | apache-cassandra (distributed database) | Binary Repackage | [Website](http://cassandra.apache.org/) |
| **[gst1-vaapi](slackbuilds/gst1-vaapi/)** | `1.26.11` | Hardware-accelerated video on Intel via VA-API | Meson / Ninja | [Website](https://github.com/GStreamer/gstreamer-vaapi/) |
| **[idea](slackbuilds/idea/)** | `2026.2.1` | Development IDE | Binary Repackage | [Website](https://www.jetbrains.com/idea/) |
| **[jzmq](slackbuilds/jzmq/)** | `3.1.0` | zeromq-jzmq (java bindings to 0MQ) | Autotools | [Website](https://github.com/zeromq/jzmq/) |
| **[lan-mouse](slackbuilds/lan-mouse/)** | `0.11.0` | Software KVM switch for LAN | Rust / Cargo | [Website](https://github.com/feschber/lan-mouse) |
| **[LanguageTool](slackbuilds/LanguageTool/)** | `6.6` | style and grammar checker | Java / Binary | [Website](https://www.languagetool.org/) |
| **[lapce](slackbuilds/lapce/)** | `0.4.6` | Lightning-fast and Powerful Code Editor | Binary Repackage | [Website](https://lap.dev/) |
| **[leveldb](slackbuilds/leveldb/)** | `1.23` | Google key-value database | CMake | [Website](https://github.com/google/leveldb) |
| **[libjcat](slackbuilds/libjcat/)** | `0.2.6` | Library for reading and writing Jcat files | Meson / Ninja | [Website](https://github.com/hughsie/libjcat) |
| **[mscgen](slackbuilds/mscgen/)** | `0.20` | Message Sequence Chart Generator | Autotools | [Website](http://www.mcternan.me.uk/mscgen/) |
| **[obsidian](slackbuilds/obsidian/)** | `1.0` | obsidian cursors theme | Source / Script | [Website](http://gnome-look.org/content/show.php/Obsidian+Cursors?content=73135) |
| **[openfire](slackbuilds/openfire/)** | `5.1.2` | XMPP server | Binary Repackage | [Website](https://www.igniterealtime.org/projects/openfire/) |
| **[openvpn3-linux](slackbuilds/openvpn3-linux/)** | `v27.1` | OpenVPN 3 Linux client | Autotools | [Website](https://github.com/OpenVPN/openvpn3-linux) |
| **[pdf4qt](slackbuilds/pdf4qt/)** | `1.6.0.0` | Open source PDF editor | CMake | [Website](https://github.com/JakubMelka/PDF4QT) |
| **[pycharm](slackbuilds/pycharm/)** | `2026.2.1` | Python and Django IDE | Binary Repackage | [Website](https://www.jetbrains.com/pycharm/) |
| **[rider](slackbuilds/rider/)** | `2026.2.1` | C# and .NET IDE | Binary Repackage | [Website](https://www.jetbrains.com/rider/) |
| **[snappy](slackbuilds/snappy/)** | `1.2.2` | A fast compressor/decompressor | CMake | [Website](https://github.com/google/snappy) |
| **[vscodium](slackbuilds/vscodium/)** | `1.126.04524` | Visual Studio Code FOSS Binary Release | Binary Repackage | [Website](https://vscodium.com/) |
| **[zed](slackbuilds/zed/)** | `1.17.2` | The fast, collaborative code editor | Binary Repackage | [Website](https://zed.dev) |
| **[zeromq](slackbuilds/zeromq/)** | `4.3.5` | lightweight messaging kernel library | Autotools | [Website](http://www.zeromq.org/) |
<!-- SLACKBUILDS_TABLE_END -->

## System Scripts

The [`scripts/`](scripts/) directory contains administration and maintenance utilities:
* `rc.cloud`: Cloud-init and cloud instance boot orchestration
* `rc.firewall`: Stateful iptables firewall rules
* `rc.zram`: ZRAM swap allocation service
* `shrinkpdf.sh`: Ghostscript PDF compression utility
* `slackbuild_tool.py`: SlackBuild repository maintenance, linting, and synchronization tool
* `update_readme.sh`: Automation helper to synchronize package lists across documentation

## Maintainer

* **Name**: Mário Antunes
* **Email**: mario.antunes@ua.pt
