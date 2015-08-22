#include <sys/queue.h>



/*
 * Create the headers needed for an HTTP request
 */
static void
evhttp_make_header_request(struct evhttp_connection *evcon,
    struct evhttp_request *req)
{
	char line[1024];
	const char *method;
	
	evhttp_remove_header(req->output_headers, "Proxy-Connection");

	/* Generate request line */
	method = evhttp_method(req->type);
	evutil_snprintf(line, sizeof(line), "%s %s HTTP/%d.%d\r\n",
	    method, req->uri, req->major, req->minor);
	evbuffer_add(evcon->output_buffer, line, strlen(line));

	/* Add the content length on a post request if missing */
	if (req->type == EVHTTP_REQ_POST &&
	    evhttp_find_header(req->output_headers, "Content-Length") == NULL){
		char size[12];
		evutil_snprintf(size, sizeof(size), "%ld",
		    (long)EVBUFFER_LENGTH(req->output_buffer));
		evhttp_add_header(req->output_headers, "Content-Length", size);
	}
}

static int
evhttp_is_connection_close(int flags, struct evkeyvalq* headers)
{
	if (flags & EVHTTP_PROXY_REQUEST) {
		/* proxy connection */
		const char *connection = evhttp_find_header(headers, "Proxy-Connection");
		return (connection == NULL || strcasecmp(connection, "keep-alive") != 0);
	} else {
		const char *connection = evhttp_find_header(headers, "Connection");
		return (connection != NULL && strcasecmp(connection, "close") == 0);
	}
}

static int
evhttp_is_connection_keepalive(struct evkeyvalq* headers)
{
	const char *connection = evhttp_find_header(headers, "Connection");
	return (connection != NULL 
	    && strncasecmp(connection, "keep-alive", 10) == 0);
}

static void
evhttp_maybe_add_date_header(struct evkeyvalq *headers)
{
	if (evhttp_find_header(headers, "Date") == NULL) {
		char date[50];
#ifndef WIN32
		struct tm cur;
#endif
		struct tm *cur_p;
		time_t t = time(NULL);
#ifdef WIN32
		cur_p = gmtime(&t);
#else
		gmtime_r(&t, &cur);
		cur_p = &cur;
#endif
		if (strftime(date, sizeof(date),
			"%a, %d %b %Y %H:%M:%S GMT", cur_p) != 0) {
			evhttp_add_header(headers, "Date", date);
		}
	}
}

static void
evhttp_maybe_add_content_length_header(struct evkeyvalq *headers,
    long content_length)
{
	if (evhttp_find_header(headers, "Transfer-Encoding") == NULL &&
	    evhttp_find_header(headers,	"Content-Length") == NULL) {
		char len[12];
		evutil_snprintf(len, sizeof(len), "%ld", content_length);
		evhttp_add_header(headers, "Content-Length", len);
	}
}

/*
 * Create the headers needed for an HTTP reply
 */

static void
evhttp_make_header_response(struct evhttp_connection *evcon,
    struct evhttp_request *req)
{
	int is_keepalive = evhttp_is_connection_keepalive(req->input_headers);
	char line[1024];
	evutil_snprintf(line, sizeof(line), "HTTP/%d.%d %d %s\r\n",
	    req->major, req->minor, req->response_code,
	    req->response_code_line);
	evbuffer_add(evcon->output_buffer, line, strlen(line));

	if (req->major == 1) {
		if (req->minor == 1)
			evhttp_maybe_add_date_header(req->output_headers);

		/*
		 * if the protocol is 1.0; and the connection was keep-alive
		 * we need to add a keep-alive header, too.
		 */
		if (req->minor == 0 && is_keepalive)
			evhttp_add_header(req->output_headers,
			    "Connection", "keep-alive");

		if (req->minor == 1 || is_keepalive) {
			/* 
			 * we need to add the content length if the
			 * user did not give it, this is required for
			 * persistent connections to work.
			 */
			evhttp_maybe_add_content_length_header(
				req->output_headers,
				(long)EVBUFFER_LENGTH(req->output_buffer));
		}
	}

	/* Potentially add headers for unidentified content. */
	if (EVBUFFER_LENGTH(req->output_buffer)) {
		if (evhttp_find_header(req->output_headers,
			"Content-Type") == NULL) {
			evhttp_add_header(req->output_headers,
			    "Content-Type", "text/html; charset=ISO-8859-1");
		}
	}

	/* if the request asked for a close, we send a close, too */
	if (evhttp_is_connection_close(req->flags, req->input_headers)) {
		evhttp_remove_header(req->output_headers, "Connection");
		if (!(req->flags & EVHTTP_PROXY_REQUEST))
		    evhttp_add_header(req->output_headers, "Connection", "close");
		evhttp_remove_header(req->output_headers, "Proxy-Connection");
	}
}

