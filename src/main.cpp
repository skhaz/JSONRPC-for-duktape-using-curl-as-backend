#include <mimalloc-override.h>
#include <mimalloc-new-delete.h>

#include <stdio.h>
#include <curl/curl.h>
#include <duktape.h>

#include <string>

static void my_panic(void *udata, const char *msg)
{
  (void)udata;

  fprintf(stderr, "*** FATAL ERROR: %s\n", (msg ? msg : "no message"));
  fflush(stderr);
  abort();
}

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

class JSONRPCWrapper
{
public:
  char *call()
  {
    return strdup("ok");
  }
};

JSONRPCWrapper jsonrpc;

duk_ret_t call(duk_context *ctx)
{
  duk_push_this(ctx);
  duk_get_prop_string(ctx, -1, "\xff"
                               "\xff"
                               "JSONRPC");

  JSONRPCWrapper *x = static_cast<JSONRPCWrapper *>(duk_to_pointer(ctx, -1));
  if (x)
  {
    char *result = x->call();
    duk_push_string(ctx, result);
    return 1;
  }

  duk_pop(ctx);

  return 0;
}

duk_ret_t jsonrpc_destructor(duk_context *ctx)
{
  return 0;
}

duk_ret_t jsonrpc_constructor(duk_context *ctx)
{
  if (!duk_is_constructor_call(ctx))
  {
    return DUK_RET_TYPE_ERROR;
  }

  duk_push_this(ctx);
  duk_push_pointer(ctx, &jsonrpc);
  duk_put_prop_string(ctx, -2, "\xff"
                               "\xff"
                               "JSONRPC");

  duk_push_c_function(ctx, jsonrpc_destructor, 1);
  duk_set_finalizer(ctx, -2);

  return 0;
}

int main()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, my_panic);

  duk_push_c_function(ctx, jsonrpc_constructor, 0);
  duk_push_bare_object(ctx);
  duk_push_c_function(ctx, call, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "call");
  duk_put_prop_string(ctx, -2, "prototype");
  duk_put_global_string(ctx, "JSONRPC");

  duk_push_c_function(ctx, native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_c_function(ctx, native_fetch, DUK_VARARGS);
  duk_put_global_string(ctx, "fetch");

  duk_eval_string_noresult(ctx, "var rpc = new JSONRPC; print('JSONRPC result: ' + rpc.call())");

  duk_destroy_heap(ctx);

  curl_global_cleanup();

  return 0;
}
