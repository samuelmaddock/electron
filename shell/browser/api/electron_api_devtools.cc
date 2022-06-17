// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_devtools.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "gin/dictionary.h"
#include "gin/object_template_builder.h"
#include "shell/browser/api/electron_api_debugger.h"
#include "shell/browser/cookie_change_notifier.h"
#include "shell/browser/electron_browser_context.h"
#include "shell/browser/javascript_environment.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

#include "content/public/browser/devtools_agent_host.h"

namespace gin {

template <>
struct Converter<content::DevToolsAgentHost*> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   content::DevToolsAgentHost* host) {
    gin_helper::Dictionary dict = gin::Dictionary::CreateEmpty(isolate);
    dict.Set("source", "todo");
    dict.Set("targetId", host->GetId());
    dict.Set("targetType", host->GetType());
    dict.Set("attached", host->IsAttached());
    dict.Set("url", host->GetURL());
    dict.Set("name", host->GetTitle());
    dict.Set("faviconUrl", host->GetFaviconURL());
    dict.Set("description", host->GetDescription());
    return dict.GetHandle();
  }
};

}  // namespace gin

namespace electron {

namespace api {

gin::WrapperInfo DevTools::kWrapperInfo = {gin::kEmbedderNativeGin};

DevTools::DevTools(v8::Isolate* isolate) {}

DevTools::~DevTools() = default;

std::vector<content::DevToolsAgentHost*> DevTools::GetAllTargets(
    v8::Isolate* isolate) {
  content::DevToolsAgentHost::List targets =
      content::DevToolsAgentHost::GetOrCreateAll();

  std::vector<content::DevToolsAgentHost*> hosts;
  for (const scoped_refptr<content::DevToolsAgentHost>& host : targets) {
    hosts.push_back(host.get());
  }

  return hosts;
}

v8::Local<v8::Value> DevTools::GetDebuggerByTargetID(
    v8::Isolate* isolate,
    const std::string& target_id) {
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetForId(target_id);
  // TODO: validate
  auto handle = electron::api::Debugger::Create(isolate, agent_host.get());
  return v8::Local<v8::Value>::New(isolate, handle.ToV8());
}

// static
gin::Handle<DevTools> DevTools::Create(v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, new DevTools(isolate));
}

gin::ObjectTemplateBuilder DevTools::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin_helper::EventEmitterMixin<DevTools>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getAllTargets", &DevTools::GetAllTargets)
      .SetMethod("getDebuggerByTargetId", &DevTools::GetDebuggerByTargetID);
}

const char* DevTools::GetTypeName() {
  return "DevTools";
}

}  // namespace api

}  // namespace electron

namespace {

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin::Dictionary dict(isolate, exports);
  dict.Set("devTools", electron::api::DevTools::Create(isolate));
}

}  // namespace

NODE_LINKED_MODULE_CONTEXT_AWARE(electron_browser_devtools, Initialize)
