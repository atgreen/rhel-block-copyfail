Name:           block-copyfail
Version:        0.1.0
Release:        1%{?dist}
Summary:        BPF LSM blocker for CVE-2026-31431 (Copy Fail)

License:        Apache-2.0 AND GPL-2.0-only
URL:            https://github.com/atgreen/rhel-block-copyfail

%global debug_package %{nil}

%if 0%{?rhel} >= 10 || 0%{?fedora}
%global with_cf2 1
%global cf2_flags -DBLOCK_CF2
%endif

Source0:        block-copyfail-%{version}.tar.gz

BuildRequires:  clang
BuildRequires:  llvm
BuildRequires:  bpftool
BuildRequires:  libbpf-devel
BuildRequires:  elfutils-libelf-devel
BuildRequires:  zlib-devel
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  systemd-rpm-macros

Requires:       libbpf
Requires:       elfutils-libelf
Requires:       zlib

%description
Zero-reboot mitigation for Copy Fail kernel privilege escalation
vulnerabilities.

Installs BPF LSM programs that block exploitation at runtime without a
reboot. Copy Fail 1 (CVE-2026-31431) blocks AF_ALG AEAD socket binds.
%if 0%{?with_cf2}
Copy Fail 2 blocks splice-based zero-copy sends on UDP sockets.
%endif
Other AF_ALG usage (hash, skcipher, rng) and normal IPsec traffic are
unaffected.

%prep
%autosetup

%build
echo ">>> rhel=%{?rhel} fedora=%{?fedora} with_cf2=%{?with_cf2} cf2_flags=%{?cf2_flags}"
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{build_ldflags}" \
  EXTRA_BPF_CFLAGS="%{?cf2_flags}" EXTRA_CFLAGS="%{?cf2_flags}"

%install
install -D -m 0755 block-copyfail %{buildroot}%{_sbindir}/block-copyfail
install -D -m 0644 block-copyfail.service %{buildroot}%{_unitdir}/block-copyfail.service

%post
%systemd_post block-copyfail.service
if [ $1 -eq 1 ]; then
    systemctl enable --now block-copyfail.service >/dev/null 2>&1 || :
fi

%preun
%systemd_preun block-copyfail.service

%postun
%systemd_postun_with_restart block-copyfail.service

%files
%license LICENSE
%doc README.md
%{_sbindir}/block-copyfail
%{_unitdir}/block-copyfail.service

%changelog
* Tue May 06 2026 Anthony Green <green@moxielogic.com> - 0.1.0-1
- Initial RPM package
