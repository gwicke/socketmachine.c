#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PREALLOC_HEADERS    10
// NULL : \s \t
static const char SEP[256] = {
	1, 0, 0, 0, 0, 0, 0, 0,   0, 1, 1, 0, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 1, 0, 0, 0, 0, 0,
	/* 64 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};
static const char NL[256] = {
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 1, 0, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 64 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};
static const char SPACE[256] = {
	0, 0, 0, 0, 0, 0, 0, 0,   0, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 64 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 128 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	/* 192 */
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0,
};
int
sm_parse_headers(char *buf, int buflen, void *kvq) {
    char *r = buf;
    char *w = buf;
    char *fieldstart = buf;
    char *end = buf + buflen;
    // pre-allocate some header structs
    /*struct ekeyval *kv = kv_first = calloc(PREALLOC_HEADERS, sizeof(struct ekeyval)); */
    int newlines = 0;
    while (r < end) {
        fieldstart = w = r;
        while(r < end && !SEP[*r])
            r++;
        if (r < end) {
            // terminate header
            if(*r == ':') {
                *r = 0;
                //kv->key = fieldstart;
                printf("H|%s\n", fieldstart);
                r++;
                while (r < end && SEP[*r])
                    r++;
                // start for value
                fieldstart = w = r;
            } else {
                printf("H|\n");
                r++;
                while (r < end && SEP[*r])
                    r++;
            }
        }

        /* parse the value part */
        while(r < end) {
            if(!NL[*r]) {
                r++;
            } else {
                newlines = 0;
                do {
                    newlines++;
                    r++;
                } while(r < end && NL[*r] && newlines <= 4);
                if(newlines == 4 || (*(r-newlines) == '\n' && newlines >= 2)) {
                    // headers done
                    *(r-newlines) = 0;
                    //kv->val = fieldstart;
                    printf("  V|%s\n", fieldstart);
                    printf("Done: %d\n", (int)(r - buf));
                    return (int)(r - buf);
                } else if(r < end) {
                    if( !SPACE[*r] ) {
                        // new header starts
                        //kv->val = fieldstart;
                        *(r-newlines) = 0;
                        printf("  V|%s\n", fieldstart);
                        break;
                    } else {
                        // continuation: use move mode
                        w = r - newlines;
                        while(r < end) {
                            if(!NL[*r]) {
                                *w = *r;
                                w++;
                                r++;
                            } else {
                                newlines = 0;
                                do {
                                    newlines++;
                                    r++;
                                } while(r < end && NL[*r] && newlines <= 4);
                                if(newlines == 4 || (*(r-newlines) == '\n' && newlines >= 2)) {
                                    // headers done
                                    *w = 0;
                                    //kv->val = fieldstart;
                                    printf("  V|%s\n", fieldstart);
                                    return (int)(r - buf);
                                } else if(r < end && !SPACE[*r] ) {
                                    // new header starts
                                    //kv->val = fieldstart;
                                    *w = 0;
                                    printf("  V|%s\n", fieldstart);
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        /*TAILQ_INSERT_TAIL(evkeyvalq, kv, next);
          kv++;
          if(kv < kv_first || kv >= kv_first + PREALLOC_HEADERS)
          kv = calloc(1, sizeof(struct ekeyval));
          else 
          kv++;*/
    }
    return -1;
}


char*teststring = "From cyrus@runner Fri Dec 21 16:25:17 2007\r\n"
"Return-Path: <cyrus@runner>\r\n"
"Received: from runner ([unix socket]) (authenticated user=gw_gabrielwicke_de bits=0)\r\n"
"        by runner (Cyrus v2.1.18-IPv6-Debian-2.1.18-2.0.1) with LMTP; Fri, 21 Dec 2007 16:25:17 +0000\r\n"
"X-Sieve: CMU Sieve 2.2\r\n"
"Return-Path: <root@netzquadrat.de>\r\n"
"X-Spam-Checker-Version: SpamAssassin 3.1.7-deb (2006-10-05) on \r\n"
"        runner.wikidev.net\r\n"
"X-Spam-Level: =?utf8?B?w7bDpA==?= \r\n"
"X-Spam-Status: No, score=1.0 required=6.0 tests=AWL,BAYES_99 autolearn=no \r\n"
"        version=3.1.7-deb\r\n"
"Received: from pitr01.farm02.netzquadrat.de (pitr01.farm02.netzquadrat.de [217.10.79.47])\r\n"
"        by runner.wikidev.net (Postfix) with ESMTP id 3B52B78074E4\r\n"
"        for <gabriel@gwicke.net>; Fri, 21 Dec 2007 16:25:08 +0000 (UTC)\r\n"
"Received: from voicemail01.de.sipgate.net ([217.10.79.35])\r\n"
"        by pitr01.farm02.netzquadrat.de with esmtp (Exim 4.50)\r\n"
"        id 1J5kgG-0000Mn-Fu\r\n"
"        for gabriel@gwicke.net; Fri, 21 Dec 2007 17:25:00 +0100\r\n"
"Received: from root by voicemail01.de.sipgate.net with local (Exim 4.63)\r\n"
"        (envelope-from <root@voicemail01.de.sipgate.net>)\r\n"
"        id 1J5kgG-0000VD-BU\r\n"
"        for gabriel@gwicke.net; Fri, 21 Dec 2007 17:25:00 +0100\r\n"
"DomainKey-Signature: s=dkim; d=paypal.de; c=nofws; q=dns;\r\n"
"    h=Received:Date:Message-Id:Subject:X-MaxCode-Template:\r\n"
"      To:From:X-Email-Type-Id:X-XPT-XSL-Name:\r\n"
"      Content-Transfer-Encoding:Content-Type:MIME-Version;\r\n"
"    b=769hbqkgu2EXdgnLUxpulK3fw4RD2W35qi4dWVMKFnljIARUj+WE7\r\n"
"      OibcZ5trQGT9cJfIKLbavIU8WGwzO1MHzx/s8JlRt8aH8XytHGK5b\r\n"
"      nhHdv6PfIMsLiebsafH6OlDThXIjwExuUx+t2FiRmCS1G1+TJ/YWt\r\n"
"      QvtPj8GvdSlY=;\r\n"
"Date: Fri, 21 Dec 2007 17:25:00 +0100\r\n"
"Ugly\r\n"
"lines\r\n"
"From: Sipgate Voicemailö <voicemail@sipgate.de>\r\n"
"To: =?iso-8859-1?q?Gabriel_Wicke?= <gabriel@gwicke.net>\r\n"
"Subject: sipgate Voicemail: Sie haben eine neue Nachricht\r\n"
"Message-ID: <Asterisk-0-1872088086-5555258-30996@voicemail01>\r\n"
"MIME-Version: 1.0\r\n"
"Content-Type: multipart/mixed; boundary=\"voicemail_05555258309961712046645\"\r\n"
"\r\n"
"Bla\r\n";
char*teststring2 = "BEGIN:VCARD\r\n"
"VERSION:3.0\r\n"
"N:Hulsman;Bernard;;;\r\n"
"FN:Bernard Hulsman\r\n"
"ADR;TYPE=HOME,POSTAL,PARCEL:;;Marcusestraat 13;Arnhem;;6836;Netherlands\r\n"
"TEL;TYPE=HOME,CELL:+31638337144\r\n"
"TEL;TYPE=HOME,VOICE:0031263270467\r\n"
"TEL;TYPE=HOME,VOICE:sip:0031507115598@sipgate.de\r\n"
"ORG:Wikiation.nl\r\n"
"EMAIL;TYPE=INTERNET,HOME,PREF:bernard@BernardHulsman.nl\r\n"
"CLASS:PUBLIC\r\n"
"UID:bd540a13f5c43265afdb4e806d435744\r\n"
"REV:2008-10-02T12:48:24Z\r\n"
"PRODID:Contacts 0.8.3\r\n"
"END:VCARD\r\n";

int main() {
char *testreq = "GET    /    HTTP   /   1  .   1\r\n"
"Accept:  text/plain   ;  q    = 0\r\n"
"         .1\r\n"
"        ,,,,\r\n"
"         ,,\r\n"
"      ,,,  ,,,\r\n"
"      ,,,  ,,,\r\n"
"      ,,,,,,,,\r\n"
"        ,,,,\r\n"
"         ,,\r\n"
"                    text/*  ; q =\r\n"
"         1                        . 00        \r\n"
"Range: bytes                    =\r\n"
"         8 \r\n"
"         -\r\n"
"Host: \r\n"
"       foo.example.com\r\n"
"       \r\n"
"Accept:                , */* ; q = 0 ,\"\"\"\r\n"
"\r\n";
    char *test2 = malloc(strlen(teststring)+1);
    memcpy(test2, teststring, strlen(teststring));
    test2[strlen(teststring)] = 0;
    sm_parse_headers(test2, strlen(teststring), NULL);
    test2 = malloc(strlen(teststring2)+1);
    memcpy(test2, teststring2, strlen(teststring2));
    test2[strlen(teststring2)] = 0;
    sm_parse_headers(test2, strlen(teststring2), NULL);
    char *tests = malloc(strlen(testreq)+1);
    memcpy(tests, testreq, strlen(testreq));
    tests[strlen(testreq)] = 0;
    int res = sm_parse_headers(tests, strlen(testreq), NULL);
    printf("res in main: %d\n", res);
    return(res);
}
        
        
