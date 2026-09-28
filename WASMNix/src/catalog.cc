#include "nix/catalog.h"

static const WasmNixApi kApis[] = {
    {"WSL", "wsl", "List"},
    {"WSL", "wsl", "ListOnline"},
    {"WSL", "wsl", "ListRunning"},
    {"WSL", "wsl", "ListQuiet"},
    {"WSL", "wsl", "Install"},
    {"WSL", "wsl", "SetDefault"},
    {"WSL", "wsl", "SetDefaultUser"},
    {"WSL", "wsl", "SetVersion"},
    {"WSL", "wsl", "Export"},
    {"WSL", "wsl", "Import"},
    {"WSL", "wsl", "ImportInPlace"},
    {"WSL", "wsl", "Unregister"},
    {"WSL", "wsl", "Terminate"},
    {"WSL", "wsl", "Shutdown"},
    {"WSL", "wsl", "Status"},
    {"WSL", "wsl", "Version"},
    {"WSL", "wsl", "Update"},
    {"WSL", "wsl", "Mount"},
    {"WSL", "wsl", "Unmount"},
    {"WSL", "wsl", "Config"},
    {"WSL", "wsl", "GetConfiguration"},
    {"WSL", "wsl", "Configure"},
    {"WSL", "wsl", "Manage"},
    {"WSL", "wsl", "Info"},
    {"WSL", "wsl", "Var"},
    {"WSL", "wsl", "Path"},
    {"WSL", "wsl", "Read"},
    {"WSL", "wsl", "Write"},
    {"WSL", "wsl", "Exec"},
    {"WSL", "wsl", "Launch"},
    {"WSL", "wsl", "LaunchWin32"},
    {"WSL", "wsl", "IsDistributionRegistered"},
    {"WSL", "wsl", "Help"},
};

const WasmNixApi* wasmnix_catalog(int* count) {
  if (count) *count = (int)(sizeof(kApis) / sizeof(kApis[0]));
  return kApis;
}
