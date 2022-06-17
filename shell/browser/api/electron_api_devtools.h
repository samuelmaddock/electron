// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_API_ELECTRON_API_DEVTOOLS_H_
#define ELECTRON_SHELL_BROWSER_API_ELECTRON_API_DEVTOOLS_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "gin/handle.h"
#include "shell/browser/event_emitter_mixin.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/gin_helper/trackable_object.h"

namespace base {
class DictionaryValue;
}

namespace gin_helper {
class Dictionary;
}

namespace content {
class DevToolsAgentHost;
}

namespace electron {

class ElectronBrowserContext;

namespace api {

class DevTools : public gin::Wrappable<DevTools>,
                 public gin_helper::EventEmitterMixin<DevTools> {
 public:
  static gin::Handle<DevTools> Create(v8::Isolate* isolate);

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // disable copy
  DevTools(const DevTools&) = delete;
  DevTools& operator=(const DevTools&) = delete;

 protected:
  DevTools(v8::Isolate* isolate);
  ~DevTools() override;

  std::vector<content::DevToolsAgentHost*> GetAllTargets(v8::Isolate* isolate);
  v8::Local<v8::Value> GetDebuggerByTargetID(v8::Isolate* isolate,
                                             const std::string& target_id);
};

}  // namespace api

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_DEVTOOLS_H_