void
evhttp_make_header(struct evhttp_connection *evcon, struct evhttp_request *req)
{
	char line[1024];
	struct evkeyval *header;

	/*
	 * Depending if this is a HTTP request or response, we might need to
	 * add some new headers or remove existing headers.
	 */
	if (req->kind == EVHTTP_REQUEST) {
		evhttp_make_header_request(evcon, req);
	} else {
		evhttp_make_header_response(evcon, req);
	}

	TAILQ_FOREACH(header, req->output_headers, next) {
		evutil_snprintf(line, sizeof(line), "%s: %s\r\n",
		    header->key, header->value);
		evbuffer_add(evcon->output_buffer, line, strlen(line));
	}
	evbuffer_add(evcon->output_buffer, "\r\n", 2);

	if (EVBUFFER_LENGTH(req->output_buffer) > 0) {
		/*
		 * For a request, we add the POST data, for a reply, this
		 * is the regular data.
		 */
		evbuffer_add_buffer(evcon->output_buffer, req->output_buffer);
	}
}

/* Separated host, port and file from URI */

int
evhttp_hostportfile(char *url, char **phost, u_short *pport, char **pfile)
{
	/* XXX not threadsafe. */
	static char host[1024];
	static char file[1024];
	char *p;
	const char *p2;
	int len;
	u_short port;

	len = strlen(HTTP_PREFIX);
	if (strncasecmp(url, HTTP_PREFIX, len))
		return (-1);

	url += len;

	/* We might overrun */
	if (strlcpy(host, url, sizeof (host)) >= sizeof(host))
		return (-1);

	p = strchr(host, '/');
	if (p != NULL) {
		*p = '\0';
		p2 = p + 1;
	} else
		p2 = NULL;

	if (pfile != NULL) {
		/* Generate request file */
		if (p2 == NULL)
			p2 = "";
		evutil_snprintf(file, sizeof(file), "/%s", p2);
	}

	p = strchr(host, ':');
	if (p != NULL) {
		*p = '\0';
		port = atoi(p + 1);

		if (port == 0)
			return (-1);
	} else
		port = HTTP_DEFAULTPORT;

	if (phost != NULL)
		*phost = host;
	if (pport != NULL)
		*pport = port;
	if (pfile != NULL)
		*pfile = file;

	return (0);
}

/* Parses the status line of a web server */

static int
evhttp_parse_response_line(struct evhttp_request *req, char *line)
{
	char *protocol;
	char *number;
	char *readable;

	protocol = strsep(&line, " ");
	if (line == NULL)
		return (-1);
	number = strsep(&line, " ");
	if (line == NULL)
		return (-1);
	readable = line;

	if (strcmp(protocol, "HTTP/1.0") == 0) {
		req->major = 1;
		req->minor = 0;
	} else if (strcmp(protocol, "HTTP/1.1") == 0) {
		req->major = 1;
		req->minor = 1;
	} else {
		event_debug(("%s: bad protocol \"%s\"",
			__func__, protocol));
		return (-1);
	}

	req->response_code = atoi(number);
	if (!evhttp_valid_response_code(req->response_code)) {
		event_debug(("%s: bad response code \"%s\"",
			__func__, number));
		return (-1);
	}

	if ((req->response_code_line = strdup(readable)) == NULL)
		event_err(1, "%s: strdup", __func__);

	return (0);
}

/* Parse the first line of a HTTP request */

static int
evhttp_parse_request_line(struct evhttp_request *req, char *line)
{
	char *method;
	char *uri;
	char *version;

	/* Parse the request line */
	method = strsep(&line, " ");
	if (line == NULL)
		return (-1);
	uri = strsep(&line, " ");
	if (line == NULL)
		return (-1);
	version = strsep(&line, " ");
	if (line != NULL)
		return (-1);

	/* First line */
	if (strcmp(method, "GET") == 0) {
		req->type = EVHTTP_REQ_GET;
	} else if (strcmp(method, "POST") == 0) {
		req->type = EVHTTP_REQ_POST;
	} else if (strcmp(method, "HEAD") == 0) {
		req->type = EVHTTP_REQ_HEAD;
	} else {
		event_debug(("%s: bad method %s on request %p from %s",
			__func__, method, req, req->remote_host));
		return (-1);
	}

	if (strcmp(version, "HTTP/1.0") == 0) {
		req->major = 1;
		req->minor = 0;
	} else if (strcmp(version, "HTTP/1.1") == 0) {
		req->major = 1;
		req->minor = 1;
	} else {
		event_debug(("%s: bad version %s on request %p from %s",
			__func__, version, req, req->remote_host));
		return (-1);
	}

	if ((req->uri = strdup(uri)) == NULL) {
		event_debug(("%s: evhttp_decode_uri", __func__));
		return (-1);
	}

	/* determine if it's a proxy request */
	if (strlen(req->uri) > 0 && req->uri[0] != '/')
		req->flags |= EVHTTP_PROXY_REQUEST;

	return (0);
}

