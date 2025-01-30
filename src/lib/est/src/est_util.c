/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <unistd.h>

#include "est_util.h"
#include "execssl.h"
#include "log.h"
#include "osp_unit.h"
#include "util.h"

#define PKIM_UTIL_PKCS7_TMP "/tmp/pkim_pkcs7.tmp"

/*
 * Decode a base64-encoded PKCS7 certificate provided by the EST server and
 * convert it to PEM format
 *
 * This function returns an allocated string representing the PEM certificate
 * or NULL on error.
 */
char *est_util_pkcs7_to_pem(const char *pkcs7)
{
    char *retval = NULL;
    char *bdec = NULL;
    char *pem = NULL;

    char spkcs7[strlen(pkcs7) + 1];

    /*
     * Remove "\n\r" from the input string -- the reason for this is that
     * "openssl base64" expects either a base64 encoded string with "\n\r"
     * (without the -A parameter) or a single line string (with the -A parameter)
     * Since the EST server can send either, remove the "\n\r" and treat it
     * as a single line base64 encoded string.
     */
    STRSCPY(spkcs7, pkcs7);
    strstrip(spkcs7, "\r\n");

    /*
     * The PKCS7 certificate we receive from the EST server is in binary DER
     * format encoded with base64. Since the execssl() functions are not
     * designed to handle binary data, we must use a temporary file. This is
     * acceptable since the certificate doesn't contain any sentivie data.
     */
    bdec = execssl(spkcs7, "base64", "-A", "-d", "-out", PKIM_UTIL_PKCS7_TMP);
    if (bdec == NULL)
    {
        LOG(ERR, "Error base64 decoding PKCS7 certificate.");
        goto error;
    }

    /* Convert PKCS7 to PEM */
    pem = execssl(NULL, "pkcs7", "-in", PKIM_UTIL_PKCS7_TMP, "-inform", "DER", "-print_certs");
    if (pem == NULL)
    {
        LOG(ERR, "Error converting PKCS7 certificate.");
        goto error;
    }

    /*
     * openssl pkcs7 prints certificte information before dumping the PEM
     * certificate, try to find the PEM signature.
     */
    char *pem_hdr = strstr(pem, "-----BEGIN CERTIFICATE-----");
    if (pem_hdr == NULL)
    {
        LOG(ERR, "Unable to find the PEM header.");
        goto error;
    }

    retval = STRDUP(pem_hdr);

error:
    if (bdec != NULL) FREE(bdec);
    if (pem != NULL) FREE(pem);
    (void)unlink(PKIM_UTIL_PKCS7_TMP);

    return retval;
}

#define STR_APPEND(b, e, str)               \
    do                                      \
    {                                       \
        char *cs;                           \
        cs = MEM_APPEND(b, e, strlen(str)); \
        memcpy(cs, (str), strlen(str));     \
        strcpy(cs, (str));                  \
    } while (false)

char *est_util_csr_subject(void)
{
    char buf[1024];
    char *send;

    char *subj = NULL;
    char *subj_e = NULL;

    if (osp_unit_id_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/commonName=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_serial_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/serialNumber=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_model_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/title=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_sku_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/surname=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_hw_revision_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/supportedApplicationContext=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_manufacturer_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/localityName=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_vendor_name_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/organizationName=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_vendor_part_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/givenName=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    if (osp_unit_factory_get(buf, sizeof(buf)))
    {
        STR_APPEND(&subj, &subj_e, "/stateOrProvinceName=");
        STR_APPEND(&subj, &subj_e, buf);
    }

    /*
     * STR_APPEND() doesn't pad the string with \0, so we need to add it here.
     * Additionally, add the subject ending '/'
     */
    send = MEM_APPEND(&subj, &subj_e, sizeof("/"));
    memcpy(send, "/", sizeof("/"));

    return subj;
}
