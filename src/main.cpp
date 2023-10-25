#include <curl/curl.h>
#include <duktape.h>
#include <stdio.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class UnmarshalSax : public nlohmann::json_sax<nlohmann::json> {
private:
  duk_context *ctx;

public:
  UnmarshalSax(duk_context *ctx) : ctx(ctx){};

  virtual ~UnmarshalSax() = default;

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
    std::cout << "start_array" << std::endl;
    return true;
  }

  virtual bool end_array() override {
    return true;
  }

  virtual bool parse_error(std::size_t position, const std::string &last_token, const nlohmann::detail::exception &ex) override {
    return false;
  }
};

/*
#define DUK_TYPE_NONE
#define DUK_TYPE_UNDEFINED 1U
#define DUK_TYPE_NULL 2U
#define DUK_TYPE_BOOLEAN 3U
#define DUK_TYPE_NUMBER 4U
#define DUK_TYPE_STRING 5U
#define DUK_TYPE_OBJECT 6U

*/
static void marshal(duk_context *ctx, json &p) {
  duk_require_stack(ctx, 4);

  const auto type = duk_get_type(ctx, -1);
  const auto key = duk_get_string(ctx, -2);
  switch (type) {
  case DUK_TYPE_NULL:
    p.emplace_back(nullptr);
    break;
  case DUK_TYPE_BOOLEAN:
    p.emplace_back(duk_get_boolean(ctx, -1));
    break;
  case DUK_TYPE_NUMBER:
    p.emplace_back(duk_get_number(ctx, -1));
    break;
  case DUK_TYPE_STRING:
    p.emplace_back(duk_get_string(ctx, -1));
    break;
  case DUK_TYPE_OBJECT:
    if (duk_is_array(ctx, -1)) {
      const auto length = duk_get_length(ctx, -1);
      auto arr = json::array();
      for (auto j = 0; j < length; j++) {
        duk_get_prop_index(ctx, -1, j);
        arr.emplace_back(duk_get_number(ctx, -1));
      }

      p[key] = arr;
    } else {
      json object;
      marshal(ctx, object);
      p[key] = object;
    }
    break;
  }
}

/*
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
  case DUK_TYPE_OBJECT:
    if (duk_is_array(ctx, i)) {
      const auto length = duk_get_length(ctx, i);
      auto arr = json::array();

      for (auto j = 0; j < length; j++) {
        duk_get_prop_index(ctx, i, j);
        arr.emplace_back(duk_get_number(ctx, -1));
        duk_pop(ctx);
      }

      params.emplace_back(arr);
      break;
    }
  }
}*/

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
  marshal(ctx, params);

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

  std::cout << "Payload: " << payload << std::endl;

  curl_easy_setopt(curl, CURLOPT_URL, "https://rpc.carimbo.cloud/");
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _writefunction);

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  UnmarshalSax sax(ctx);

  return json::sax_parse(stream, &sax) ? 1 : 0;
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
      print(JSON.stringify(JSONRPC.invoke('arith_scalar', [1, 2, 3], 2)))
    } catch (error) {
      print('Error: ' + error)
    }
  )";

  duk_eval_string_noresult(ctx, script);

  duk_destroy_heap(ctx);
  curl_global_cleanup();
  return 0;
}
