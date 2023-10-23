#include <mimalloc-new-delete.h>
#include <mimalloc-override.h>

#include <curl/curl.h>
#include <duktape.h>
#include <stdio.h>

#include <string>

static void _panic(void *udata, const char *msg) {
  (void)udata;

  fprintf(stderr, "*** FATAL ERROR: %s\n", (msg ? msg : "no message"));
  fflush(stderr);
  abort();
}

// size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string *buffer)
// {
//   buffer->append(static_cast<char *>(ptr), size * nmemb);
//   return size * nmemb;
// }

static duk_ret_t native_print(duk_context *ctx) {
  printf("%s\n", duk_to_string(ctx, 0));
  return 0;
}

static duk_ret_t _native_fetch(duk_context *ctx) {
  CURL *curl;
  CURLcode res;
  std::string buffer;
  int ret = 1;

  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, duk_to_string(ctx, 0));
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void *ptr, size_t size, size_t nmemb, std::string *buffer) -> size_t {
    buffer->append(static_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
  });

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    ret = 0;
    goto error;
  }

  duk_push_string(ctx, buffer.c_str());

error:
  curl_easy_cleanup(curl);
  return ret;
}

static duk_ret_t _native_call(duk_context *ctx) {

  duk_push_string(ctx, strdup("hello world"));

  return 1;
}

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, _panic);

  duk_push_c_function(ctx, native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_object(ctx);
  duk_push_c_function(ctx, _native_call, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "call");
  duk_put_global_string(ctx, "JSONRPC");

  duk_eval_string_noresult(ctx, "try { print('JSONRPC: ' + JSONRPC.call({result: true})) } catch(e) { print('Error: ' + e) }");

  duk_destroy_heap(ctx);

  curl_global_cleanup();

  return 0;
}
