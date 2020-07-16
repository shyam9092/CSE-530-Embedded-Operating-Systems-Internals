#ifndef __KERNEL__
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#endif

#include <linux/netlink.h>

#define genl_attr_msg_max 256
#define genl_family_name "usr_krnl_nl"
#define genl_mcgp_name "genl_mcgp0"

enum {
	GENL_TEST_C_UNSPEC,		
	GENL_TEST_C_MSG,
};

enum genl_multicast_groups{
	genl_mcgrp0,
};

#define GENL_MCGRP_MAX	1

static char *genl_mcgrp_name[GENL_MCGRP_MAX] = {
    genl_mcgp_name,
};

enum genl_test_attrs {
	GENL_TEST_ATTR_UNSPEC,		/* Must NOT use element 0 */

	GENL_STRING_ATTR,
    GENL_STRUCT_ATTR,
    GENL_INT_ATTR,

	__GENL_TEST_ATTR__MAX,
};

#define genl_attr_max (__GENL_TEST_ATTR__MAX + 1)

static struct nla_policy genl_policy[genl_attr_max - 1] = {
	[GENL_STRING_ATTR] = {
		.type = NLA_STRING,
    },

    [GENL_STRUCT_ATTR] = {
        .type = NLA_NESTED,
    },

    [GENL_INT_ATTR] = {
        .type = NLA_U32,
    },
};