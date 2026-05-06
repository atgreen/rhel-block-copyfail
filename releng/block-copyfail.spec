Name:           block-copyfail
Version:        0.1.0
Release:        1%{?dist}
Summary:        BPF LSM blocker for CVE-2026-31431 (Copy Fail)

License:        Apache-2.0 AND GPL-2.0-only
URL:            https://github.com/atgreen/rhel-block-copyfail

%global debug_package %{nil}
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
Zero-reboot mitigation for CVE-2026-31431 ("Copy Fail"), a Linux kernel
privilege escalation vulnerability in the algif_aead cryptographic interface.

Installs a BPF LSM program that blocks all AF_ALG AEAD socket binds,
preventing exploitation via crypto template nesting. Other AF_ALG usage
(hash, skcipher, rng) is unaffected.

%prep
%autosetup

%build
# BPF_CFLAGS are hardcoded (clang -target bpf); RPM flags apply to userspace only
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{build_ldflags}"

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
# No automatic restart — avoid an unprotected window during upgrades
%systemd_postun block-copyfail.service

%files
%license LICENSE
%doc README.md
%{_sbindir}/block-copyfail
%{_unitdir}/block-copyfail.service

%changelog
* Tue May 06 2026 Anthony Green <green@moxielogic.com> - 0.1.0-1
- Initial RPM package
