Name: sp-stress
Version: 0.2.0
Release: 1%{?dist}
Summary: Tools for creating stress test loads
Group: Development/Tools
License: GPLv2+
URL: http://www.gitorious.org/+maemo-tools-developers/maemo-tools/sp-stress
Source: %{name}_%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build

%description
   Contains the following tools:
   - `swpload' exercises the kernel virtual memory subsystem by creating
     clients that access memory pages with configurable patterns.
   - `cpuload' generates CPU load according to specified value.
   - `memload' allocates configurable amount of memory.
   - `flash_eater' allocates disk space on the filesystem so that only a
     configurable amount of free space will be left.
   - `run_secs' allows execution of given command for a configurable time
     period, after which it will be terminated.

%prep
%setup -q -n %{name}

%build
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/ioload
%{_bindir}/swpload
%{_bindir}/memload
%{_bindir}/cpuload
%{_bindir}/run_secs
%{_bindir}/flash_eater
%{_mandir}/man1/*.1.gz
%doc doc/README COPYING 

%changelog
* Tue Aug 02 2011 Eero Tamminen <eero.tamminen@nokia.com> 0.2.0
  * Added support of uncompressable pages by default (and option -f
    rand|fast).
  * Added update of oom_adj to 0 by default (and option -j XXX).

* Fri Sep 04 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.1.9-1
  * memload to handle oom_adj write errors correctly

* Wed Jun 10 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.1.8-1
  * Fix possible overflow

* Wed Mar 25 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.1.7-1
  * Add help message for option '-l' of memload

* Tue Mar 17 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.1.6-1
  * Support for filling RAM and disk until only specified 
    amount is free

* Tue Jan 27 2009 Eero Tamminen <eero.tamminen@nokia.com> 0.1.5-1
  * Ioload was deprecated and replaced with a minimal shell script that
    recommends using spew instead.
  * Added swpload tool for generating paging load. 

* Tue Nov 18 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.4-1
  * Memload: Fixed a segmentation fault occurring in argument handling.

* Fri Oct 31 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.3-1
  * Cpuload now supports selection of scheduler
  * Cpuload supports now setting up highest or lowest priority available
    without resorting to additional tools.
  * Fixed the manual page for flash_eater missing from previous release.
  * Cpuload now validates load values more strictly.

* Thu Oct 09 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.2-1
  * Added new script (flash_eater) to use up flash storage space until
    only a given amount remains.
  * Added a new option (-l) to memload; it will allocate memory so that
    only a given amount is left.
  * Added documentation for the new script and memload option.

* Fri Apr 04 2008 Eero Tamminen <eero.tamminen@nokia.com> 0.1.1-1
  * Cpuload no longer attempts to automatically raise its own priority.
    Instead, this needs to be explicitly requested with the new -p
    option. 
  * Fixed cpuload reporting priority raising failures even when it
    actually had succeeded

* Wed Nov 28 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.1.0-1
  * Added run_secs convenience script for running stress tools for a
    limited amount of time. 
  * Added missing manual pages
  * Checked and fixed copyright information

* Thu Apr 19 2007 Eero Tamminen <eero.tamminen@nokia.com> 0.0.1-1
  * Initial Release.

