#include <mimalloc-override.h>
#include <mimalloc-new-delete.h>

#include <stdio.h>
#include <curl/curl.h>
#include <duktape.h>

#include <string>

size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string *buffer)
{
  buffer->append(static_cast<char *>(ptr), size * nmemb);
  return size * nmemb;
}

static duk_ret_t native_print(duk_context *ctx)
{
  printf("%s\n", duk_to_string(ctx, 0));
  return 0;
}

static duk_ret_t native_fetch(duk_context *ctx)
{
  CURL *curl;
  CURLcode res;
  std::string buffer;

  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, duk_to_string(ctx, 0));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK)
  {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    goto error;
  }

  duk_push_string(ctx, buffer.c_str());

  curl_easy_cleanup(curl);

  return 1;

error:
  return 0;
}

int main()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap_default();

  duk_push_c_function(ctx, native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");
  duk_eval_string_noresult(ctx, "print('hello world');");

  duk_push_c_function(ctx, native_fetch, DUK_VARARGS);
  duk_put_global_string(ctx, "fetch");
  duk_eval_string_noresult(ctx, "print(fetch('https://httpbin.org/get'));");

  duk_destroy_heap(ctx);

  curl_global_cleanup();

  /*
duk_context *ctx = duk_create_heap_default();
duk_eval_string(ctx, "1+2");
printf("1+2=%d\n", (int)duk_get_int(ctx, -1));
duk_destroy_heap(ctx);

CURL *curl;
CURLcode res;

curl_global_init(CURL_GLOBAL_DEFAULT);

curl = curl_easy_init();

if (curl == NULL)
{
  fprintf(stderr, "curl_easy_init() failed\n");
  return 1;
}

curl_easy_setopt(curl, CURLOPT_URL, "https://httpbin.org/get");

FILE *file = fopen("httpbin.get", "wb");
curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

res = curl_easy_perform(curl);

if (res != CURLE_OK)
  fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

curl_easy_cleanup(curl);

curl_global_cleanup();
*/
  return 0;
}
