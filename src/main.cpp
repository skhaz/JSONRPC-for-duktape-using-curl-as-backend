#include <curl/curl.h>
#include <duktape.h>
#include <stdio.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

static void marshal(duk_context *ctx, json &p, duk_idx_t idx = -1) {
  duk_require_stack(ctx, 4);

  const auto type = duk_get_type(ctx, idx);
  const auto key = duk_get_string(ctx, idx);
  switch (type) {
  case DUK_TYPE_NULL:
    p = nullptr;
    break;
  case DUK_TYPE_BOOLEAN:
    p = static_cast<bool>(duk_get_boolean(ctx, idx));
    break;
  case DUK_TYPE_NUMBER:
    p = static_cast<double>(duk_get_number(ctx, idx));
    break;
  case DUK_TYPE_STRING:
    p = static_cast<const char *>(duk_get_string(ctx, idx));
    break;
  case DUK_TYPE_OBJECT:
    if (duk_is_array(ctx, idx)) {
      const auto lenght = duk_get_length(ctx, idx);

      for (duk_uarridx_t i = 0; i < lenght; i++) {
        json element;
        duk_get_prop_index(ctx, idx, static_cast<duk_uarridx_t>(i));
        marshal(ctx, element, idx + 1);
        p.push_back(element);
        duk_pop(ctx);
      }
    } else if (duk_is_object(ctx, idx)) {
      // duk_enum(ctx, idx, DUK_ENUM_OWN_PROPERTIES_ONLY);
      duk_pop(ctx);
      // const auto lenght = duk_get_length(ctx, idx);
      // std::cout << "lenght: " << lenght << std::endl;
      // for (duk_uarridx_t i = 0; i < lenght; i++) {
      //   std::cout << "loop" << std::endl;
      //   duk_pop(ctx);
      // }
      // std::cout << "enum" << std::endl;
      // while (duk_next(ctx, idx, true)) {
      //   std::cout << "loop" << std::endl;
      //   json key, value;

      //   duk_insert(ctx, idx - 2);
      //   marshal(ctx, key, idx);
      //   marshal(ctx, value, idx);
      //   p[key] = value;
      //   duk_pop_2(ctx);
      // }
    }
    break;
  }
}

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
    duk_push_int(ctx, val);
    duk_put_prop(ctx, -3);

    return true;
  }

  virtual bool number_unsigned(number_unsigned_t val) override {
    duk_push_uint(ctx, val);
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
    std::cout << "key: " << val << std::endl;
    duk_push_string(ctx, val.c_str());

    return true;
  }

  virtual bool end_object() override {
    return true;
  }

  virtual bool start_array(std::size_t elements) override {
    duk_push_array(ctx);
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
    json p;
    marshal(ctx, p, i);
    params.emplace_back(p);
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

  std::cout << "Payload: " << payload << std::endl;

  curl_easy_setopt(curl, CURLOPT_URL, "https://rpc.carimbo.cloud/");
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _writefunction);

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  std::cout << "Response: " << stream << std::endl;

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
      const result = JSONRPC.invoke('arith_add', 1, 2, [1, 2, 3, [4, 5, 6], "string", true, false, null, { "key": "value" }])
      print('Result ' + JSON.stringify(result))

      //const result = JSONRPC.invoke('helper_sample', [])
      //print(JSON.stringify(result))
    } catch (error) {
      print('Error: ' + error)
    }
  )";

  duk_eval_string_noresult(ctx, script);

  duk_destroy_heap(ctx);
  curl_global_cleanup();
  return 0;
}
