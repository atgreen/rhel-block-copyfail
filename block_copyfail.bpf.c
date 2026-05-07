/* BPF LSM program to block Copy Fail vulnerabilities.
 *
 * Copy Fail 1 (CVE-2026-31431): hooks socket_bind and blocks all AF_ALG
 * AEAD binds.  The vulnerability is in algif_aead, and authencesn can be
 * nested inside wrapper templates (e.g. pcrypt), so blocking the entire
 * AEAD type is the only bypass-proof approach.  Other AF_ALG usage
 * (hash, skcipher, rng) is unaffected.
 *
 * Copy Fail 2 (BLOCK_CF2, RHEL 10 only): hooks socket_sendmsg and blocks
 * MSG_SPLICE_PAGES sends on ESP-in-UDP sockets.  The vulnerability uses
 * xfrm ESP-in-UDP with MSG_SPLICE_PAGES to get a no-COW page-cache write.
 * Normal IPsec traffic (non-splice sends) is unaffected.
 */

#include <linux/types.h>
#include <linux/bpf.h>
#include <linux/errno.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "block_copyfail.h"

#ifdef BLOCK_CF2
#include <bpf/bpf_core_read.h>

/* CO-RE struct stubs — field names must match kernel BTF.
 * Actual offsets are relocated at load time by libbpf. */
struct sock_common {
	__u16 skc_family;
} __attribute__((preserve_access_index));

struct sock {
	struct sock_common __sk_common;
	__u8 sk_protocol;
} __attribute__((preserve_access_index));

struct udp_sock {
	__u8 encap_type;
} __attribute__((preserve_access_index));

struct socket {
	short type;
	struct sock *sk;
} __attribute__((preserve_access_index));

struct msghdr {
	unsigned int msg_flags;
} __attribute__((preserve_access_index));

struct sockaddr;

#define AF_INET   2
#define AF_INET6 10
#define SOCK_DGRAM 2
#define IPPROTO_UDP 17
#define MSG_SPLICE_PAGES       0x08000000
#define UDP_ENCAP_ESPINUDP_NON_IKE 1
#define UDP_ENCAP_ESPINUDP         2

#else /* !BLOCK_CF2 */

struct socket;
struct sockaddr;

#endif /* BLOCK_CF2 */

/* struct sockaddr_alg layout (from linux/if_alg.h):
 *   offset 0:  __u16  salg_family
 *   offset 2:  __u8   salg_type[14]
 *   offset 16: __u32  salg_feat
 *   offset 20: __u32  salg_mask
 *   offset 24: __u8   salg_name[64]
 * We only need 7 bytes to check salg_type == "aead\0".
 */
#define SOCKADDR_ALG_TYPE_OFFSET 2
#define SOCKADDR_ALG_CHECK_LEN 7

static const char aead_type[5] = "aead";

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 4096);
} events SEC(".maps");

static __always_inline void emit_block_event(__u32 hook)
{
	struct block_event *evt;

	evt = bpf_ringbuf_reserve(&events, sizeof(*evt), 0);
	if (!evt)
		return;

	evt->pid = bpf_get_current_pid_tgid() >> 32;
	bpf_get_current_comm(evt->comm, sizeof(evt->comm));
	evt->hook = hook;
	evt->ts = bpf_ktime_get_ns();
	bpf_ringbuf_submit(evt, 0);
}

/* Copy Fail 1: block AF_ALG AEAD binds */
SEC("lsm/socket_bind")
int BPF_PROG(block_copyfail, struct socket *sock,
	     struct sockaddr *address, int addrlen, int ret)
{
	if (ret)
		return ret;

	if (addrlen < SOCKADDR_ALG_CHECK_LEN)
		return 0;

	__u8 buf[SOCKADDR_ALG_CHECK_LEN];

	if (bpf_probe_read(buf, sizeof(buf), address) < 0)
		return 0;

	__u16 family;
	__builtin_memcpy(&family, &buf[0], sizeof(family));
	if (family != AF_ALG)
		return 0;

	if (__builtin_memcmp(&buf[SOCKADDR_ALG_TYPE_OFFSET], aead_type, 5) != 0)
		return 0;

	emit_block_event(BLOCK_HOOK_CF1);
	return -EPERM;
}

#ifdef BLOCK_CF2
/* Copy Fail 2: block MSG_SPLICE_PAGES sends on ESP-in-UDP sockets.
 *
 * The exploit splices a target file's page into a pipe, then splices
 * that pipe into an ESP-in-UDP socket.  The kernel sets MSG_SPLICE_PAGES
 * on this path, and ESP decrypt-in-place corrupts the shared page cache.
 * Blocking this specific combination is the narrowest possible mitigation.
 */
SEC("lsm/socket_sendmsg")
int BPF_PROG(block_copyfail2, struct socket *sock,
	     struct msghdr *msg, int size, int ret)
{
	struct sock *sk;
	struct udp_sock *usk;

	if (ret)
		return ret;

	if (!(BPF_CORE_READ(msg, msg_flags) & MSG_SPLICE_PAGES))
		return 0;

	if (BPF_CORE_READ(sock, type) != SOCK_DGRAM)
		return 0;

	sk = BPF_CORE_READ(sock, sk);
	if (!sk)
		return 0;

	if (BPF_CORE_READ(sk, __sk_common.skc_family) != AF_INET &&
	    BPF_CORE_READ(sk, __sk_common.skc_family) != AF_INET6)
		return 0;

	if (BPF_CORE_READ(sk, sk_protocol) != IPPROTO_UDP)
		return 0;

	usk = (struct udp_sock *)sk;
	__u8 encap = BPF_CORE_READ(usk, encap_type);
	if (encap != UDP_ENCAP_ESPINUDP &&
	    encap != UDP_ENCAP_ESPINUDP_NON_IKE)
		return 0;

	emit_block_event(BLOCK_HOOK_CF2);
	return -EPERM;
}
#endif /* BLOCK_CF2 */

char LICENSE[] SEC("license") = "GPL";
