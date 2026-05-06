## block-copyfail

> **Unofficial and only lightly tested.** Use at your own risk.

**Zero-reboot** mitigation for **CVE-2026-31431** ("Copy Fail"), a Linux kernel
privilege escalation vulnerability in the `algif_aead` cryptographic interface.

Based on [openshift/block-copyfail](https://github.com/openshift/block-copyfail),
which in turn was based on [atgreen/block-copyfail](https://github.com/atgreen/block-copyfail).
This repo repackages it as RPMs with a systemd service for standalone RHEL systems.

If you can reboot, the simplest fix is to blacklist the module:

```bash
echo "blacklist algif_aead" | sudo tee /etc/modprobe.d/block-copyfail.conf
sudo reboot
```

This package is for systems that **cannot be rebooted** — production servers,
long-running workloads, etc. It installs a BPF LSM program as a systemd service
that blocks all AF_ALG AEAD socket binds at runtime, without a reboot. Other
AF_ALG usage (hash, skcipher, rng) is unaffected.

## Install

RPM packages are available for RHEL 9 and RHEL 10 (and compatible distros like
CentOS Stream, AlmaLinux, Rocky Linux). BPF LSM must be enabled in the kernel
(RHEL 9.8+ and RHEL 10 have it by default).

```bash
sudo dnf config-manager --add-repo \
  https://atgreen.github.io/rhel-block-copyfail/rpm-repo/block-copyfail.repo
sudo dnf install block-copyfail
sudo systemctl enable --now block-copyfail
```

## Verify

```bash
systemctl status block-copyfail
journalctl -u block-copyfail -f
```

## How it works

CVE-2026-31431 chains AF_ALG sockets with `authencesn` AEAD and `splice()` to
corrupt arbitrary files in the kernel page cache. The BPF LSM program hooks
`socket_bind` and returns `-EPERM` for any AF_ALG AEAD bind, preventing
exploitation regardless of crypto template nesting (e.g. `pcrypt(authencesn(...))`).

## Removal

```bash
sudo systemctl disable --now block-copyfail
sudo dnf remove block-copyfail
```

The BPF program detaches automatically when the service stops. No reboot needed.

## Building from source

```bash
sudo dnf install clang llvm bpftool libbpf-devel elfutils-libelf-devel \
  zlib-devel gcc make pkgconfig

make
sudo install -m 0755 block-copyfail /usr/sbin/
sudo install -m 0644 block-copyfail.service /usr/lib/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now block-copyfail
```

## License

Apache-2.0
