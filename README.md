# The Tilde Text Editor

Tilde is a text editor for the console/terminal, which provides an intuitive
interface for people accustomed to GUI environments such as Gnome, KDE and
Windows. For example, the short-cut to copy the current selection is Control-C,
and to paste the previously copied text the short-cut Control-V can be used.
As another example, the File menu can be accessed by pressing Meta-F.

For more information, see [the homepage](https://os.ghalkes.nl/tilde)

## Installing Tilde

The easiest way to install Tilde is by using the repositories from the Tilde
homepage [download section](https://os.ghalkes.nl/tilde/download.html). If there
are no binary packages provided for your distribution or hardware, you can still
build Tilde from the official releases provided there. Be aware that Tilde
depends on several support libraries, which are also provided through the
Tilde homepage.

Building from the official release is recommended over attempting to build from
the git repositories for installing Tilde. Only for development of Tilde should
the git repositories be used.

## Developing Tilde

To help developing Tilde, you will need to build Tilde from the git
repositories. The repositories assume that all parts of Tilde, i.e. Tilde
itself and its support libraries, are built from the git repositories. Please
follow the steps below to build Tilde from the git repositories:

1. Install the dependencies of Tilde from the system libraries.
2. Clone the repositories:
```bash
for i in makesys transcript t3shared t3window t3widget t3key t3config t3highlight tilde ; do
	git clone https://github.com/gphalkes/$i.git
done
```
3. Build all packages: `./t3shared/doall make -C src`

Once the build is complete, tilde/src/.objects/edit is the newly compiled
tilde. If the [termdebug](https://os.ghalkes.nl/termdebug.html) suite of tools
is installed, then tilde/src/tedit can be used to run the editor while
recording the input and output for debugging purposes.
