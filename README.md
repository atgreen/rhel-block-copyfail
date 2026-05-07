## block-copyfail

> **Unofficial and only lightly tested.** Use at your own risk.

**Zero-reboot** mitigation for **Copy Fail** kernel privilege escalation
vulnerabilities (CVE-2026-31431 and Copy Fail 2).

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

RPM packages are available for RHEL 8, 9, and 10 (and compatible distros like
CentOS Stream, AlmaLinux, Rocky Linux). BPF LSM must be enabled in the kernel
(RHEL 8.5+, 9, and 10 have it by default). Note that BPF LSM on RHEL 8 is
classified as Technology Preview by Red Hat.

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

**Copy Fail 1** (CVE-2026-31431): chains AF_ALG sockets with `authencesn`
AEAD and `splice()` to corrupt arbitrary files in the kernel page cache.
The BPF LSM program hooks `socket_bind` and returns `-EPERM` for any AF_ALG
AEAD bind, preventing exploitation regardless of crypto template nesting
(e.g. `pcrypt(authencesn(...))`).

**Copy Fail 2** (RHEL 10 / Fedora only): uses xfrm ESP-in-UDP with
`MSG_SPLICE_PAGES` to achieve the same page-cache write via a different
subsystem. The exploit splices shared page-cache pages into a plain UDP
socket; an ESP-in-UDP receiver then decrypts in-place, corrupting the
shared pages. The BPF LSM program hooks `socket_sendmsg` and blocks all
splice-based zero-copy sends on UDP sockets (the sending socket has no
ESP encap, so only the broader check works). Normal `sendmsg`/`write`
to UDP is unaffected.

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

make                                          # Copy Fail 1 only
make EXTRA_BPF_CFLAGS=-DBLOCK_CF2 EXTRA_CFLAGS=-DBLOCK_CF2  # + Copy Fail 2
sudo install -m 0755 block-copyfail /usr/sbin/
sudo install -m 0644 block-copyfail.service /usr/lib/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now block-copyfail
```

## License

Apache-2.0