const char *
evhttp_find_header(const struct evkeyvalq *headers, const char *key)
{
	struct evkeyval *header;

	TAILQ_FOREACH(header, headers, next) {
		if (strcasecmp(header->key, key) == 0)
			return (header->value);
	}

	return (NULL);
}

void
evhttp_clear_headers(struct evkeyvalq *headers)
{
	struct evkeyval *header;

	for (header = TAILQ_FIRST(headers);
	    header != NULL;
	    header = TAILQ_FIRST(headers)) {
		TAILQ_REMOVE(headers, header, next);
		free(header->key);
		free(header->value);
		free(header);
	}
}

/*
 * Returns 0,  if the header was successfully removed.
 * Returns -1, if the header could not be found.
 */

int
evhttp_remove_header(struct evkeyvalq *headers, const char *key)
{
	struct evkeyval *header;

	TAILQ_FOREACH(header, headers, next) {
		if (strcasecmp(header->key, key) == 0)
			break;
	}

	if (header == NULL)
		return (-1);

	/* Free and remove the header that we found */
	TAILQ_REMOVE(headers, header, next);
	free(header->key);
	free(header->value);
	free(header);

	return (0);
}

int
evhttp_add_header(struct evkeyvalq *headers,
    const char *key, const char *value)
{
	struct evkeyval *header = NULL;

	event_debug(("%s: key: %s val: %s\n", __func__, key, value));

	if (strchr(value, '\r') != NULL || strchr(value, '\n') != NULL ||
	    strchr(key, '\r') != NULL || strchr(key, '\n') != NULL) {
		/* drop illegal headers */
		event_debug(("%s: dropping illegal header\n", __func__));
		return (-1);
	}

	header = calloc(1, sizeof(struct evkeyval));
	if (header == NULL) {
		event_warn("%s: calloc", __func__);
		return (-1);
	}
	if ((header->key = strdup(key)) == NULL) {
		free(header);
		event_warn("%s: strdup", __func__);
		return (-1);
	}
	if ((header->value = strdup(value)) == NULL) {
		free(header->key);
		free(header);
		event_warn("%s: strdup", __func__);
		return (-1);
	}

	TAILQ_INSERT_TAIL(headers, header, next);

	return (0);
}

/*
 * Parses header lines from a request or a response into the specified
 * request object given an event buffer.
 *
 * Returns
 *   DATA_CORRUPTED      on error
 *   MORE_DATA_EXPECTED  when we need to read more headers
 *   ALL_DATA_READ       when all headers have been read.
 */

enum message_read_status
evhttp_parse_firstline(struct evhttp_request *req, struct evbuffer *buffer)
{
	char *line;
	enum message_read_status status = ALL_DATA_READ;

	line = evbuffer_readline(buffer);
	if (line == NULL)
		return (MORE_DATA_EXPECTED);

	switch (req->kind) {
	case EVHTTP_REQUEST:
		if (evhttp_parse_request_line(req, line) == -1)
			status = DATA_CORRUPTED;
		break;
	case EVHTTP_RESPONSE:
		if (evhttp_parse_response_line(req, line) == -1)
			status = DATA_CORRUPTED;
		break;
	default:
		status = DATA_CORRUPTED;
	}

	free(line);
	return (status);
}

static int
evhttp_append_to_last_header(struct evkeyvalq *headers, const char *line)
{
	struct evkeyval *header = TAILQ_LAST(headers, evkeyvalq);
	char *newval;
	size_t old_len, line_len;

	if (header == NULL)
		return (-1);

	old_len = strlen(header->value);
	line_len = strlen(line);

	newval = realloc(header->value, old_len + line_len + 1);
	if (newval == NULL)
		return (-1);

	memcpy(newval + old_len, line, line_len + 1);
	header->value = newval;

	return (0);
}

enum message_read_status
evhttp_parse_headers(struct evhttp_request *req, struct evbuffer* buffer)
{
	char *line;
	enum message_read_status status = MORE_DATA_EXPECTED;

	struct evkeyvalq* headers = req->input_headers;
	while ((line = evbuffer_readline(buffer))
	       != NULL) {
		char *skey, *svalue;

		if (*line == '\0') { /* Last header - Done */
			status = ALL_DATA_READ;
			free(line);
			break;
		}

		/* Check if this is a continuation line */
		if (*line == ' ' || *line == '\t') {
			if (evhttp_append_to_last_header(headers, line) == -1)
				goto error;
			continue;
		}

		/* Processing of header lines */
		svalue = line;
		skey = strsep(&svalue, ":");
		if (svalue == NULL)
			goto error;

		svalue += strspn(svalue, " ");

		if (evhttp_add_header(headers, skey, svalue) == -1)
			goto error;

		free(line);
	}

	return (status);

 error:
	free(line);
	return (DATA_CORRUPTED);
}

