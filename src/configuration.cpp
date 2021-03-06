/*
 * configuration.cpp
 * CraftOS-PC 2
 *
 * This file implements functions for interacting with the configuration.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <fstream>
#include <unordered_map>
#include <configuration.hpp>
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"

struct configuration config;
int onboardingMode = 0;

#ifdef __EMSCRIPTEN__
extern "C" {extern void syncfs(); }
#endif

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {"", true, false, false};
    std::ifstream in(getBasePath() + WS("/config/") + to_path_t(id) + WS(".json"));
    if (!in.is_open()) return cfg;
    if (in.peek() == std::ifstream::traits_type::eof()) { in.close(); return cfg; } // treat an empty file as if it didn't exist in the first place
    Value root;
    Poco::JSON::Object::Ptr p;
    try { p = root.parse(in); } catch (Poco::JSON::JSONException &e) {
        cfg.loadFailure = true;
        const std::string message = "An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.";
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str(), NULL);
        else if (selectedRenderer == 3) RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else if (selectedRenderer == 4) TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else printf("%s\n", message.c_str());
        in.close();
        return cfg;
    }
    in.close();
    cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label")) {
        if (root.isMember("base64")) cfg.label = b64decode(root["label"].asString());
        else cfg.label = std::string(root["label"].asString());
    }
    if (root.isMember("startFullscreen")) cfg.startFullscreen = root["startFullscreen"].asBool();
    return cfg;
}

void setComputerConfig(int id, const computer_configuration& cfg) {
    if (cfg.loadFailure) return;
    Value root;
    if (!cfg.label.empty()) root["label"] = b64encode(cfg.label);
    root["isColor"] = cfg.isColor;
    root["base64"] = true;
    root["startFullscreen"] = cfg.startFullscreen;
    std::ofstream out(getBasePath() + WS("/config/") + to_path_t(id) + WS(".json"));
    out << root;
    out.close();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void* {syncfs(); return NULL; }, NULL, true);
#endif
}

#define readConfigSetting(name, type) if (root.isMember(#name)) config.name = root[#name].as##type()

bool configLoadError = false;

// first: 0 = immediate, 1 = reboot, 2 = relaunch
// second: 0 = boolean, 1 = number, 2 = string
std::unordered_map<std::string, std::pair<int, int> > configSettings = {
    {"http_enable", {1, 0}},
    {"debug_enable", {1, 0}},
    {"mount_mode", {0, 1}},
    {"disable_lua51_features", {1, 0}},
    {"default_computer_settings", {1, 2}},
    {"logErrors", {0, 0}},
    {"showFPS", {0, 0}},
    {"computerSpaceLimit", {0, 1}},
    {"maximumFilesOpen", {0, 1}},
    {"abortTimeout", {0, 1}},
    {"maxNotesPerTick", {2, 1}},
    {"clockSpeed", {0, 1}},
    {"ignoreHotkeys", {0, 0}},
    {"checkUpdates", {2, 0}},
    {"romReadOnly", {2, 0}},
    {"useHDFont", {2, 0}},
    {"configReadOnly", {0, 0}},
    {"vanilla", {1, 0}},
    {"initialComputer", {2, 1}},
    {"maxRecordingTime", {0, 1}},
    {"recordingFPS", {0, 1}},
    {"showMountPrompt", {0, 0}},
    {"maxOpenPorts", {0, 1}},
    {"mouse_move_throttle", {0, 1}},
    {"monitorsUseMouseEvents", {0, 0}},
    {"defaultWidth", {2, 1}},
    {"defaultHeight", {2, 1}},
    {"standardsMode", {0, 0}},
    {"isColor", {0, 0}},
    {"startFullscreen", {2, 0}},
    {"useHardwareRenderer", {2, 0}},
    {"preferredHardwareDriver", {2, 2}},
    {"useVsync", {2, 0}},
    {"http_websocket_enabled", {1, 0}},
    {"http_max_websockets", {0, 1}},
    {"http_max_websocket_message", {1, 0}},
    {"http_max_requests", {0, 1}},
    {"http_max_upload", {0, 1}},
    {"http_max_download", {0, 1}},
    {"http_timeout", {0, 1}}
};

const std::string hiddenOptions[] = {"customFontPath", "customFontScale", "customCharScale", "skipUpdate", "lastVersion"};

std::unordered_map<std::string, Poco::Dynamic::Var> unknownOptions;

void config_init() {
    createDirectory(getBasePath() + WS("/config"));
    config = {
        true,
        false,
        MOUNT_MODE_RO_STRICT,
        {"*"},
        {
            "127.0.0.0/8",
            "10.0.0.0/8",
            "172.16.0.0/12",
            "192.168.0.0/16",
            "fd00::/8"
        },
        {
            "/Users",
            "/home"
        },
        {
            "/"
        },
        {},
        false,
        "",
        false,
        false,
        1000000,
        128,
        17000,
        8,
        20,
        false,
        true,
        true,
        "",
        0,
        0,
        "",
        false,
        false,
        0,
        15,
        10,
        0,
        true,
        128,
        -1,
        false,
        51,
        19,
        false,
        false,
        "",
        false,
        false,
        false,
        {},
        true,
        4,
        65536,
        16,
        4194304,
        16777216,
        30000
    };
    std::ifstream in(getBasePath() + WS("/config/global.json"));
    if (!in.is_open()) { onboardingMode = 1; return; }
    Value root;
    Poco::JSON::Object::Ptr p;
    try {
        p = root.parse(in);
    } catch (Poco::JSON::JSONException &e) {
        configLoadError = true;
        const std::string message = "An error occurred while parsing the global configuration file: " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.";
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str(), NULL);
        else if (selectedRenderer == 3) RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else if (selectedRenderer == 4) TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else printf("%s\n", message.c_str());
        in.close();
        return;
    }
    in.close();
    readConfigSetting(http_enable, Bool);
    readConfigSetting(debug_enable, Bool);
    readConfigSetting(mount_mode, Int);
    readConfigSetting(disable_lua51_features, Bool);
    readConfigSetting(default_computer_settings, String);
    readConfigSetting(logErrors, Bool);
    readConfigSetting(showFPS, Bool);
    readConfigSetting(computerSpaceLimit, Int);
    readConfigSetting(maximumFilesOpen, Int);
    readConfigSetting(abortTimeout, Int);
    readConfigSetting(maxNotesPerTick, Int);
    readConfigSetting(clockSpeed, Int);
    readConfigSetting(ignoreHotkeys, Bool);
    readConfigSetting(checkUpdates, Bool);
    readConfigSetting(romReadOnly, Bool);
    readConfigSetting(customFontPath, String);
    readConfigSetting(customFontScale, Int);
    readConfigSetting(customCharScale, Int);
    readConfigSetting(skipUpdate, String);
    readConfigSetting(configReadOnly, Bool);
    readConfigSetting(vanilla, Bool);
    readConfigSetting(initialComputer, Int);
    readConfigSetting(maxRecordingTime, Int);
    readConfigSetting(recordingFPS, Int);
    readConfigSetting(cliControlKeyMode, Int);
    readConfigSetting(showMountPrompt, Bool);
    readConfigSetting(maxOpenPorts, Int);
    readConfigSetting(mouse_move_throttle, Int);
    readConfigSetting(monitorsUseMouseEvents, Bool);
    readConfigSetting(defaultWidth, Int);
    readConfigSetting(defaultHeight, Int);
    readConfigSetting(standardsMode, Bool);
    readConfigSetting(useHardwareRenderer, Bool);
    readConfigSetting(preferredHardwareDriver, String);
    readConfigSetting(useVsync, Bool);
    readConfigSetting(serverMode, Bool);
    readConfigSetting(http_websocket_enabled, Bool);
    readConfigSetting(http_max_websockets, Int);
    readConfigSetting(http_max_websocket_message, Int);
    readConfigSetting(http_max_requests, Int);
    readConfigSetting(http_max_upload, Int);
    readConfigSetting(http_max_download, Int);
    readConfigSetting(http_timeout, Int);
    if (root.isMember("pluginData")) for (const auto& e : root["pluginData"]) config.pluginData[e.first] = e.second.extract<std::string>();
    // for JIT: substr until the position of the first '-' in CRAFTOSPC_VERSION (todo: find a static way to determine this)
    if (onboardingMode == 0 && (!root.isMember("lastVersion") || root["lastVersion"].asString().substr(0, sizeof(CRAFTOSPC_VERSION) - 1) != CRAFTOSPC_VERSION)) { onboardingMode = 2; config_save(); }
    for (const auto& e : root)
        if (configSettings.find(e.first) == configSettings.end() && std::find(hiddenOptions, hiddenOptions + (sizeof(hiddenOptions) / sizeof(std::string)), e.first) == hiddenOptions + (sizeof(hiddenOptions) / sizeof(std::string)))
            unknownOptions.insert(e);
}

void config_save() {
    if (configLoadError) return;
    Value root;
    root["http_enable"] = config.http_enable;
    root["debug_enable"] = config.debug_enable;
    root["mount_mode"] = config.mount_mode;
    root["disable_lua51_features"] = config.disable_lua51_features;
    root["default_computer_settings"] = config.default_computer_settings;
    root["logErrors"] = config.logErrors;
    root["showFPS"] = config.showFPS;
    root["computerSpaceLimit"] = config.computerSpaceLimit;
    root["maximumFilesOpen"] = config.maximumFilesOpen;
    root["abortTimeout"] = config.abortTimeout;
    root["maxNotesPerTick"] = config.maxNotesPerTick;
    root["clockSpeed"] = config.clockSpeed;
    root["ignoreHotkeys"] = config.ignoreHotkeys;
    root["checkUpdates"] = config.checkUpdates;
    root["romReadOnly"] = config.romReadOnly;
    root["customFontPath"] = config.customFontPath;
    root["customFontScale"] = config.customFontScale;
    root["customCharScale"] = config.customCharScale;
    root["skipUpdate"] = config.skipUpdate;
    root["configReadOnly"] = config.configReadOnly;
    root["vanilla"] = config.vanilla;
    root["initialComputer"] = config.initialComputer;
    root["maxRecordingTime"] = config.maxRecordingTime;
    root["recordingFPS"] = config.recordingFPS;
    root["cliControlKeyMode"] = config.cliControlKeyMode;
    root["showMountPrompt"] = config.showMountPrompt;
    root["maxOpenPorts"] = config.maxOpenPorts;
    root["mouse_move_throttle"] = config.mouse_move_throttle;
    root["monitorsUseMouseEvents"] = config.monitorsUseMouseEvents;
    root["defaultWidth"] = config.defaultWidth;
    root["defaultHeight"] = config.defaultHeight;
    root["standardsMode"] = config.standardsMode;
    root["useHardwareRenderer"] = config.useHardwareRenderer;
    root["preferredHardwareDriver"] = config.preferredHardwareDriver;
    root["useVsync"] = config.useVsync;
    root["serverMode"] = config.serverMode;
    root["http_websocket_enabled"] = config.http_websocket_enabled;
    root["http_max_websockets"] = config.http_max_websockets;
    root["http_max_websocket_message"] = config.http_max_websocket_message;
    root["http_max_requests"] = config.http_max_requests;
    root["http_max_upload"] = config.http_max_upload;
    root["http_max_download"] = config.http_max_download;
    root["http_timeout"] = config.http_timeout;
    root["lastVersion"] = CRAFTOSPC_VERSION;
    root["pluginData"] = Value();
    for (const auto& e : config.pluginData) root["pluginData"][e.first] = e.second;
    for (const auto& opt : unknownOptions) root[opt.first] = opt.second;
    std::ofstream out(getBasePath() + WS("/config/global.json"));
    out << root;
    out.close();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void* {syncfs(); return NULL; }, NULL, true);
#endif
}