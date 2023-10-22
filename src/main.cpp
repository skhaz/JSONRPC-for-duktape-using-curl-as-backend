#include <stdio.h>
#include <curl/curl.h>
#include <duktape.h>

#include <mimalloc.h>
// #include <mimalloc-override.h>
// #include <mimalloc-new-delete.h>

int main()
{
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

  return 0;
}
