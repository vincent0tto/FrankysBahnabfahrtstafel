#include "core/https_json.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

#include "certs.h"
#include "config.h"
#include "core/log.h"

const char* fetchResultName(FetchResult r) {
  switch (r) {
    case FetchResult::Ok: return "ok";
    case FetchResult::NetworkError: return "network error";
    case FetchResult::HttpError: return "http error";
    case FetchResult::ParseError: return "parse error";
  }
  return "?";
}

FetchResult httpsGetJson(const char* url, JsonDocument& filter, JsonDocument& doc, const char* tag) {
  WiFiClientSecure client;
  client.setCACert(VRN_ROOT_CA);
  client.setHandshakeTimeout(HTTP_TIMEOUT_MS / 1000);
  // WiFiClientSecure::setTimeout() (also called by HTTPClient) only sets the
  // socket timeout; Stream's read timeout used by the JSON parser stays at
  // 1 s, so a short server pause ended in "IncompleteInput".
  static_cast<Stream&>(client).setTimeout(HTTP_TIMEOUT_MS);

  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding -> stream straight into the parser
  http.setReuse(false);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent("FrankysBahnabfahrtstafel/1.0 (ESP32)");

  // the log names the URL without its query
  int urlLen = (int)strcspn(url, "?");
  if (!http.begin(client, url)) return FetchResult::NetworkError;
  int code = http.GET();
  if (code <= 0) {
    char err[96] = "";
    client.lastError(err, sizeof(err));
    Log.printf("[%s] %.*s: %s (tls: %s)\n", tag, urlLen, url, http.errorToString(code).c_str(), err);
    http.end();
    return FetchResult::NetworkError;
  }
  if (code != HTTP_CODE_OK) {
    Log.printf("[%s] %.*s: HTTP %d\n", tag, urlLen, url, code);
    http.end();
    return FetchResult::HttpError;
  }

  // The nesting limit counts the filtered-out parts too; ESPN's team logos
  // (events[].competitions[].competitors[].team.logos[].rel[]) are 11 deep,
  // one more than ArduinoJson's default of 10.
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter),
                                             DeserializationOption::NestingLimit(16));
  // The server closes the socket right after the body; stop TLS ourselves so
  // http.end() doesn't try to drain a dead connection (logs mbedTLS -76).
  client.stop();
  http.end();
  if (err) {
    Log.printf("[%s] %.*s: JSON %s\n", tag, urlLen, url, err.c_str());
    return FetchResult::ParseError;
  }
  return FetchResult::Ok;
}
