#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtypes.h"
#include "strutils.h"
#include "memutil.h"
#include "util.h"

#define IPV4_STR_SIZE 16

static rr_parser_container default_rr_parser =
{
    0, 0, escape, NULL, NULL, 0
};

bool
is_default_rr_parser(rr_parser_container *parser)
{
    return parser == &default_rr_parser;
}


char *
mk_error(const char * msg, const uint8_t * packet, uint32_t pos,
         uint16_t rdlength)
{
    char * tmp = escape_data(packet, pos, pos+rdlength);
    char *buffer = strfmt("%s%s", msg, tmp);
    FREE(tmp);

    return buffer;
}


#define A_DOC "A (IPv4 address) format\n"\
"A records are simply an IPv4 address, and are formatted as such."

char *
A(const uint8_t * packet, uint32_t pos, uint32_t i,
  uint16_t rdlength, uint32_t plen)
{
    if (rdlength != 4)
    {
        return mk_error("Bad A record: ", packet, pos, rdlength);
    }

    return strfmt("%d.%d.%d.%d", packet[pos], packet[pos+1],
                                 packet[pos+2], packet[pos+3]);
}


#define D_DOC "domain name like format\n"\
"A DNS like name. This format is used for many record types."

char *
domain_name(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
            uint16_t rdlength, uint32_t plen)
{
    char * name = read_rr_name(packet, &pos, id_pos, plen);
    if (name == NULL)
    {
        name = mk_error("Bad DNS name: ", packet, pos, rdlength);
    }

    return name;
}


#define SOA_DOC "Start of Authority format\n"\
"Presented as a series of labeled SOA fields."

char * soa(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
           uint16_t rdlength, uint32_t plen)
{
    char * mname;
    char * rname;
    char * buffer;
    uint32_t serial, refresh, retry, expire, minimum;
    const char * format = "mname: %s, rname: %s, serial: %d, "
                          "refresh: %d, retry: %d, expire: %d, min: %d";

    mname = read_rr_name(packet, &pos, id_pos, plen);
    if (mname == NULL) return mk_error("Bad SOA: ", packet, pos, rdlength);
    rname = read_rr_name(packet, &pos, id_pos, plen);
    if (rname == NULL) return mk_error("Bad SOA: ", packet, pos, rdlength);

    int i;
    serial = refresh = retry = expire = minimum = 0;
    for (i = 0; i < 4; i++)
    {
        serial  <<= 8; serial  |= packet[pos+(i+(4*0))];
        refresh <<= 8; refresh |= packet[pos+(i+(4*1))];
        retry   <<= 8; retry   |= packet[pos+(i+(4*2))];
        expire  <<= 8; expire  |= packet[pos+(i+(4*3))];
        minimum <<= 8; minimum |= packet[pos+(i+(4*4))];
    }

    buffer = strfmt(format, mname, rname, serial, refresh, retry, expire, minimum);
    FREE(mname);
    FREE(rname);
    return buffer;
}


#define MX_DOC "Mail Exchange record format\n"\
"A standard dns name preceded by a preference number."

char *
mx(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
   uint16_t rdlength, uint32_t plen)
{

    uint16_t pref = (packet[pos] << 8) + packet[pos+1];
    char * name;
    char * buffer;
    uint32_t spos = pos;

    pos = pos + 2;
    name = read_rr_name(packet, &pos, id_pos, plen);
    if (name == NULL)
    {
        return mk_error("Bad MX: ", packet, spos, rdlength);
    }

    buffer = strfmt("%d,%s", pref, name);
    FREE(name);
    return buffer;
}


#define OPTS_DOC "EDNS option record format\n"\
"These records contain a size field for warning about extra large DNS \n"\
"packets, an extended rcode, and an optional set of dynamic fields.\n"\
"The size and extended rcode are printed, but the dynamic fields are \n"\
"simply escaped. Note that the associated format function is non-standard,\n"\
"as EDNS records modify the basic resourse record protocol (there is no \n"\
"class field, for instance. RFC 2671"

char *
opts(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
     uint16_t rdlength, uint32_t plen)
{
    uint16_t payload_size = (packet[pos] << 8) + packet[pos+1];
    char *buffer;
    const char * base_format = "size:%d,rcode:0x%02x%02x%02x%02x,%s";
    char *rdata = escape_data(packet, pos+6, pos + 6 + rdlength);

    buffer = strfmt(base_format, payload_size, packet[pos+2], packet[pos+3],
                                 packet[pos+4], packet[pos+5], rdata);
    FREE(rdata);

    return buffer;
}


#define SRV_DOC "Service record format. RFC 2782\n"\
"Service records are used to identify various network services and ports.\n"\
"The format is: 'priority,weight,port target'\n"\
"The target is a somewhat standard DNS name."

