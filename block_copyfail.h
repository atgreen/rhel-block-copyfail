#ifndef BLOCK_COPYFAIL_H
#define BLOCK_COPYFAIL_H

#ifndef __bpf__
#include <linux/types.h>
#endif

#define AF_ALG   38
#define AF_RXRPC 33

#define BLOCK_HOOK_CF1   1   /* AF_ALG AEAD bind (CVE-2026-31431) */
#define BLOCK_HOOK_CF2   2   /* UDP splice (Copy Fail 2 / Dirty Frag ESP) */
#define BLOCK_HOOK_DF    3   /* AF_RXRPC create (Dirty Frag rxkad path) */
#define BLOCK_HOOK_ENCAP 4   /* UDP_ENCAP setsockopt (pre-6.5 fallback) */

struct block_event {
	__u32 pid;
	char  comm[16];
	__u32 hook;
	__u64 ts;
};

#endif
