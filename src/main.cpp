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
  int ret = 1;

  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, duk_to_string(ctx, 0));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

  res = curl_easy_perform(curl);

  if (res != CURLE_OK)
  {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    ret = 0;
    goto error;
  }

  duk_push_string(ctx, buffer.c_str());

error:
  curl_easy_cleanup(curl);
  return ret;
}

/*
var req = new XMLHttpRequest();
req.responseType = 'json';
req.open('GET', url, true);
req.onload  = function() {
   var jsonResponse = req.response;
   // do something with jsonResponse
};
req.send(null);
*/

int main()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap_default();

  duk_push_c_function(ctx, native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_c_function(ctx, native_fetch, DUK_VARARGS);
  duk_put_global_string(ctx, "fetch");
  duk_eval_string_noresult(ctx, "print(fetch('https://httpbin.org/get'));");

  duk_destroy_heap(ctx);

  curl_global_cleanup();

  return 0;
}
