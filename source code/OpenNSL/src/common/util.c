#include <stdlib.h>
#include <stdint.h>

#define	BWL_PRE_PACKED_STRUCT
#define	BWL_POST_PACKED_STRUCT	__attribute__((packed))
#define	ETHER_ADDR_LEN		6

typedef uint8_t u8;
typedef uint8_t uint8;
typedef unsigned long ulong;

BWL_PRE_PACKED_STRUCT struct ether_addr {
	u8 octet[ETHER_ADDR_LEN];
} BWL_POST_PACKED_STRUCT;

static inline unsigned long
simple_strtoul(const char *nptr, char **endptr, int base)
{
	return strtoul(nptr, endptr, base);
}

// from bcmutils.c (https://elixir.bootlin.com/linux/v2.6.37/source/drivers/staging/brcm80211/util/bcmutils.c#L345)
int bcm_ether_atoe(char *p, struct ether_addr *ea)
{
	int i = 0;

	for (;;) {
		ea->octet[i++] = (char)simple_strtoul(p, &p, 16);
		if (!*p++ || i == 6)
			break;
	}

	return i == 6;
}

// from bcmutils.c
char* bcm_ether_ntoa(const struct ether_addr *ea, char *buf)
{
	static const char hex[] =
	  {
		  '0', '1', '2', '3', '4', '5', '6', '7',
		  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	  };
	const uint8 *octet = ea->octet;
	char *p = buf;
	int i;

	for (i = 0; i < 6; i++, octet++) {
		*p++ = hex[(*octet >> 4) & 0xf];
		*p++ = hex[*octet & 0xf];
		*p++ = ':';
	}

	*(p-1) = '\0';

	return (buf);
}