char *
srv(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
    uint16_t rdlength, uint32_t plen)
{
    uint16_t priority = (packet[pos] << 8) + packet[pos+1];
    uint16_t weight = (packet[pos+2] << 8) + packet[pos+3];
    uint16_t port = (packet[pos+4] << 8) + packet[pos+5];
    char *target, *buffer;
    pos = pos + 6;
    /* Don't read beyond the end of the rr. */
    target = read_rr_name(packet, &pos, id_pos, pos+rdlength-6);
    if (target == NULL)
        return mk_error("Bad SRV", packet, pos, rdlength);

    buffer = strfmt("%d,%d,%d %s", priority, weight, port, target);
    FREE(target);

    return buffer;
}

#define AAAA_DOC "IPv6 record format.  RFC 3596\n"\
"A standard IPv6 address. No attempt is made to abbreviate the address."

char *
AAAA(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
                  uint16_t rdlength, uint32_t plen) {
    char *buffer;
    uint16_t ipv6[8];
    int i;

    if (rdlength != 16)
    {
        return mk_error("Bad AAAA record", packet, pos, rdlength);
    }

    for (i=0; i < 8; i++)
    {
        ipv6[i] = (packet[pos+i*2] << 8) + packet[pos+i*2+1];
    }

    buffer = strfmt("%x:%x:%x:%x:%x:%x:%x:%x", ipv6[0], ipv6[1], ipv6[2],
                                               ipv6[3], ipv6[4], ipv6[5],
                                               ipv6[6], ipv6[7]);

    return buffer;
}


#define KEY_DOC "dnssec Key format. RFC 4034\n"\
"format: flags, proto, algorithm, key\n"\
"All fields except the key are printed as decimal numbers.\n"\
"The key is given in base64. "

char *
dnskey(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
       uint16_t rdlength, uint32_t plen)
{
    uint16_t flags = (packet[pos] << 8) + packet[pos+1];
    uint8_t proto = packet[pos+2];
    uint8_t algorithm = packet[pos+3];
    char *buffer, *key;

    key = b64encode(packet, pos+4, rdlength-4);
    buffer = strfmt("%d,%d,%d,%s", flags, proto, algorithm, key);
    FREE(key);

    return buffer;
}


#define RRSIG_DOC "DNS SEC Signature. RFC 4304\n"\
"format: tc,alg,labels,ottl,expiration,inception,tag signer signature\n"\
"All fields except the signer and signature are given as decimal numbers.\n"\
"The signer is a standard DNS name.\n"\
"The signature is base64 encoded."

char *
rrsig(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
      uint16_t rdlength, uint32_t plen)
{
    uint32_t o_pos = pos;
    uint16_t tc = (packet[pos] << 8) + packet[pos+1];
    uint8_t alg = packet[pos+2];
    uint8_t labels = packet[pos+3];
    u_int ottl, sig_exp, sig_inc;
    uint16_t key_tag = (packet[pos+16] << 8) + packet[pos+17];
    char *signer, *signature, *buffer;

    pos = pos + 4;
    ottl = (packet[pos] << 24) + (packet[pos+1] << 16) +
           (packet[pos+2] << 8) + packet[pos+3];
    pos = pos + 4;
    sig_exp = (packet[pos] << 24) + (packet[pos+1] << 16) +
              (packet[pos+2] << 8) + packet[pos+3];
    pos = pos + 4;
    sig_inc = (packet[pos] << 24) + (packet[pos+1] << 16) +
              (packet[pos+2] << 8) + packet[pos+3];
    pos = pos + 6;
    signer = read_rr_name(packet, &pos, id_pos, o_pos+rdlength);
    if (signer == NULL)
    {
        return mk_error("Bad Signer name", packet, pos, rdlength);
    }

    signature = b64encode(packet, pos, o_pos+rdlength-pos);
    buffer = strfmt("%d,%d,%d,%d,%d,%d,%d,%s,%s", tc, alg, labels, ottl,
                    sig_exp, sig_inc, key_tag, signer, signature);
    FREE(signer);
    FREE(signature);

    return buffer;
}


#define NSEC_DOC "NSEC format.  RFC 4034\n"\
"Format: domain bitmap\n"\
"domain is a DNS name, bitmap is hex escaped."

char *
nsec(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
     uint16_t rdlength, uint32_t plen)
{

    char *buffer, *domain, *bitmap;

    domain = read_rr_name(packet, &pos, id_pos, pos+rdlength);
    if (domain == NULL)
    {
        return mk_error("Bad NSEC domain", packet, pos, rdlength);
    }

    bitmap = escape_data(packet, pos, pos+rdlength);
    buffer = strfmt("%s,%s", domain, bitmap);
    FREE(domain);
    FREE(bitmap);

    return buffer;

}

#define DS_DOC "DS DNS SEC record.  RFC 4034\n"\
"format: key_tag,algorithm,digest_type,digest\n"\
"The keytag, algorithm, and digest type are given as base 10.\n"\
"The digest is base64 encoded."

