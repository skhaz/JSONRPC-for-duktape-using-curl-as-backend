#include <curl/curl.h>
#include <duktape.h>
#include <stdio.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class JsonSax : public nlohmann::json_sax<nlohmann::json> {
private:
  duk_context *ctx;

public:
  JsonSax(duk_context *ctx) : ctx(ctx){};

  virtual ~JsonSax() = default;

  virtual bool null() override {
    duk_push_null(ctx);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool boolean(bool val) override {
    duk_push_boolean(ctx, val);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool number_integer(number_integer_t val) override {
    duk_push_number(ctx, val);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool number_unsigned(number_unsigned_t val) override {
    duk_push_number(ctx, val);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool number_float(number_float_t val, const string_t &s) override {
    duk_push_number(ctx, val);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool string(string_t &val) override {
    duk_push_string(ctx, val.c_str());
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool binary(binary_t &val) override {
    return true;
  }

  virtual bool start_object(std::size_t elements) override {
    duk_push_object(ctx);

    return true;
  }

  virtual bool key(string_t &val) override {
    duk_push_string(ctx, val.c_str());

    return true;
  }

  virtual bool end_object() override {
    return true;
  }

  virtual bool start_array(std::size_t elements) override {
    return true;
  }

  virtual bool end_array() override {
    return true;
  }

  virtual bool parse_error(std::size_t position, const std::string &last_token, const nlohmann::detail::exception &ex) override {
    return false;
  }
};

static void
_panic(void *udata, const char *message) {
  (void)udata;

  fprintf(stderr, "*** FATAL ERROR: %s\n", (message ? message : "no message"));
  fflush(stderr);
  abort();
}

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

  request["jsonrpc"] = "2.0";
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

  curl_easy_setopt(curl, CURLOPT_URL, "https://rpc.carimbo.cloud/");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _writefunction);

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  JsonSax handler(ctx);

  return json::sax_parse(stream, &handler) ? 1 : 0;
}

int main() {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, _panic);

  duk_push_c_function(ctx, _native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_object(ctx);
  duk_push_c_function(ctx, _native_jsonrpc, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "invoke");
  duk_put_global_string(ctx, "JSONRPC");

  const auto script = R"(
    try {
      print(JSON.stringify(JSONRPC.invoke('arith_add', 3, 5)))
    } catch (e) {
      print(e)
    }
  )";

  duk_eval_string_noresult(ctx, script);

  duk_destroy_heap(ctx);
  curl_global_cleanup();
  return 0;
}
