// #include <mimalloc-new-delete.h>
// #include <mimalloc-override.h>

#include <curl/curl.h>
#include <duktape.h>
#include <stdio.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

static void
_panic(void *udata, const char *message) {
  (void)udata;

  fprintf(stderr, "*** FATAL ERROR: %s\n", (message ? message : "no message"));
  fflush(stderr);
  abort();
}

// size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string *buffer)
// {
//   buffer->append(static_cast<char *>(ptr), size * nmemb);
//   return size * nmemb;
// }

static duk_ret_t _native_print(duk_context *ctx) {
  printf("%s\n", duk_to_string(ctx, 0));
  return 0;
}

static size_t _writefunction(void *buffer, size_t size, size_t count, std::string *stream) {
  auto s_size = size * count;
  stream->append((char *)buffer, 0, s_size);
  return s_size;
}

static duk_ret_t _native_jsonrpc(duk_context *ctx) {
  json request, params = json::array();
  std::string stream;

  const auto id = nullptr;
  const auto method = duk_get_string(ctx, 0);
  const auto argc = duk_get_top(ctx);
  for (auto i = 1; i < argc; i++) {
    const auto type = duk_get_type(ctx, i);

    switch (type) {
    case DUK_TYPE_BOOLEAN:
      params.emplace_back(duk_get_boolean(ctx, i));
      break;
    case DUK_TYPE_NUMBER:
      params.emplace_back(duk_get_number(ctx, i));
      break;
    case DUK_TYPE_STRING:
      params.emplace_back(duk_get_string(ctx, i));
      break;
    case DUK_TYPE_NULL:
      params.emplace_back(nullptr);
      break;
    }
  }

  request["method"] = method;
  request["params"] = params;
  request["id"] = id;
  duk_pop(ctx);

  const auto curl = curl_easy_init();

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charset: utf-8");

  const auto payload = request.dump();

  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:9000/rpc");
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
  curl_easy_setopt(
      curl, CURLOPT_WRITEFUNCTION, +[](void *buffer, size_t size, size_t count, std::string *stream) -> size_t {
        auto s_size = size * count;
        stream->append((char *)buffer, 0, s_size);
        return s_size;
      });

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  duk_push_string(ctx, stream.c_str());

  return 1;
}

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, _panic);

  duk_push_c_function(ctx, _native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_object(ctx);
  duk_push_c_function(ctx, _native_jsonrpc, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "call");
  duk_put_global_string(ctx, "JSONRPC");

  duk_eval_string_noresult(ctx, "try { print('JSONRPC: ' + JSONRPC.call('Greet', 3, 9)) } catch(e) { print('Error: ' + e) }");

  // duk_eval_string_noresult(ctx, "try { print('JSONRPC: ' + JSONRPC.call('SetData')) } catch(e) { print('Error: ' + e) }");

  duk_destroy_heap(ctx);

  curl_global_cleanup();

  return 0;
}