char *
ds(const uint8_t * packet, uint32_t pos, uint32_t id_pos,
   uint16_t rdlength, uint32_t plen)
{
    uint16_t key_tag = (packet[pos] << 8) + packet[pos+1];
    uint8_t alg = packet[pos+2];
    uint8_t dig_type = packet[pos+3];
    char * digest = b64encode(packet,pos+4,rdlength-4);
    char * buffer;

    buffer = strfmt("%d,%d,%d,%s", key_tag, alg, dig_type, digest);
    FREE(digest);

    return buffer;
}


#define NULL_DOC "This data is simply hex escaped. \n"\
"Non printable characters are given as a hex value (\\x30), for example."
char *
escape(const uint8_t * packet, uint32_t pos, uint32_t i,
       uint16_t rdlength, uint32_t plen)
{
    return escape_data(packet, pos, pos + rdlength);
}


rr_parser_container rr_parsers[] =
{
    {1, 1, A, "A", A_DOC, 0},
    {0, 2, domain_name, "NS", D_DOC, 0},
    {0, 5, domain_name, "CNAME", D_DOC, 0},
    {0, 6, soa, "SOA", SOA_DOC, 0},
    {0, 12, domain_name, "PTR", D_DOC, 0},
    {1, 33, srv, "SRV", SRV_DOC, 0},
    {1, 28, AAAA, "AAAA", AAAA_DOC, 0},
    {0, 15, mx, "MX", MX_DOC, 0},
    {0, 46, rrsig, "RRSIG", RRSIG_DOC, 0},
    {0, 16, escape, "TEXT", NULL_DOC, 0},
    {0, 47, nsec, "NSEC", NSEC_DOC, 0},
    {0, 43, ds, "DS", DS_DOC, 0},
    {0, 10, escape, "NULL", NULL_DOC, 0},
    {0, 48, dnskey, "DNSKEY", KEY_DOC, 0},
    {0, 255, escape, "ANY", NULL_DOC, 0}
};

static inline int
count_parsers() {
    return sizeof(rr_parsers)/sizeof(rr_parser_container);
}

/*
 * We occasionally sort the parsers according to how they're used in order
 * to speed up lookups.
 */
void
sort_parsers() {
    int m ,n;
    int change = 1;
    int pcount = count_parsers();
    rr_parser_container tmp;
    for (m = 0; m < pcount - 1 && change == 1; m++)
    {
        change = 0;
        for (n = 0; n < pcount - 1; n++)
        {
            if (rr_parsers[n].count < rr_parsers[n+1].count)
            {
                tmp = rr_parsers[n];
                rr_parsers[n] = rr_parsers[n+1];
                rr_parsers[n+1] = tmp;
                change = 1;
            }
        }
    }
    /* Reset the counts */
    for (m = 0; m < pcount - 1; m++)
    {
        rr_parsers[m].count = 0;
    }
}

unsigned int PACKETS_SEEN = 0;
#define REORDER_LIMIT 100000

/* Find the parser that corresponds to the given cls and rtype. */
rr_parser_container *
find_parser(uint16_t cls, uint16_t rtype)
{
    unsigned int i=0, pcount = count_parsers();
    rr_parser_container * found = NULL;

    /*
     * Re-arrange the order of the parsers according to how often things are
     * seen every REORDER_LIMIT packets.
     */
    if (PACKETS_SEEN > REORDER_LIMIT)
    {
        PACKETS_SEEN = 0;
        sort_parsers();
    }
    PACKETS_SEEN++;

    while (i < pcount && found == NULL)
    {
        rr_parser_container pc = rr_parsers[i];
        if ((pc.rtype == rtype || pc.rtype == 0) &&
            (pc.cls == cls || pc.cls == 0))
        {
            rr_parsers[i].count++;
            found = &rr_parsers[i];
            break;
        }
        i++;
    }

    if (found == NULL) found = &default_rr_parser;

    found->count++;

    return found;
}

void
print_parsers(void)
{
    int i;

    printf("What follows is a list of handled DNS classes and resource \n"
           "record types. \n"
           " - The class # may be listed as 'any', though anything \n"
           "   other than the internet class is rarely seen. \n"
           " - Parsers for records other than those in RFC 1035 should \n"
           "   have their RFC listed. \n"
           " - Unhandled resource records are simply string escaped.\n"
           " - Some resource records share parsers and documentation.\n\n"
           "class, rtype, name: documentation\n");
    for (i=0; i < count_parsers(); i++)
    {
        rr_parser_container cont = rr_parsers[i];
        if (cont.cls == 0) printf("any,");
        else printf("%d,", cont.cls);

        printf(" %d, %s: %s\n\n", cont.rtype, cont.name, cont.doc);
    }
}

void print_parser_usage(void)
{
    int i;
    rr_parser_container pc;

    fprintf(stderr, "parser usage:\n");
    for (i=0; i < count_parsers(); i++)
    {
        pc = rr_parsers[i];
        fprintf(stderr, "  %s - %llu\n", pc.name, pc.count);
    }

    fprintf(stderr, "  undefined parser - %llu\n", default_rr_parser.count);
}
