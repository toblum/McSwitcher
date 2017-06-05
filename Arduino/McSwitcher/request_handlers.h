server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(200, "text/plain", String(ESP.getFreeHeap()));
});

server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

server.addHandler(new SPIFFSEditor(AP_NAME, AP_PASS));

server.onNotFound([](AsyncWebServerRequest *request){
  DEBUG_PRINT("NOT_FOUND: ");
  if(request->method() == HTTP_GET)
    DEBUG_PRINT("GET");
  else if(request->method() == HTTP_POST)
    DEBUG_PRINT("POST");
  else if(request->method() == HTTP_DELETE)
    DEBUG_PRINT("DELETE");
  else if(request->method() == HTTP_PUT)
    DEBUG_PRINT("PUT");
  else if(request->method() == HTTP_PATCH)
    DEBUG_PRINT("PATCH");
  else if(request->method() == HTTP_HEAD)
    DEBUG_PRINT("HEAD");
  else if(request->method() == HTTP_OPTIONS)
    DEBUG_PRINT("OPTIONS");
  else
    DEBUG_PRINT("UNKNOWN");
  DEBUG_PRINTF3(" http://%s%s\n", request->host().c_str(), request->url().c_str());

  if(request->contentLength()){
    DEBUG_PRINTF2("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    DEBUG_PRINTF2("_CONTENT_LENGTH: %u\n", request->contentLength());
  }

  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++){
    AsyncWebHeader* h = request->getHeader(i);
    DEBUG_PRINTF3("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }

  int params = request->params();
  for(i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){
      DEBUG_PRINTF4("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      DEBUG_PRINTF3("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      DEBUG_PRINTF3("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }

  request->send(404);
});

server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index)
    DEBUG_PRINTF2("UploadStart: %s\n", filename.c_str());
  DEBUG_PRINTF2("%s", (const char*)data);
  if(final)
    DEBUG_PRINTF3("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
});

server.begin();
