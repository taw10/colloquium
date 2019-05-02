Colloquium: Narrative-based presentation system
===============================================

Copyright © 2017-2019 Thomas White

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Author: Thomas White <taw@bitwiz.me.uk>

See the COPYING file for full licence conditions.

Summary
-------

Colloquium is a presentation program that is more than just "slideware".
Instead of concentrating on slides, Colloquium makes the "narrative" of your
presentation the most important thing.  Slides are embedded in your narrative,
forming part of the flow of your talk.


Installation instructions
-------------------------

Colloquium uses the Meson build system (http://mesonbuild.com), which works
with Ninja (https://ninja-build.org/).  Start by installing these if you don't
already have them.  For example, in Fedora:

    $ sudo dnf install meson ninja-build

or for Debian/Ubuntu:

    $ sudo apt install meson ninja-build

or for Mac OS users, using Homebrew (https://brew.sh):

    $ brew install meson ninja

You will also need the gettext and GTK 3 development files:

    $ sudo dnf install gettext-devel gtk3-devel

or:

    $ sudo apt install gettext-devel libgtk-3-dev

or:

    $ brew install gettext gtk+3

This should pull in the other dependencies, which are GDK, GLib, GIO, Cairo,
Pango and gdk-pixbuf.  You may need to additionally install Flex and Bison.

Set up the build directory using Meson:

    $ meson build

Compile Colloquium using Ninja:

    $ ninja -C build

To install:

    $ sudo ninja -C build install


Running the program
-------------------

Colloquium should appear in your desktop environment's menus.  Alternatively,
it can be started from the command line:

    $ colloquium

The first time Colloquium runs, it will show an introduction document to help
you get started.


Contributing
------------

Clone from either GitHub or my private repository:

    $ git clone git://git.bitwiz.me.uk/colloquium.git
    $ git clone https://github.com/taw10/colloquium.git

Browse the repository:  https://git.bitwiz.me.uk/?p=colloquium.git or https://github.com/taw10/colloquium

Issue tracker:  https://github.com/taw10/colloquium/issues
