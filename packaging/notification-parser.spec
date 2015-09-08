Name:		notification-parser
Summary:	notification parser library
Version:	1.2.2.1
Release:	1
VCS:     framework/appfw/notification-parser#38642594039ce772cb046363f5ebb4d331308c2e
Group:		System/Libraries
License:	Apache
Source0:	%{name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(pkgmgr-info)

%description
notification parser library

%package devel
Summary:    notification parser library (Development)
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
notification parser library (DEV)

%prep
%setup -q

%build 
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%ifarch %{ix86}
CXXFLAGS="$CXXFLAGS -D_OSP_DEBUG_ -D_SECURE_LOG -D_OSP_X86_ -D_OSP_EMUL_" cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DFULLVER=%{version} -DMAJORVER=${MAJORVER}
%else
CXXFLAGS="-O2 -g -pipe -Wall -fno-exceptions -Wformat -Wformat-security -Wl,--as-needed -fmessage-length=0 -march=armv7-a -mtune=cortex-a8 -mlittle-endian -mfpu=neon -mfloat-abi=softfp -D__SOFTFP__ -mthumb -Wa,-mimplicit-it=thumb -funwind-tables -D_OSP_DEBUG_ -D_SECURE_LOG -D_OSP_ARMEL_" cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DFULLVER=%{version} -DMAJORVER=${MAJORVER}
%endif

# Call make instruction with smp support
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}

%make_install
mkdir -p %{buildroot}/opt/usr/dbspace
if [ ! -s %{buildroot}/opt/usr/dbspace/.notification_parser.db ]; then
echo "The database file for notification will be created."
sqlite3 %{buildroot}/opt/usr/dbspace/.notification_parser.db <<EOF
CREATE TABLE notification_setting (appid TEXT PRIMARY KEY NOT NULL, notification TEXT, sounds TEXT, contents TEXT, badge TEXT, pkgid TEXT, reserved1 TEXT, reserved2 TEXT);
EOF
fi

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest notification-parser.manifest
/usr/share/license/%{name}
/usr/etc/package-manager/parserlib/libnotifications.so*
/opt/usr/dbspace/.notification_parser.db
/opt/usr/dbspace/.notification_parser.db-journal

%files devel
%{_libdir}/pkgconfig/notification-parser.pc
