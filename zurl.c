#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <zlib.h>

#define ZURL    "http://httpbin.org/"
#define ZOUT    "out.z"
#define ZCHUNK  16384

struct stream {
  char *fn;                     /* output filename */
  FILE *f;                      /* output file handle */
  z_stream strm;                /* zlib stream */
  unsigned char *buf;           /* zlib input buffer */
  size_t size;                  /* zlib input buffer size */
  unsigned char zbuf[ZCHUNK];   /* zlib output buffer */
  int error;                    /* error flag (0=ok, 1=error) */
};

static struct stream *stream_new(const char *filename);
static int stream_open(struct stream *s, int level);
static int stream_close(struct stream *s);
static void stream_del(struct stream *s);
static size_t stream_deflate(struct stream *s, int flush);
static void stream_consume(struct stream *s, const char *buf, size_t size);
static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *opq);

int
main(void)
{
  struct stream *s;
  CURL *curl;
  CURLcode res;

  s = stream_new(ZOUT);
  stream_open(s, Z_BEST_COMPRESSION);

  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, ZURL);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) s);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    fprintf(stderr, "error: %s\n", curl_easy_strerror(res));

  stream_close(s);
  if (s->error)
    fprintf(stderr, "error: stream is invalid\n");

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  stream_del(s);

  return 0;
}

static struct stream *
stream_new(const char *filename)
{
  struct stream *s = malloc(sizeof(*s));
  s->fn = strdup(filename);
  s->f = NULL;
  s->buf = NULL;
  s->error = 0;
  return s;
}

static int
stream_open(struct stream *s, int level)
{
  s->f = fopen(s->fn, "wb");
  if (!s->f) goto err;
  if (deflateInit(&s->strm, level) != Z_OK) goto err;
  return 0;
err:
  s->error = 1;
  return 1;
}

static int
stream_close(struct stream *s)
{
  stream_deflate(s, Z_FINISH);
  if (deflateEnd(&s->strm) != Z_OK) goto err;
  if (fclose(s->f) != 0) goto err;
  return 0;
err:
  s->error = 1;
  return 1;
}

static void
stream_del(struct stream *s)
{
  free(s->fn);
  free(s->buf);
  free(s);
}

static size_t
stream_deflate(struct stream *s, int flush)
{
  int ret;
  unsigned int have;
  s->strm.avail_in = s->size;
  s->strm.next_in = s->buf;
  do {
    s->strm.avail_out = ZCHUNK;
    s->strm.next_out = s->zbuf;
    ret = deflate(&s->strm, flush);
    if (ret == Z_STREAM_ERROR) goto err;
    have = ZCHUNK - s->strm.avail_out;
    fwrite(s->zbuf, 1, have, s->f);
  } while (s->strm.avail_out == 0);
  if (s->strm.avail_in != 0) goto err;
  return s->size;
err:
  s->error = 1;
  return 0;
}

static void
stream_consume(struct stream *s, const char *buf, size_t size)
{
  s->size = size;
  s->buf = realloc(s->buf, size);
  memcpy(s->buf, buf, size);
}

static size_t
write_cb(char *ptr, size_t size, size_t nmemb, void *opq)
{
  size_t len = size * nmemb;
  struct stream *s = (struct stream *) opq;
  if (s->buf) stream_deflate(s, Z_NO_FLUSH);
  stream_consume(s, ptr, len);
  return len;
}