static int
evhttp_get_body_length(struct evhttp_request *req)
{
	struct evkeyvalq *headers = req->input_headers;
	const char *content_length;
	const char *connection;

	content_length = evhttp_find_header(headers, "Content-Length");
	connection = evhttp_find_header(headers, "Connection");
		
	if (content_length == NULL && connection == NULL)
		req->ntoread = -1;
	else if (content_length == NULL &&
	    strcasecmp(connection, "Close") != 0) {
		/* Bad combination, we don't know when it will end */
		event_warnx("%s: we got no content length, but the "
		    "server wants to keep the connection open: %s.",
		    __func__, connection);
		return (-1);
	} else if (content_length == NULL) {
		req->ntoread = -1;
	} else {
		char *endp;
		ev_int64_t ntoread = evutil_strtoll(content_length, &endp, 10);
		if (*content_length == '\0' || *endp != '\0' || ntoread < 0) {
			event_debug(("%s: illegal content length: %s",
				__func__, content_length));
			return (-1);
		}
		req->ntoread = ntoread;
	}
		
	event_debug(("%s: bytes to read: %lld (in buffer %ld)\n",
		__func__, req->ntoread,
		EVBUFFER_LENGTH(req->evcon->input_buffer)));

	return (0);
}

/*
static char *SEP[256];
memset(SEP, 0, 256);
SEP[0] = 1;
SEF[':'] = 1;
SEP[' '] = 1;
SEP['\t'] = 1;
SEP['\r'] = 1;
SEP['\n'] = 1;

static char *NL[256];
memset(NL, 0, 256);
NL[0] = 1;
NL['\r'] = 1;
NL['\n'] = 1;

static char *SPACE[256];
memset(SPACE, 0, 256);
SPACE[0] = 1;
SPACE[' '] = 1;
SPACE['\t'] = 1;
*/
#define PREALLOC_HEADERS 10

int
sm_parse_headers(char *buf, size_t buflen; struct evkeyvalq *headers) {
    char *r, w, fieldstart = buf;
    char *end = buf + buflen;
    // pre-allocate some header structs
    struct ekeyval *kv = kv_first = calloc(PREALLOC_HEADERS, sizeof(struct ekeyval));
    int newlines = 0;
    while (r < end) {
        fieldstart = r;
        while(r < end && !SEP[*r])
            r++;
        if (r < end) {
            // terminate header
            *r = 0;
            kv->key = fieldstart;
            r++;
        }
        while (r < end && SEP[*r])
            r++;
        
        // value
        fieldstart = w = r;
        newlines = 0;
        while(r < end) {
            if(!NL[*r])
                if(w > fieldstart) {
                    *w = *r;
                    w++;
                }
                r++;
            else {
                do {
                    newlines++;
                    r++;
                } while(r < end && NL[*r] && newlines <= 4)
                if(newlines == 4) {
                    // headers done
                    if(w == fieldstart)
                        buf[r-newlines] = 0;
                    else
                        buf[w] = 0;
                    kv->val = fieldstart;
                    return (int)(r - buf);
                } else if(r < end && SPACE[*r] && w == fieldstart)
                    // continuation: remove
                    w = r - newlines;
            }
        }

        TAILQ_INSERT_TAIL(evkeyvalq, kv, next);
        kv++;
        if(kv < kv_first || kv >= kv_first + PREALLOC_HEADERS)
            kv = calloc(1, sizeof(struct ekeyval));
        else 
            kv++;
    }
    return -1;
}

static char *testreq = "GET    /    HTTP   /   1  .   1
Accept:  text/plain   ;  q    = 0
         .1
        ,,,,
         ,,
      ,,,  ,,,
      ,,,  ,,,
      ,,,,,,,,
        ,,,,
         ,,
                    text/*  ; q =
         1                        . 00        
Range: bytes                    =
         8 
         -
Host: 
       foo.example.com
       
Accept:                , */* ; q = 0 ,\"\"\"

\"\"\"Support for vcard qualifiers:
    split ; to dict, attach to each value (order!)
    namespace?
    dc.subject.first -> subject.: [ ].namespace
    insert at pos func
    
    general:
        list 
            splitre = re.compile(r'(?=\\),'), replace '\,' with ','
        kvpair
            splitre = re.compile(r'(?=\\);'), replace '\;' with ','
        kv 
            splitre = re.compile(r'(?=\\)='), replace '\=' with ','
    registry of special parsers similar to twisted web:
        time: ISO, HTTP
        currency
        percent
        float
        qvals for Accept
        mimetypes
        vcard address (; list with order semantics)
";

    
