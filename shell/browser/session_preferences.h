// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_SESSION_PREFERENCES_H_
#define ELECTRON_SHELL_BROWSER_SESSION_PREFERENCES_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/supports_user_data.h"

namespace content {
class BrowserContext;
}

namespace electron {

struct PreloadScript {
  enum class ScriptType { kWebFrame, kServiceWorker };

  base::FilePath file_path;
  ScriptType script_type;
};

class SessionPreferences : public base::SupportsUserData::Data {
 public:
  static SessionPreferences* FromBrowserContext(
      content::BrowserContext* context);
  static std::vector<base::FilePath> GetValidPreloads(
      content::BrowserContext* context);

  static void CreateForBrowserContext(content::BrowserContext* context);

  ~SessionPreferences() override;

  void set_preloads(const std::vector<base::FilePath>& preloads) {
    preloads_ = preloads;
  }
  const std::vector<base::FilePath>& preloads() const { return preloads_; }

  void set_preload_scripts(const std::vector<PreloadScript>& preload_scripts) {
    preload_scripts_ = preload_scripts;
  }
  const std::vector<PreloadScript>& preload_scripts() const {
    return preload_scripts_;
  }

 private:
  SessionPreferences();

  // The user data key.
  static int kLocatorKey;

  std::vector<base::FilePath> preloads_;
  std::vector<PreloadScript> preload_scripts_;
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_SESSION_PREFERENCES_H_
