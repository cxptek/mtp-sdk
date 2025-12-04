require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "TpSdk"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/cxptek/mtp-sdk.git", :tag => "#{s.version}" }

  s.source_files = [
    "ios/**/*.{m,mm}",
    "ios/**/*.{h}",
    "cpp/**/*.{h,hpp,cpp}",
  ]

  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => "$(PODS_TARGET_SRCROOT)/.. $(PODS_TARGET_SRCROOT)/../nitrogen/generated/shared/c++ $(PODS_TARGET_SRCROOT)/../cpp",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++20"
  }

  s.dependency 'React-jsi'
  s.dependency 'React-callinvoker'

  load 'nitrogen/generated/ios/TpSdk+autolinking.rb'
  add_nitrogen_files(s)

  install_modules_dependencies(s)
end
